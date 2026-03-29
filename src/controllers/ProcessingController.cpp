/**
 * @file ProcessingController.cpp
 * @brief 处理控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/FileController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/models/MessageModel.h"
#include "EnhanceVision/services/ImageExportService.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include "EnhanceVision/core/TaskCoordinator.h"
#include "EnhanceVision/core/ResourceManager.h"
#include "EnhanceVision/core/AIEnginePool.h"
#include "EnhanceVision/core/VideoProcessor.h"
#include "EnhanceVision/core/video/VideoProcessorFactory.h"
#include <QUuid>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <QtConcurrent>
#include <algorithm>

namespace EnhanceVision {

namespace {
constexpr qint64 kPerfLogThresholdMs = 8;

}

ProcessingController::ProcessingController(QObject* parent)
    : QObject(parent)
    , m_processingModel(new ProcessingModel(this))
    , m_queueStatus(QueueStatus::Running)
    , m_threadPool(new QThreadPool(this))
    , m_currentProcessingCount(0)
    , m_taskCounter(0)
    , m_fileController(nullptr)
    , m_messageModel(nullptr)
    , m_sessionController(nullptr)
    , m_taskCoordinator(TaskCoordinator::instance())
    , m_resourceManager(ResourceManager::instance())
    , m_resourcePressure(0)
{
    // 线性任务队列：一次只处理一个任务
    m_threadPool->setMaxThreadCount(1);
    m_threadPool->setThreadPriority(QThread::LowPriority);
    
    connect(m_taskCoordinator, &TaskCoordinator::taskOrphaned,
            this, &ProcessingController::onTaskOrphaned);
    
    connect(m_resourceManager, &ResourceManager::resourcePressureChanged,
            this, &ProcessingController::onResourcePressureChanged);
    
    connect(m_resourceManager, &ResourceManager::resourceExhausted,
            this, &ProcessingController::resourceExhausted);
    
    m_resourceManager->startMonitoring(2000);

    m_sessionSyncTimer = new QTimer(this);
    m_sessionSyncTimer->setSingleShot(true);
    m_sessionSyncTimer->setInterval(2500);  // 增大防抖间隔，减少同步频率
    connect(m_sessionSyncTimer, &QTimer::timeout, this, [this]() {
        if (m_sessionController) {
            m_sessionController->syncCurrentMessagesToSession();
        }
    });

    m_memorySyncTimer = new QTimer(this);
    m_memorySyncTimer->setSingleShot(true);
    m_memorySyncTimer->setInterval(100);
    connect(m_memorySyncTimer, &QTimer::timeout, this, &ProcessingController::flushSessionMemorySync);

    // 初始化 AI 推理引擎和模型注册表
    m_modelRegistry = new ModelRegistry(this);
    m_aiEngine = new AIEngine(this);
    m_aiEnginePool = new AIEnginePool(2, m_modelRegistry, this);
    
    // 初始化统一进度管理器
    m_progressManager = std::make_unique<ProgressManager>(this);

    QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
    if (!QDir(modelsPath).exists()) {
        modelsPath = QCoreApplication::applicationDirPath() + "/../resources/models";
    }
    if (!QDir(modelsPath).exists()) {
        modelsPath = QCoreApplication::applicationDirPath() + "/resources/models";
    }
    if (!QDir(modelsPath).exists()) {
        modelsPath = QDir(QCoreApplication::applicationDirPath()).filePath("../../resources/models");
    }
    m_modelRegistry->initialize(modelsPath);
    m_aiEngine->setModelRegistry(m_modelRegistry);

    // AI 推理信号不再在构造函数中静态连接
    // 改为在 startTask() 中通过 connectAiEngineForTask() 为每个 AI 任务动态连接
}

ProcessingController::~ProcessingController()
{
    cancelAllTasks();
    if (m_aiEngine) {
        m_aiEngine->cancelProcess();
    }
    m_threadPool->waitForDone();
}

ProcessingModel* ProcessingController::processingModel() const
{
    return m_processingModel;
}

QueueStatus ProcessingController::queueStatus() const
{
    return m_queueStatus;
}

ModelRegistry* ProcessingController::modelRegistry() const
{
    return m_modelRegistry;
}

AIEngine* ProcessingController::aiEngine() const
{
    return m_aiEngine;
}

int ProcessingController::queueSize() const
{
    return m_tasks.size();
}

int ProcessingController::currentProcessingCount() const
{
    return m_currentProcessingCount;
}

void ProcessingController::pauseQueue()
{
    if (m_queueStatus != QueueStatus::Paused) {
        m_queueStatus = QueueStatus::Paused;
        emit queueStatusChanged();
        emit queuePaused();
    }
}

void ProcessingController::resumeQueue()
{
    if (m_queueStatus != QueueStatus::Running) {
        m_queueStatus = QueueStatus::Running;
        emit queueStatusChanged();
        emit queueResumed();
        processNextTask();
    }
}

void ProcessingController::setMessageModel(MessageModel* messageModel)
{
    m_messageModel = messageModel;
}

void ProcessingController::setSessionController(SessionController* sessionController)
{
    m_sessionController = sessionController;
}

void ProcessingController::cancelTask(const QString& taskId)
{
    // 支持按 taskId 或 messageId 取消
    bool isMessageId = false;
    for (const auto& task : m_tasks) {
        if (task.messageId == taskId) {
            isMessageId = true;
            break;
        }
    }

    if (isMessageId) {
        for (int i = m_tasks.size() - 1; i >= 0; --i) {
            if (m_tasks[i].messageId != taskId) {
                continue;
            }

            const QString id = m_tasks[i].taskId;
            if (m_tasks[i].status == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(id);
                emit taskCancelled(id);
                m_tasks.removeAt(i);
            } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                gracefulCancel(id);
            }
        }

        updateQueuePositions();
        syncModelTasks();
        emit queueSizeChanged();

        if (m_messageModel) {
            m_messageModel->updateStatus(taskId, static_cast<int>(ProcessingStatus::Cancelled));
        }

        processNextTask();
        return;
    }

    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId != taskId) {
            continue;
        }

        const QString messageId = m_tasks[i].messageId;

        if (m_tasks[i].status == ProcessingStatus::Pending) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            m_tasks.removeAt(i);
            updateQueuePositions();
            emit taskCancelled(taskId);
            emit queueSizeChanged();
        } else if (m_tasks[i].status == ProcessingStatus::Processing) {
            gracefulCancel(taskId);
        }

        syncModelTasks();
        syncMessageProgress(messageId);
        syncMessageStatus(messageId);
        processNextTask();
        break;
    }
}

void ProcessingController::cancelAllTasks()
{
    int cancelledProcessingCount = 0;

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QString taskId = m_tasks[i].taskId;
        const ProcessingStatus oldStatus = m_tasks[i].status;

        if (oldStatus == ProcessingStatus::Pending) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            emit taskCancelled(taskId);
            m_tasks.removeAt(i);
            continue;
        }

        if (oldStatus == ProcessingStatus::Processing) {
            // 直接取消并移除，而不是调用 gracefulCancel（它不会移除任务）
            m_taskCoordinator->requestCancellation(taskId);
            
            // 取消 AI 引擎处理
            if (m_taskMessages.contains(taskId)) {
                const Message& msg = m_taskMessages[taskId];
                if (msg.mode == ProcessingMode::AIInference) {
                    AIEngine* poolEngine = m_aiEnginePool->engineForTask(taskId);
                    if (poolEngine) {
                        poolEngine->cancelProcess();
                    }
                }
            }

            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            emit taskCancelled(taskId);
            m_tasks.removeAt(i);
            cancelledProcessingCount++;
        }
    }

    // 修正处理计数器
    if (cancelledProcessingCount > 0 && m_currentProcessingCount > 0) {
        m_currentProcessingCount = qMax(0, m_currentProcessingCount - cancelledProcessingCount);
        emit currentProcessingCountChanged();
    }

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
}

void ProcessingController::forceCancelAllTasks()
{
    qWarning() << "[ProcessingController] Force cancelling all tasks"
               << "currentTaskCount:" << m_tasks.size()
               << "processingCount:" << m_currentProcessingCount;

    // 强制取消 AI 推理引擎池中所有活动引擎
    // 遍历所有任务，逐个取消正在处理的 AI 引擎
    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Processing && m_taskMessages.contains(task.taskId)) {
            const Message& msg = m_taskMessages[task.taskId];
            if (msg.mode == ProcessingMode::AIInference) {
                AIEngine* poolEngine = m_aiEnginePool->engineForTask(task.taskId);
                if (poolEngine) {
                    poolEngine->cancelProcess();
                }
            }
        }
    }
    if (m_aiEngine) {
        m_aiEngine->forceCancel();
    }

    // 清空线程池队列
    if (m_threadPool) {
        m_threadPool->clear();
    }

    // 取消所有任务并正确清理
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QString taskId = m_tasks[i].taskId;
        const ProcessingStatus oldStatus = m_tasks[i].status;

        m_tasks[i].status = ProcessingStatus::Cancelled;
        cleanupTask(taskId);
        emit taskCancelled(taskId);
    }
    m_tasks.clear();

    // 重置处理计数器 - 这是关键修复
    const int oldCount = m_currentProcessingCount;
    m_currentProcessingCount = 0;
    if (oldCount != 0) {
        emit currentProcessingCountChanged();
    }

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();

    qWarning() << "[ProcessingController] All tasks force cancelled successfully";
}

QString ProcessingController::addTask(const Message& message)
{
    QString sessionId;
    if (m_sessionController) {
        sessionId = m_sessionController->sessionIdForMessage(message.id);
        if (sessionId.isEmpty()) {
            sessionId = m_sessionController->activeSessionId();
        }
    }
    
    for (const auto& mediaFile : message.mediaFiles) {
        QueueTask task;
        task.taskId = generateTaskId();
        task.messageId = message.id;
        task.fileId = mediaFile.id;
        task.position = m_tasks.size();
        task.progress = 0;
        task.queuedAt = QDateTime::currentDateTime();
        task.status = ProcessingStatus::Pending;

        m_tasks.append(task);
        m_processingModel->addTask(task);
        m_taskToMessage[task.taskId] = message.id;
        m_taskMessages[task.taskId] = message;
        
        qint64 estimatedMemory = estimateMemoryUsage(mediaFile.filePath, mediaFile.type);
        registerTaskContext(task.taskId, message.id, sessionId, mediaFile.id, estimatedMemory);

        emit taskAdded(task.taskId);
    }

    updateQueuePositions();
    emit queueSizeChanged();

    processNextTask();

    return message.id;
}

void ProcessingController::reorderTask(const QString& taskId, int newPosition)
{
    int oldIndex = -1;
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            oldIndex = i;
            break;
        }
    }

    if (oldIndex == -1 || newPosition < 0 || newPosition >= m_tasks.size()) {
        return;
    }

    // 只允许重新排列待处理的任务
    if (m_tasks[oldIndex].status != ProcessingStatus::Pending) {
        return;
    }

    QueueTask task = m_tasks.takeAt(oldIndex);
    m_tasks.insert(newPosition, task);

    updateQueuePositions();
    syncModelTasks();
}

QueueTask* ProcessingController::getTask(const QString& taskId)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            return &task;
        }
    }
    return nullptr;
}

QList<QueueTask> ProcessingController::getAllTasks() const
{
    return m_tasks;
}

bool ProcessingController::hasTasksForMessage(const QString& messageId) const
{
    if (messageId.isEmpty()) {
        return false;
    }

    for (const auto& task : m_tasks) {
        if (task.messageId == messageId &&
            task.status != ProcessingStatus::Completed &&
            task.status != ProcessingStatus::Failed &&
            task.status != ProcessingStatus::Cancelled) {
            return true;
        }
    }

    return false;
}

void ProcessingController::clearCompletedTasks()
{
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [](const QueueTask& task) {
                return task.status == ProcessingStatus::Completed ||
                       task.status == ProcessingStatus::Failed ||
                       task.status == ProcessingStatus::Cancelled;
            }),
        m_tasks.end()
    );

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
}

void ProcessingController::processNextTask()
{
    if (m_queueStatus != QueueStatus::Running) {
        return;
    }

    // 队列状态一致性验证与自动修复
    validateAndRepairQueueState();

    // 线性任务队列：一次只处理一个任务
    if (m_currentProcessingCount >= 1) {
        return;
    }

    // 查找下一个待处理的任务（FIFO原则）
    QueueTask* nextTask = nullptr;
    for (auto& task : m_tasks) {
        if (task.status != ProcessingStatus::Pending) {
            continue;
        }
        
        // 检查 AI 推理引擎池可用性
        if (m_taskMessages.contains(task.taskId)) {
            const Message& msg = m_taskMessages[task.taskId];
            if (msg.mode == ProcessingMode::AIInference && m_aiEnginePool->availableCount() <= 0) {
                continue;
            }
        }
        
        nextTask = &task;
        break;
    }

    if (!nextTask) {
        return;
    }

    if (!tryStartTask(*nextTask)) {
        // 如果启动失败，稍后重试
        QTimer::singleShot(100, this, &ProcessingController::processNextTask);
    }
}

QString ProcessingController::generateTaskId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ProcessingController::validateAndRepairQueueState()
{
    // 统计实际处理中的任务数量
    int actualProcessingCount = 0;
    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Processing) {
            actualProcessingCount++;
        }
    }

    // 检测并修复计数器不一致
    if (m_currentProcessingCount != actualProcessingCount) {
        qWarning() << "[ProcessingController] Queue state inconsistency detected!"
                   << "m_currentProcessingCount:" << m_currentProcessingCount
                   << "actualProcessingCount:" << actualProcessingCount
                   << "taskCount:" << m_tasks.size();

        // 自动修复计数器
        const int oldCount = m_currentProcessingCount;
        m_currentProcessingCount = actualProcessingCount;
        
        emit currentProcessingCountChanged();
    }

    // 清理已完成/失败/取消但仍留在列表中的任务（如果超过阈值）
    int settledTaskCount = 0;
    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Completed ||
            task.status == ProcessingStatus::Failed ||
            task.status == ProcessingStatus::Cancelled) {
            settledTaskCount++;
        }
    }

    // 如果已结束的任务超过 50 个，自动清理
    if (settledTaskCount > 50) {
        clearCompletedTasks();
    }
}

void ProcessingController::preloadModel(const QString& modelId)
{
    if (modelId.isEmpty() || !m_aiEnginePool) {
        return;
    }
    
    m_aiEnginePool->warmupModel(modelId);
}

void ProcessingController::updateQueuePositions()
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        m_tasks[i].position = i;
    }
}

void ProcessingController::startTask(QueueTask& task)
{
    QElapsedTimer perfTimer;
    perfTimer.start();

    task.status = ProcessingStatus::Processing;
    task.startedAt = QDateTime::currentDateTime();
    task.progress = 0;  // 确保进度初始化为 0
    m_currentProcessingCount++;

    const QString startedSessionId = resolveSessionIdForMessage(task.messageId);
    
    // 【关键修复】注册活动任务到 TaskStateManager
    TaskStateManager::instance()->registerActiveTask(
        task.taskId, task.messageId, startedSessionId, task.fileId
    );

    emit taskStarted(task.taskId);
    emit currentProcessingCountChanged();
    syncModelTask(task);

    updateFileStatusForSessionMessage(startedSessionId, task.messageId, task.fileId,
        ProcessingStatus::Processing, QString());
    syncMessageStatus(task.messageId, startedSessionId);
    syncMessageProgress(task.messageId, startedSessionId);

    QString taskId = task.taskId;
    
    if (m_taskMessages.contains(taskId)) {
        const Message& message = m_taskMessages[taskId];
        
        if (message.mode == ProcessingMode::Shader) {
            QString inputPath;
            for (const auto& file : message.mediaFiles) {
                for (const auto& t : m_tasks) {
                    if (t.taskId == taskId && t.fileId == file.id) {
                        inputPath = file.filePath;
                        break;
                    }
                }
                if (!inputPath.isEmpty()) break;
            }
            
            if (inputPath.isEmpty()) {
                failTask(taskId, tr("未找到媒体文件"));
                return;
            }
            
            if (ImageUtils::isImageFile(inputPath)) {
                QFileInfo fileInfo(inputPath);
                QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed";
                QDir().mkpath(processedDir);
                QString outputPath = processedDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
                
                auto processor = QSharedPointer<ImageProcessor>::create();
                m_activeImageProcessors[taskId] = processor;
                
                connect(processor.data(), &ImageProcessor::progressChanged,
                        this, [this, taskId](int progress, const QString&) {
                    updateTaskProgress(taskId, progress);
                }, Qt::QueuedConnection);
                
                connect(processor.data(), &ImageProcessor::finished,
                        this, [this, taskId](bool success, const QString& resultPath, const QString& error) {
                    m_activeImageProcessors.remove(taskId);
                    
                    if (success) {
                        completeTask(taskId, resultPath);
                    } else {
                        failTask(taskId, error);
                    }
                }, Qt::QueuedConnection);
                
                processor->processImageAsync(inputPath, outputPath, message.shaderParams);
                return;
            }
            
            QTimer::singleShot(10, this, [this, taskId]() {
                completeTask(taskId, "");
            });
            return;
        }
        
        if (message.mode == ProcessingMode::AIInference) {
            // AI 推理模式：使用 AIEngine 处理
            QString modelId = message.aiParams.modelId;
            if (modelId.isEmpty()) {
                failTask(taskId, tr("未指定AI模型"));
                return;
            }

            // 找到对应的媒体文件
            QString inputPath;
            for (const auto& file : message.mediaFiles) {
                for (const auto& t : m_tasks) {
                    if (t.taskId == taskId && t.fileId == file.id) {
                        inputPath = file.filePath;
                        break;
                    }
                }
                if (!inputPath.isEmpty()) break;
            }

            if (inputPath.isEmpty()) {
                failTask(taskId, tr("未找到媒体文件"));
                return;
            }

            // 生成输出路径（统一到应用专用 processed 目录，避免原目录权限/覆盖问题）
            QFileInfo fileInfo(inputPath);
            const QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/processed/ai";
            if (!QDir().mkpath(processedDir)) {
                failTask(taskId, tr("无法创建AI输出目录: %1").arg(processedDir));
                return;
            }

            // 视频文件输出为 .mp4，图像文件保持原格式（默认 png）
            const bool isVideo = ImageUtils::isVideoFile(inputPath);
            const QString suffix = isVideo ? QStringLiteral("mp4")
                                           : (!fileInfo.suffix().isEmpty() ? fileInfo.suffix() : QStringLiteral("png"));
            const QString uniqueToken = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") +
                                        QStringLiteral("_") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
            const QString outputPath = processedDir + "/" +
                                       fileInfo.completeBaseName() + "_ai_enhanced_" + uniqueToken + "." + suffix;

            // 从引擎池获取可用引擎
            AIEngine* engine = m_aiEnginePool->acquire(taskId);
            if (!engine) {
                failTask(taskId, tr("AI引擎池已耗尽，无法启动任务"));
                return;
            }
            connectAiEngineForTask(engine, taskId);

            // ── 异步模型加载：避免阻塞 UI 线程 ──────────────
            if (engine->currentModelId() == modelId) {
                // 模型已加载，直接启动推理
                launchAiInference(engine, taskId, inputPath, outputPath, message);
            } else {
                // 标记任务正在等待模型加载
                m_pendingModelLoadTaskIds.insert(taskId);
                
                // 连接模型加载完成信号
                QMetaObject::Connection loadConn = connect(engine, &AIEngine::modelLoadCompleted,
                    this, [this, engine, taskId, inputPath, outputPath, modelId]
                          (bool success, const QString& loadedModelId, const QString& error) {
                    m_pendingModelLoadTaskIds.remove(taskId);
                    
                    if (!success) {
                        qWarning() << "[ProcessingController][AI] async model load failed"
                                   << "task:" << taskId
                                   << "model:" << loadedModelId
                                   << "error:" << error;
                        failTask(taskId, error.isEmpty() ? tr("模型加载失败: %1").arg(loadedModelId) : error);
                        return;
                    }
                    
                    // 检查任务是否仍然有效
                    bool taskValid = false;
                    for (const auto& t : m_tasks) {
                        if (t.taskId == taskId && t.status == ProcessingStatus::Processing) {
                            taskValid = true;
                            break;
                        }
                    }
                    
                    if (taskValid && m_taskMessages.contains(taskId)) {
                        const Message& msg = m_taskMessages[taskId];
                        launchAiInference(engine, taskId, inputPath, outputPath, msg);
                    }
                }, Qt::SingleShotConnection);
                
                // 启动异步加载
                engine->loadModelAsync(modelId);
            }

            return;
        }
    }

    QTimer::singleShot(10, this, [this, taskId]() {
        completeTask(taskId, "");
    });
}

void ProcessingController::updateTaskProgress(const QString& taskId, int progress)
{
    QElapsedTimer perfTimer;
    perfTimer.start();

    TaskStateManager::instance()->updateTaskProgress(taskId, progress);

    constexpr int kProgressEmitStep = 2;
    constexpr qint64 kProgressEmitIntervalMs = 100;

    const int normalizedProgress = qBound(0, progress, 100);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            const int previousProgress = task.progress;
            task.progress = normalizedProgress;

            const int lastReported = m_lastReportedTaskProgress.value(taskId, -1);
            const qint64 lastUpdateMs = m_lastTaskProgressUpdateMs.value(taskId, 0);

            const bool firstEmit = (lastReported < 0);
            const bool crossedStep = (lastReported < 0) || (normalizedProgress - lastReported >= kProgressEmitStep);
            const bool timeoutReached = (nowMs - lastUpdateMs) >= kProgressEmitIntervalMs;

            if (firstEmit || crossedStep || timeoutReached || normalizedProgress < lastReported) {
                m_lastReportedTaskProgress[taskId] = normalizedProgress;
                m_lastTaskProgressUpdateMs[taskId] = nowMs;

                emit taskProgress(taskId, normalizedProgress);
                syncModelTask(task);

                if (normalizedProgress != previousProgress) {
                    constexpr qint64 kMessageProgressSyncIntervalMs = 150;
                    const qint64 lastMessageSyncMs = m_lastMessageProgressSyncMs.value(task.messageId, 0);
                    const bool shouldSyncMessageProgress = ((nowMs - lastMessageSyncMs) >= kMessageProgressSyncIntervalMs);
                    if (shouldSyncMessageProgress) {
                        syncMessageProgress(task.messageId);
                    }
                }
            }
            break;
        }
    }
}

void ProcessingController::completeTask(const QString& taskId, const QString& resultPath)
{

    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            QString messageId = m_tasks[i].messageId;
            QString fileId = m_tasks[i].fileId;
            const QString sessionId = resolveSessionIdForMessage(messageId);

            if (m_tasks[i].status == ProcessingStatus::Cancelled) {
                cleanupTask(taskId);
                emit taskCancelled(taskId);
            } else if (m_taskCoordinator->isOrphaned(taskId)) {
                handleOrphanedTask(taskId);
                return;
            } else {
                m_tasks[i].progress = 99;
                emit taskProgress(taskId, 99);
                syncModelTask(m_tasks[i]);
                syncMessageProgress(messageId);
                
                m_tasks[i].status = ProcessingStatus::Completed;
                m_tasks[i].progress = 100;
                emit taskCompleted(taskId, resultPath);

                const Message msg = messageForSession(sessionId, messageId);

                bool fileHandled = false;
                for (const MediaFile& mf : msg.mediaFiles) {
                    if (mf.id == fileId) {
                        if (msg.mode == ProcessingMode::Shader && ImageUtils::isImageFile(mf.filePath)) {
                            const QString finalPath = !resultPath.isEmpty() ? resultPath : mf.filePath;
                            
                            if (!finalPath.isEmpty() && ThumbnailProvider::instance()) {
                                const QString thumbId = "processed_" + fileId;
                                ThumbnailProvider::instance()->generateThumbnailAsync(finalPath, thumbId, QSize(512, 512));
                            }
                            
                            updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                                ProcessingStatus::Completed, finalPath);
                            fileHandled = true;
                            
                            finalizeTask(taskId, sessionId, messageId);
                        } else if (msg.mode == ProcessingMode::Shader && ImageUtils::isVideoFile(mf.filePath)) {
                            processShaderVideoThumbnailAsync(
                                taskId,
                                messageId,
                                fileId,
                                mf.filePath,
                                msg.shaderParams
                            );
                            fileHandled = true;
                        } else {
                            if (msg.mode == ProcessingMode::AIInference) {
                                QFileInfo aiOutputInfo(resultPath);
                                const bool aiOutputValid = !resultPath.isEmpty() && aiOutputInfo.exists() && aiOutputInfo.size() > 0;
                                if (!aiOutputValid) {
                                    qWarning() << "[ProcessingController][AI] completeTask invalid result path"
                                               << "task:" << taskId
                                               << "message:" << messageId
                                               << "file:" << fileId
                                               << "result:" << resultPath
                                               << "exists:" << aiOutputInfo.exists()
                                               << "size:" << aiOutputInfo.size();
                                    failTask(taskId, tr("AI推理结果无效或输出文件缺失"));
                                    return;
                                }
                            }

                            const QString finalPath = !resultPath.isEmpty() ? resultPath : mf.filePath;

                            if (!finalPath.isEmpty() && ThumbnailProvider::instance()) {
                                const QString thumbId = "processed_" + fileId;
                                ThumbnailProvider::instance()->generateThumbnailAsync(finalPath, thumbId, QSize(512, 512));
                            }

                            updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                                ProcessingStatus::Completed, finalPath);
                            fileHandled = true;

                            finalizeTask(taskId, sessionId, messageId);
                        }
                        break;
                    }
                }

                if (!fileHandled) {
                    qWarning() << "[ProcessingController] completeTask target file not found in message"
                               << "task:" << taskId
                               << "message:" << messageId
                               << "file:" << fileId;
                    failTask(taskId, tr("未找到待更新的媒体文件"));
                    return;
                }
            }
            break;
        }
    }
}

void ProcessingController::onShaderExportCompleted(const QString& exportId, bool success, const QString& outputPath, const QString& error)
{
    if (!m_pendingExports.contains(exportId)) {
        return;
    }

    PendingExport pending = m_pendingExports.take(exportId);

    if (pending.isVideoThumbnail && !pending.tempFramePath.isEmpty()) {
        QFile::remove(pending.tempFramePath);
    }

    const bool orphaned = m_taskCoordinator->isOrphaned(exportId);

    if (success) {
        if (!orphaned) {
            const QString thumbId = "processed_" + pending.fileId;
            if (ThumbnailProvider::instance()) {
                ThumbnailProvider::instance()->generateThumbnailAsync(outputPath, thumbId, QSize(512, 512));
            }

            updateFileStatusForSessionMessage(pending.sessionId, pending.messageId, pending.fileId,
                ProcessingStatus::Completed, outputPath);
        }
    } else {
        if (!orphaned) {
            updateFileStatusForSessionMessage(pending.sessionId, pending.messageId, pending.fileId,
                ProcessingStatus::Failed, QString());
            updateErrorForSessionMessage(pending.sessionId, pending.messageId, error);
        }
    }

    finalizeTask(exportId, pending.sessionId, pending.messageId);
}

void ProcessingController::processShaderVideoThumbnailAsync(const QString& taskId,
                                                            const QString& messageId,
                                                            const QString& fileId,
                                                            const QString& filePath,
                                                            const ShaderParams& shaderParams)
{
    const QString sessionId = resolveSessionIdForMessage(messageId);

    QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed/shader_video";
    QDir().mkpath(processedDir);
    QFileInfo fi(filePath);
    QString outputPath = processedDir + "/" + fi.completeBaseName()
                         + "_shader_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
                         + "." + fi.suffix();

    auto videoProcessor = QSharedPointer<VideoProcessor>::create();
    m_activeVideoProcessors[taskId] = videoProcessor;

    // 缩略图将在视频处理完成后生成，避免读取正在写入的文件

    // 直接在主线程设置回调，避免嵌套异步调用
    // VideoProcessor 内部会启动自己的工作线程
    auto progressCb = [this, taskId](int progress, const QString&) {
        if (!m_activeVideoProcessors.contains(taskId)) return;
        int mappedProgress = 10 + progress * 85 / 100;
        QMetaObject::invokeMethod(this, [this, taskId, mappedProgress]() {
            updateTaskProgress(taskId, mappedProgress);
        }, Qt::QueuedConnection);
    };

    // 捕获 videoProcessor 确保生命周期延长到回调完成
    auto finishCb = [this, videoProcessor, taskId, sessionId, messageId, fileId, outputPath](
                        bool success, const QString& resultPath, const QString& error) {
        Q_UNUSED(resultPath);
        QMetaObject::invokeMethod(this, [this, videoProcessor, taskId, sessionId, messageId,
                                          fileId, outputPath, success, error]() {
            m_activeVideoProcessors.remove(taskId);

            if (m_taskCoordinator->isOrphaned(taskId)) {
                finalizeTask(taskId, sessionId, messageId);
                return;
            }

            if (success) {
                // 验证输出文件完整性
                QFileInfo outputInfo(outputPath);
                if (!outputInfo.exists() || outputInfo.size() == 0) {
                    qWarning() << "[ProcessingController][Shader] output file invalid"
                               << "path:" << outputPath
                               << "exists:" << outputInfo.exists()
                               << "size:" << outputInfo.size();
                    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                        ProcessingStatus::Failed, QString());
                    updateErrorForSessionMessage(sessionId, messageId, tr("视频输出文件无效"));
                } else {
                    // 成功后生成缩略图
                    if (ThumbnailProvider::instance()) {
                        const QString thumbId = "processed_" + fileId;
                        ThumbnailProvider::instance()->generateThumbnailAsync(
                            outputPath, thumbId, QSize(512, 512));
                    }
                    
                    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                        ProcessingStatus::Completed, outputPath);
                }
            } else {
                updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                    ProcessingStatus::Failed, QString());
                updateErrorForSessionMessage(sessionId, messageId,
                    error.isEmpty() ? tr("视频Shader处理失败") : error);
            }

            finalizeTask(taskId, sessionId, messageId);
        }, Qt::QueuedConnection);
    };

    // 直接调用 processVideoAsync，它内部会启动工作线程
    videoProcessor->processVideoAsync(filePath, outputPath, shaderParams, progressCb, finishCb);
}

QVariantMap ProcessingController::shaderParamsToVariantMap(const ShaderParams& params)
{
    QVariantMap map;
    map["brightness"] = params.brightness;
    map["contrast"] = params.contrast;
    map["saturation"] = params.saturation;
    map["hue"] = params.hue;
    map["sharpness"] = params.sharpness;
    map["blur"] = params.blur;
    map["denoise"] = params.denoise;
    map["exposure"] = params.exposure;
    map["gamma"] = params.gamma;
    map["temperature"] = params.temperature;
    map["tint"] = params.tint;
    map["vignette"] = params.vignette;
    map["highlights"] = params.highlights;
    map["shadows"] = params.shadows;
    return map;
}

void ProcessingController::failTask(const QString& taskId, const QString& error)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            const QString messageId = task.messageId;
            const QString fileId = task.fileId;
            const QString sessionId = resolveSessionIdForMessage(messageId);

            task.status = ProcessingStatus::Failed;
            emit taskFailed(taskId, error);

            if (!m_taskCoordinator->isOrphaned(taskId)) {
                updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                    ProcessingStatus::Failed, QString());
                updateErrorForSessionMessage(sessionId, messageId, error);
            }

            finalizeTask(taskId, sessionId, messageId);
            break;
        }
    }
}

void ProcessingController::setFileController(FileController* fileController)
{
    m_fileController = fileController;
}

QString ProcessingController::sendToProcessing(int mode, const QVariantMap& params)
{
    if (!m_fileController) {
        return QString();
    }

    QList<MediaFile> files = m_fileController->getAllFiles();
    if (files.isEmpty()) {
        return QString();
    }


    // 创建 Message 对象
    Message message;
    message.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    message.timestamp = QDateTime::currentDateTime();
    message.mode = (mode == 0) ? ProcessingMode::Shader : ProcessingMode::AIInference;
    message.mediaFiles = files;
    message.parameters = params;
    message.status = ProcessingStatus::Pending;
    message.progress = 0;

    // 设置 AI 参数
    if (mode == 1) {
        message.aiParams.modelId = params.value("modelId").toString();
        message.aiParams.useGpu = params.value("useGpu", true).toBool();
        message.aiParams.tileSize = params.value("tileSize", 0).toInt();

        QString categoryStr = params.value("category").toString();
        if (!categoryStr.isEmpty()) {
            static const QMap<QString, ModelCategory> catMap = {
                {"super_resolution", ModelCategory::SuperResolution},
                {"denoising", ModelCategory::Denoising},
                {"deblurring", ModelCategory::Deblurring},
                {"dehazing", ModelCategory::Dehazing},
                {"colorization", ModelCategory::Colorization},
                {"low_light", ModelCategory::LowLight},
                {"frame_interpolation", ModelCategory::FrameInterpolation},
                {"inpainting", ModelCategory::Inpainting}
            };
            message.aiParams.category = catMap.value(categoryStr, ModelCategory::SuperResolution);
        }

        // 收集模型特定参数
        QVariantMap modelParams;
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (it.key() != "modelId" && it.key() != "useGpu" &&
                it.key() != "tileSize" && it.key() != "category" &&
                it.key() != "modelIndex") {
                modelParams[it.key()] = it.value();
            }
        }
        message.aiParams.modelParams = modelParams;
    }

    // 设置 Shader 参数
    if (mode == 0) {
        message.shaderParams.brightness = params.value("brightness", 0.0f).toFloat();
        message.shaderParams.contrast = params.value("contrast", 1.0f).toFloat();
        message.shaderParams.saturation = params.value("saturation", 1.0f).toFloat();
        message.shaderParams.hue = params.value("hue", 0.0f).toFloat();
        message.shaderParams.sharpness = params.value("sharpness", 0.0f).toFloat();
        message.shaderParams.blur = params.value("blur", 0.0f).toFloat();
        message.shaderParams.denoise = params.value("denoise", 0.0f).toFloat();
        message.shaderParams.exposure = params.value("exposure", 0.0f).toFloat();
        message.shaderParams.gamma = params.value("gamma", 1.0f).toFloat();
        message.shaderParams.temperature = params.value("temperature", 0.0f).toFloat();
        message.shaderParams.tint = params.value("tint", 0.0f).toFloat();
        message.shaderParams.vignette = params.value("vignette", 0.0f).toFloat();
        message.shaderParams.highlights = params.value("highlights", 0.0f).toFloat();
        message.shaderParams.shadows = params.value("shadows", 0.0f).toFloat();
    }

    // 将消息添加到 MessageModel（QML 会自动更新显示）
    if (m_messageModel) {
        m_messageModel->addMessage(message);
    }
    
    // 同步消息到当前活动的 Session
    if (m_sessionController && m_messageModel) {
        requestSessionSync();
    }

    // 清空文件列表（文件已转入消息）
    m_fileController->clearFiles();
    return addTask(message);
}

void ProcessingController::retryMessage(const QString& messageId)
{
    if (!m_messageModel) {
        qWarning() << "[ProcessingController] retryMessage: MessageModel not set";
        return;
    }
    
    Message message = m_messageModel->messageById(messageId);
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] retryMessage: Message not found:" << messageId;
        return;
    }
    
    m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Pending));
    m_messageModel->updateProgress(messageId, 0);
    m_messageModel->updateErrorMessage(messageId, QString());
    
    // 重置所有文件状态
    for (const auto& file : message.mediaFiles) {
        m_messageModel->updateFileStatus(messageId, file.id, 
            static_cast<int>(ProcessingStatus::Pending), QString());
    }
    
    // 同步到会话
    if (m_sessionController) {
        requestSessionSync();
    }
    
    // 重新添加任务
    addTask(message);
}

void ProcessingController::retryFailedFiles(const QString& messageId)
{
    if (!m_messageModel) {
        qWarning() << "[ProcessingController] retryFailedFiles: MessageModel not set";
        return;
    }

    Message message = m_messageModel->messageById(messageId);
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] retryFailedFiles: Message not found:" << messageId;
        return;
    }

    QList<MediaFile> filesToRetry;
    for (const auto& file : message.mediaFiles) {
        if (file.status == ProcessingStatus::Failed || file.status == ProcessingStatus::Cancelled) {
            filesToRetry.append(file);
        }
    }

    if (filesToRetry.isEmpty()) {
        return;
    }

    m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Processing));
    m_messageModel->updateErrorMessage(messageId, QString());

    const QString sessionId = resolveSessionIdForMessage(messageId);

    for (const auto& file : filesToRetry) {
        m_messageModel->updateFileStatus(messageId, file.id,
            static_cast<int>(ProcessingStatus::Pending), QString());

        QueueTask task;
        task.taskId = generateTaskId();
        task.messageId = messageId;
        task.fileId = file.id;
        task.position = m_tasks.size();
        task.progress = 0;
        task.queuedAt = QDateTime::currentDateTime();
        task.status = ProcessingStatus::Pending;

        m_tasks.append(task);
        m_processingModel->addTask(task);
        m_taskToMessage[task.taskId] = messageId;
        m_taskMessages[task.taskId] = message;

        qint64 estimatedMemory = estimateMemoryUsage(file.filePath, file.type);
        registerTaskContext(task.taskId, messageId, sessionId, file.id, estimatedMemory);

        emit taskAdded(task.taskId);
    }

    updateQueuePositions();
    emit queueSizeChanged();

    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
}

void ProcessingController::retrySingleFailedFile(const QString& messageId, int fileIndex)
{
    if (!m_messageModel) {
        qWarning() << "[ProcessingController] retrySingleFailedFile: MessageModel not set";
        return;
    }
    
    const QString sessionId = resolveSessionIdForMessage(messageId);
    Message message = messageForSession(sessionId, messageId);
    if (message.id.isEmpty()) {
        message = m_messageModel->messageById(messageId);
    }
    
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] retrySingleFailedFile: Message not found:" << messageId;
        return;
    }
    
    if (fileIndex < 0 || fileIndex >= message.mediaFiles.size()) {
        qWarning() << "[ProcessingController] retrySingleFailedFile: Invalid file index:" << fileIndex;
        return;
    }
    
    const MediaFile& file = message.mediaFiles[fileIndex];
    bool canRetry = file.status == ProcessingStatus::Failed || 
                    file.status == ProcessingStatus::Cancelled;
    
    if (!canRetry) {
        qWarning() << "[ProcessingController] retrySingleFailedFile: File is not in a retryable status:" << fileIndex 
                   << "status:" << static_cast<int>(file.status);
        return;
    }
    
    if (message.status == ProcessingStatus::Failed || message.status == ProcessingStatus::Pending) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Processing));
    }
    m_messageModel->updateErrorMessage(messageId, QString());
    
    m_messageModel->updateFileStatus(messageId, file.id,
        static_cast<int>(ProcessingStatus::Pending), QString());
    
    QueueTask task;
    task.taskId = generateTaskId();
    task.messageId = messageId;
    task.fileId = file.id;
    task.position = m_tasks.size();
    task.progress = 0;
    task.queuedAt = QDateTime::currentDateTime();
    task.status = ProcessingStatus::Pending;

    m_tasks.append(task);
    m_processingModel->addTask(task);
    m_taskToMessage[task.taskId] = messageId;
    m_taskMessages[task.taskId] = message;
    
    qint64 estimatedMemory = estimateMemoryUsage(file.filePath, file.type);
    registerTaskContext(task.taskId, messageId, sessionId, file.id, estimatedMemory);

    emit taskAdded(task.taskId);

    updateQueuePositions();
    emit queueSizeChanged();
    
    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
}

void ProcessingController::autoRetryInterruptedFiles(const QString& messageId, const QString& sessionId)
{
    if (!m_sessionController) {
        qWarning() << "[ProcessingController] autoRetryInterruptedFiles: SessionController not set";
        return;
    }

    const QString targetSessionId = !sessionId.isEmpty()
        ? sessionId
        : resolveSessionIdForMessage(messageId);

    // 从 Session 中获取消息，而不是从 MessageModel（支持后台会话）
    Message message = m_sessionController->messageInSession(targetSessionId, messageId);
    if (message.id.isEmpty()) {
        // 如果 Session 中找不到，尝试从 MessageModel 获取（当前活动会话）
        if (m_messageModel) {
            message = m_messageModel->messageById(messageId);
        }
    }
    
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] autoRetryInterruptedFiles: Message not found:"
                   << messageId << "session:" << targetSessionId;
        return;
    }

    if (hasTasksForMessage(messageId)) {
        // 该消息已有在途任务，避免会话切换触发重复入队导致卡住或串扰
        return;
    }

    QList<MediaFile> filesToRetry;
    for (const auto& file : message.mediaFiles) {
        if (file.status == ProcessingStatus::Pending || file.status == ProcessingStatus::Processing) {
            filesToRetry.append(file);
        }
    }

    if (filesToRetry.isEmpty()) {
        return;
    }

    // 更新 Session 中的消息状态
    message.status = ProcessingStatus::Processing;
    message.errorMessage.clear();
    for (auto& file : message.mediaFiles) {
        if (file.status == ProcessingStatus::Pending || file.status == ProcessingStatus::Processing) {
            file.status = ProcessingStatus::Pending;
        }
    }
    m_sessionController->updateMessageInSession(targetSessionId, message);

    // 如果是当前活动会话，同时更新 MessageModel
    if (m_messageModel && m_sessionController->activeSessionId() == targetSessionId) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Processing));
        m_messageModel->updateErrorMessage(messageId, QString());
        for (const auto& file : filesToRetry) {
            m_messageModel->updateFileStatus(messageId, file.id,
                static_cast<int>(ProcessingStatus::Pending), QString());
        }
    }

    for (const auto& file : filesToRetry) {
        QueueTask task;
        task.taskId = generateTaskId();
        task.messageId = messageId;
        task.fileId = file.id;
        task.position = m_tasks.size();
        task.progress = 0;
        task.queuedAt = QDateTime::currentDateTime();
        task.status = ProcessingStatus::Pending;

        m_tasks.append(task);
        m_processingModel->addTask(task);
        m_taskToMessage[task.taskId] = messageId;
        m_taskMessages[task.taskId] = message;

        qint64 estimatedMemory = estimateMemoryUsage(file.filePath, file.type);
        registerTaskContext(task.taskId, messageId, targetSessionId, file.id, estimatedMemory);

        emit taskAdded(task.taskId);
    }

    updateQueuePositions();
    emit queueSizeChanged();

    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
}

void ProcessingController::requestSessionMemorySync(const QString& messageId)
{
    if (!m_sessionController) {
        return;
    }

    if (!messageId.isEmpty()) {
        m_pendingMemorySyncMessageIds.insert(messageId);
    }

    if (m_memorySyncTimer) {
        m_memorySyncTimer->start();
    }
}

void ProcessingController::flushSessionMemorySync()
{
    if (!m_sessionController) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (m_pendingMemorySyncMessageIds.isEmpty()) {
        m_sessionController->syncCurrentMessagesToSession();
        return;
    }

    const QSet<QString> pendingIds = m_pendingMemorySyncMessageIds;
    for (const QString& messageId : pendingIds) {
        const qint64 lastMs = m_lastSessionMemorySyncMs.value(messageId, 0);
        if (lastMs > 0 && (nowMs - lastMs) < 100) {
            continue;
        }
        m_lastSessionMemorySyncMs[messageId] = nowMs;
        m_pendingMemorySyncMessageIds.remove(messageId);
    }

    if (!pendingIds.isEmpty()) {
        m_sessionController->syncCurrentMessagesToSession();
    }
}

void ProcessingController::syncVisibleMessageIfNeeded(const QString& sessionId,
                                                      const QString& messageId,
                                                      const std::function<void()>& syncFn)
{
    if (!m_messageModel || !m_sessionController || sessionId.isEmpty()) {
        return;
    }

    if (m_sessionController->activeSessionId() != sessionId) {
        return;
    }

    syncFn();
    requestSessionMemorySync(messageId);
}

void ProcessingController::requestSessionSync()
{
    if (!m_sessionSyncTimer) {
        return;
    }

    m_sessionSyncTimer->start();
}

void ProcessingController::syncModelTasks()
{
    m_processingModel->updateTasks(m_tasks);
}

void ProcessingController::syncModelTask(const QueueTask& task)
{
    m_processingModel->updateTask(task);
}

void ProcessingController::syncModelTaskById(const QString& taskId)
{
    for (const auto& task : m_tasks) {
        if (task.taskId == taskId) {
            syncModelTask(task);
            return;
        }
    }
}

QString ProcessingController::resolveSessionIdForMessage(const QString& messageId, const QString& fallbackSessionId) const
{
    if (!fallbackSessionId.isEmpty()) {
        return fallbackSessionId;
    }

    QString resolvedSessionId;
    for (auto it = m_taskContexts.constBegin(); it != m_taskContexts.constEnd(); ++it) {
        const TaskContext& ctx = it.value();
        if (ctx.messageId == messageId && !ctx.sessionId.isEmpty()) {
            resolvedSessionId = ctx.sessionId;
            break;
        }
    }

    if (!resolvedSessionId.isEmpty()) {
        return resolvedSessionId;
    }

    if (m_sessionController) {
        resolvedSessionId = m_sessionController->sessionIdForMessage(messageId);
        if (!resolvedSessionId.isEmpty()) {
            return resolvedSessionId;
        }

        return m_sessionController->activeSessionId();
    }

    return QString();
}

Message ProcessingController::messageForSession(const QString& sessionId, const QString& messageId) const
{
    if (m_sessionController && !sessionId.isEmpty()) {
        const Message sessionMessage = m_sessionController->messageInSession(sessionId, messageId);
        if (!sessionMessage.id.isEmpty()) {
            return sessionMessage;
        }
    }

    if (m_messageModel) {
        return m_messageModel->messageById(messageId);
    }

    return Message();
}

void ProcessingController::updateFileStatusForSessionMessage(const QString& sessionId, const QString& messageId,
                                                             const QString& fileId, ProcessingStatus status,
                                                             const QString& resultPath)
{
    bool sessionUpdated = false;

    if (m_sessionController && !sessionId.isEmpty()) {
        Message message = m_sessionController->messageInSession(sessionId, messageId);
        if (!message.id.isEmpty()) {
            bool changed = false;
            for (auto& file : message.mediaFiles) {
                if (file.id == fileId) {
                    if (file.status != status) {
                        file.status = status;
                        changed = true;
                    }
                    if (!resultPath.isEmpty() && file.resultPath != resultPath) {
                        if (file.originalPath.isEmpty()) {
                            file.originalPath = file.filePath;
                        }
                        file.resultPath = resultPath;
                        changed = true;
                    }
                    break;
                }
            }

            if (changed) {
                sessionUpdated = m_sessionController->updateMessageInSession(sessionId, message);
                if (!sessionUpdated) {
                    qWarning() << "[ProcessingController] updateMessageInSession failed"
                               << "session:" << sessionId
                               << "message:" << messageId
                               << "file:" << fileId
                               << "status:" << static_cast<int>(status)
                               << "result:" << resultPath;
                }
            } else {
                sessionUpdated = true;
            }
        } else {
            qWarning() << "[ProcessingController] message not found in session while updating file status"
                       << "session:" << sessionId
                       << "message:" << messageId
                       << "file:" << fileId
                       << "status:" << static_cast<int>(status)
                       << "result:" << resultPath;
        }
    }

    if (m_messageModel) {
        m_messageModel->updateFileStatus(messageId, fileId, static_cast<int>(status), resultPath);
    }

    if (sessionUpdated && m_sessionController && !sessionId.isEmpty() &&
        m_sessionController->activeSessionId() == sessionId) {
        requestSessionMemorySync(messageId);
    }
}

void ProcessingController::updateErrorForSessionMessage(const QString& sessionId, const QString& messageId,
                                                        const QString& errorMessage)
{
    if (!m_sessionController || sessionId.isEmpty()) {
        return;
    }

    Message message = m_sessionController->messageInSession(sessionId, messageId);
    if (message.id.isEmpty()) {
        return;
    }

    message.errorMessage = errorMessage;
    m_sessionController->updateMessageInSession(sessionId, message);

    syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, errorMessage]() {
        m_messageModel->updateErrorMessage(messageId, errorMessage);
    });
}

void ProcessingController::updateProgressForSessionMessage(const QString& sessionId, const QString& messageId, int progress)
{
    if (!m_sessionController || sessionId.isEmpty()) {
        return;
    }

    Message message = m_sessionController->messageInSession(sessionId, messageId);
    if (message.id.isEmpty()) {
        return;
    }

    message.progress = qBound(0, progress, 100);
    m_sessionController->updateMessageInSession(sessionId, message);

    syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, message]() {
        m_messageModel->updateProgress(messageId, message.progress);
    });
}

void ProcessingController::updateStatusForSessionMessage(const QString& sessionId, const QString& messageId, ProcessingStatus status)
{
    if (!m_sessionController || sessionId.isEmpty()) {
        return;
    }

    Message message = m_sessionController->messageInSession(sessionId, messageId);
    if (message.id.isEmpty()) {
        return;
    }

    message.status = status;
    m_sessionController->updateMessageInSession(sessionId, message);

    syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, status]() {
        m_messageModel->updateStatus(messageId, static_cast<int>(status));
    });
}

void ProcessingController::updateQueuePositionForSessionMessage(const QString& sessionId, const QString& messageId, int queuePosition)
{
    if (!m_sessionController || sessionId.isEmpty()) {
        return;
    }

    Message message = m_sessionController->messageInSession(sessionId, messageId);
    if (message.id.isEmpty()) {
        return;
    }

    message.queuePosition = queuePosition;
    m_sessionController->updateMessageInSession(sessionId, message);

    syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, queuePosition]() {
        m_messageModel->updateQueuePosition(messageId, queuePosition);
    });
}

void ProcessingController::syncMessageProgress(const QString& messageId, const QString& sessionId)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kMessageProgressSyncDebounceMs = 250;  // 增大防抖间隔

    const qint64 lastSyncMs = m_lastMessageProgressSyncMs.value(messageId, 0);
    if (lastSyncMs > 0 && (nowMs - lastSyncMs) < kMessageProgressSyncDebounceMs) {
        return;
    }

    m_lastMessageProgressSyncMs[messageId] = nowMs;

    const QString targetSessionId = resolveSessionIdForMessage(messageId, sessionId);
    if (targetSessionId.isEmpty()) {
        return;
    }

    int totalTasks = 0;
    int totalProgress = 0;

    for (const auto& task : m_tasks) {
        if (task.messageId == messageId) {
            TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
            if (!ctx.sessionId.isEmpty() && ctx.sessionId != targetSessionId) {
                continue;
            }

            totalTasks++;
            
            if (task.status == ProcessingStatus::Processing) {
                totalProgress += task.progress;
            } else if (task.status == ProcessingStatus::Completed) {
                totalProgress += 100;
            } else if (task.status == ProcessingStatus::Failed ||
                       task.status == ProcessingStatus::Cancelled) {
                totalProgress += 100;
            }
            // Pending 状态的任务不计入进度（不累加）
        }
    }

    int overallProgress = 0;
    if (totalTasks > 0) {
        overallProgress = totalProgress / totalTasks;
    }
    
    // 确保进度在合理范围内
    overallProgress = qBound(0, overallProgress, 100);
    
    updateProgressForSessionMessage(targetSessionId, messageId, overallProgress);
}

void ProcessingController::syncMessageStatus(const QString& messageId, const QString& sessionId)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kMessageStatusSyncDebounceMs = 300;  // 增大防抖间隔

    const qint64 lastSyncMs = m_lastMessageStatusSyncMs.value(messageId, 0);
    if (lastSyncMs > 0 && (nowMs - lastSyncMs) < kMessageStatusSyncDebounceMs) {
        return;
    }

    m_lastMessageStatusSyncMs[messageId] = nowMs;

    const QString targetSessionId = resolveSessionIdForMessage(messageId, sessionId);
    if (targetSessionId.isEmpty()) {
        return;
    }

    const Message message = messageForSession(targetSessionId, messageId);
    if (message.id.isEmpty()) {
        return;
    }

    const int totalFiles = message.mediaFiles.size();
    if (totalFiles == 0) {
        return;
    }

    int pendingFiles = 0;
    int processingFiles = 0;
    int completedFiles = 0;
    int failedFiles = 0;
    int cancelledFiles = 0;

    for (const auto& file : message.mediaFiles) {
        switch (file.status) {
        case ProcessingStatus::Pending:    pendingFiles++;    break;
        case ProcessingStatus::Processing: processingFiles++; break;
        case ProcessingStatus::Completed:  completedFiles++;  break;
        case ProcessingStatus::Failed:     failedFiles++;     break;
        case ProcessingStatus::Cancelled:  cancelledFiles++;  break;
        }
    }

    ProcessingStatus newStatus = ProcessingStatus::Pending;

    const bool allSettled = (pendingFiles == 0 && processingFiles == 0);
    if (allSettled) {
        if (failedFiles > 0 && completedFiles == 0) {
            newStatus = ProcessingStatus::Failed;
        } else if (cancelledFiles == totalFiles) {
            newStatus = ProcessingStatus::Cancelled;
        } else {
            newStatus = ProcessingStatus::Completed;
        }
    } else if (processingFiles > 0 || completedFiles > 0 || failedFiles > 0 || cancelledFiles > 0) {
        newStatus = ProcessingStatus::Processing;
    }

    updateStatusForSessionMessage(targetSessionId, messageId, newStatus);

    int queuePos = 0;
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && task.status == ProcessingStatus::Pending) {
            TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
            if (!ctx.sessionId.isEmpty() && ctx.sessionId != targetSessionId) {
                continue;
            }
            queuePos = task.position + 1;
            break;
        }
    }
    updateQueuePositionForSessionMessage(targetSessionId, messageId, queuePos);
}

int ProcessingController::resourcePressure() const
{
    return m_resourcePressure;
}

void ProcessingController::cancelMessageTasks(const QString& messageId)
{
    m_taskCoordinator->cancelMessageTasks(messageId);
    
    int cancelledProcessingCount = 0;
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].messageId == messageId) {
            const QString taskId = m_tasks[i].taskId;
            const ProcessingStatus oldStatus = m_tasks[i].status;
            
            if (oldStatus == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(taskId);
                emit taskCancelled(taskId);
                m_tasks.removeAt(i);
            } else if (oldStatus == ProcessingStatus::Processing) {
                // 直接取消并移除，确保计数器正确
                m_taskCoordinator->requestCancellation(taskId);
                
                // 取消 AI 引擎处理
                if (m_taskMessages.contains(taskId)) {
                    const Message& msg = m_taskMessages[taskId];
                    if (msg.mode == ProcessingMode::AIInference) {
                        AIEngine* poolEngine = m_aiEnginePool->engineForTask(taskId);
                        if (poolEngine) {
                            poolEngine->cancelProcess();
                        }
                    }
                }
                
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(taskId);
                emit taskCancelled(taskId);
                m_tasks.removeAt(i);
                cancelledProcessingCount++;
            }
        }
    }
    
    // 修正处理计数器
    if (cancelledProcessingCount > 0 && m_currentProcessingCount > 0) {
        m_currentProcessingCount = qMax(0, m_currentProcessingCount - cancelledProcessingCount);
        emit currentProcessingCountChanged();
    }
    
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
    
    if (m_messageModel) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Cancelled));
    }
    
    emit messageTasksCancelled(messageId);
    processNextTask();
}

void ProcessingController::cancelMessageFileTasks(const QString& messageId, const QString& fileId)
{
    if (messageId.isEmpty() || fileId.isEmpty()) {
        return;
    }

    bool removedTask = false;
    int cancelledProcessingCount = 0;

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].messageId != messageId || m_tasks[i].fileId != fileId) {
            continue;
        }

        const QString taskId = m_tasks[i].taskId;
        const ProcessingStatus oldStatus = m_tasks[i].status;
        m_taskCoordinator->requestCancellation(taskId);

        if (oldStatus == ProcessingStatus::Pending) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            emit taskCancelled(taskId);
            m_tasks.removeAt(i);
            removedTask = true;
        } else if (oldStatus == ProcessingStatus::Processing) {
            // 直接取消并移除，确保计数器正确
            if (m_taskMessages.contains(taskId)) {
                const Message& msg = m_taskMessages[taskId];
                if (msg.mode == ProcessingMode::AIInference) {
                    AIEngine* poolEngine = m_aiEnginePool->engineForTask(taskId);
                    if (poolEngine) {
                        poolEngine->cancelProcess();
                    }
                }
            }
            
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            emit taskCancelled(taskId);
            m_tasks.removeAt(i);
            removedTask = true;
            cancelledProcessingCount++;
        }
    }

    // 修正处理计数器
    if (cancelledProcessingCount > 0 && m_currentProcessingCount > 0) {
        m_currentProcessingCount = qMax(0, m_currentProcessingCount - cancelledProcessingCount);
        emit currentProcessingCountChanged();
    }

    if (removedTask) {
        updateQueuePositions();
        syncModelTasks();
        emit queueSizeChanged();
    }

    processNextTask();
}

void ProcessingController::cancelSessionTasks(const QString& sessionId)
{
    m_taskCoordinator->cancelSessionTasks(sessionId);
    
    QSet<QString> messageIds;
    int cancelledProcessingCount = 0;
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        QString taskId = m_tasks[i].taskId;
        TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
        
        if (ctx.sessionId == sessionId) {
            messageIds.insert(m_tasks[i].messageId);
            const ProcessingStatus oldStatus = m_tasks[i].status;
            
            if (oldStatus == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(taskId);
                emit taskCancelled(taskId);
                m_tasks.removeAt(i);
            } else if (oldStatus == ProcessingStatus::Processing) {
                // 直接取消并移除，确保计数器正确
                m_taskCoordinator->requestCancellation(taskId);
                
                // 取消 AI 引擎处理
                if (m_taskMessages.contains(taskId)) {
                    const Message& msg = m_taskMessages[taskId];
                    if (msg.mode == ProcessingMode::AIInference) {
                        AIEngine* poolEngine = m_aiEnginePool->engineForTask(taskId);
                        if (poolEngine) {
                            poolEngine->cancelProcess();
                        }
                    }
                }
                
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(taskId);
                emit taskCancelled(taskId);
                m_tasks.removeAt(i);
                cancelledProcessingCount++;
            }
        }
    }
    
    // 修正处理计数器
    if (cancelledProcessingCount > 0 && m_currentProcessingCount > 0) {
        m_currentProcessingCount = qMax(0, m_currentProcessingCount - cancelledProcessingCount);
        emit currentProcessingCountChanged();
    }
    
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
    
    for (const QString& messageId : messageIds) {
        if (m_messageModel) {
            m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Cancelled));
        }
    }
    
    emit sessionTasksCancelled(sessionId);
    processNextTask();
}

void ProcessingController::pauseSessionTasks(const QString& sessionId)
{
    QSet<QString> taskIds = m_taskCoordinator->sessionTaskIds(sessionId);
    for (const QString& taskId : taskIds) {
        m_taskCoordinator->triggerPause(taskId, true);
    }
}

void ProcessingController::resumeSessionTasks(const QString& sessionId)
{
    QSet<QString> taskIds = m_taskCoordinator->sessionTaskIds(sessionId);
    for (const QString& taskId : taskIds) {
        m_taskCoordinator->triggerPause(taskId, false);
    }
}

void ProcessingController::onSessionChanging(const QString& oldSessionId, const QString& newSessionId)
{
    QSet<QString> oldSessionTaskIds = m_taskCoordinator->sessionTaskIds(oldSessionId);
    for (const QString& taskId : oldSessionTaskIds) {
        TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
        if (ctx.taskId.isEmpty()) {
            continue;
        }

        if (ctx.priority != TaskPriority::Background) {
            ctx.priority = TaskPriority::Background;
            if (m_taskContexts.contains(taskId)) {
                m_taskContexts[taskId].priority = TaskPriority::Background;
            }
            m_taskCoordinator->updateTaskState(taskId, ctx.state);
        }
    }

    QSet<QString> newSessionTaskIds = m_taskCoordinator->sessionTaskIds(newSessionId);
    for (const QString& taskId : newSessionTaskIds) {
        TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
        if (ctx.taskId.isEmpty()) {
            continue;
        }

        if (ctx.priority != TaskPriority::UserInteractive) {
            ctx.priority = TaskPriority::UserInteractive;
            if (m_taskContexts.contains(taskId)) {
                m_taskContexts[taskId].priority = TaskPriority::UserInteractive;
            }
            m_taskCoordinator->updateTaskState(taskId, ctx.state);
        }
    }
}


void ProcessingController::onTaskOrphaned(const QString& taskId)
{
    handleOrphanedTask(taskId);
}

void ProcessingController::onResourcePressureChanged(int pressureLevel)
{
    m_resourcePressure = pressureLevel;
    emit resourcePressureChanged();
    
    if (pressureLevel == 2) {
        pauseQueue();
    } else if (pressureLevel == 0 && m_queueStatus == QueueStatus::Paused) {
        resumeQueue();
    }
}

bool ProcessingController::tryStartTask(QueueTask& task)
{
    if (m_taskCoordinator->isOrphaned(task.taskId)) {
        handleOrphanedTask(task.taskId);
        return false;
    }

    if (m_taskMessages.contains(task.taskId)) {
        const Message& message = m_taskMessages[task.taskId];
        if (message.mode == ProcessingMode::AIInference && m_aiEnginePool->availableCount() <= 0) {
            return false;
        }
    }
    
    TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
    
    if (!m_resourceManager->canStartNewTask(ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB)) {
        return false;
    }
    
    if (!m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB)) {
        return false;
    }
    
    startTask(task);
    return true;
}

void ProcessingController::finalizeTask(const QString& taskId, const QString& sessionId, const QString& messageId)
{
    cleanupTask(taskId);
    m_currentProcessingCount--;

    emit currentProcessingCountChanged();
    syncModelTaskById(taskId);
    syncMessageProgress(messageId, sessionId);
    syncMessageStatus(messageId, sessionId);
    
    if (m_sessionController) {
        m_sessionController->saveSessionsImmediately();
    }
    processNextTask();
}

void ProcessingController::gracefulCancel(const QString& taskId, int timeoutMs)
{
    Q_UNUSED(timeoutMs);

    m_taskCoordinator->requestCancellation(taskId);

    QueueTask* targetTask = getTask(taskId);
    if (targetTask && targetTask->status == ProcessingStatus::Processing) {
        if (m_taskMessages.contains(taskId)) {
            const Message& msg = m_taskMessages[taskId];
            if (msg.mode == ProcessingMode::AIInference) {
                if (m_aiEngine) {
                    m_aiEngine->cancelProcess();
                }
                AIEngine* poolEngine = m_aiEnginePool->engineForTask(taskId);
                if (poolEngine && poolEngine != m_aiEngine) {
                    poolEngine->cancelProcess();
                }
            }
        }

        targetTask->status = ProcessingStatus::Cancelled;
        emit taskCancelled(taskId);

        const QString sessionId = resolveSessionIdForMessage(targetTask->messageId);
        updateFileStatusForSessionMessage(sessionId, targetTask->messageId, targetTask->fileId,
            ProcessingStatus::Cancelled, QString());
        syncMessageProgress(targetTask->messageId, sessionId);
        syncMessageStatus(targetTask->messageId, sessionId);

        cleanupTask(taskId);
        if (m_currentProcessingCount > 0) {
            m_currentProcessingCount--;
            emit currentProcessingCountChanged();
        }

        syncModelTaskById(taskId);
        processNextTask();
        return;
    }

    cleanupTask(taskId);
}

void ProcessingController::handleOrphanedTask(const QString& taskId)
{
    bool wasProcessing = false;
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].taskId == taskId) {
            wasProcessing = (m_tasks[i].status == ProcessingStatus::Processing);
            m_tasks[i].status = ProcessingStatus::Cancelled;
            m_tasks.removeAt(i);
            break;
        }
    }
    
    cleanupTask(taskId);
    
    // 如果孤儿任务是正在处理的任务，需要减少计数器
    if (wasProcessing && m_currentProcessingCount > 0) {
        m_currentProcessingCount--;
        emit currentProcessingCountChanged();
    }
    
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
    
    // 触发下一个任务处理
    processNextTask();
}

void ProcessingController::cancelVideoProcessing(const QString& taskId)
{
    if (m_activeVideoProcessors.contains(taskId)) {
        auto processor = m_activeVideoProcessors[taskId];
        if (processor) {
            processor->cancel();
        }
        m_activeVideoProcessors.remove(taskId);
    }
}

void ProcessingController::cleanupTask(const QString& taskId)
{
    // 【关键修复】从 TaskStateManager 注销活动任务
    TaskStateManager::instance()->unregisterActiveTask(taskId);
    
    cancelVideoProcessing(taskId);
    
    if (m_activeImageProcessors.contains(taskId)) {
        auto processor = m_activeImageProcessors.take(taskId);
        if (processor) {
            processor->cancel();
        }
    }
    
    m_resourceManager->release(taskId);
    m_taskCoordinator->unregisterTask(taskId);
    m_pendingExports.remove(taskId);
    m_taskContexts.remove(taskId);
    m_lastReportedTaskProgress.remove(taskId);
    m_lastTaskProgressUpdateMs.remove(taskId);
    m_taskMessages.remove(taskId);
    m_pendingModelLoadTaskIds.remove(taskId);

    disconnectAiEngineForTask(taskId);
    m_aiEnginePool->release(taskId);
}

void ProcessingController::connectAiEngineForTask(AIEngine* engine, const QString& taskId)
{
    QList<QMetaObject::Connection> conns;

    conns << connect(engine, &AIEngine::progressChanged, this, [this, taskId](double progress) {
        for (auto& task : m_tasks) {
            if (task.taskId == taskId && task.status == ProcessingStatus::Processing) {
                updateTaskProgress(taskId, static_cast<int>(progress * 100));
                return;
            }
        }
    });

    conns << connect(engine, &AIEngine::processFileCompleted, this,
            [this, taskId](bool success, const QString& resultPath, const QString& error) {
        if (success) {
            QFileInfo resultInfo(resultPath);
            if (resultPath.isEmpty() || !resultInfo.exists() || resultInfo.size() <= 0) {
                qWarning() << "[ProcessingController][AI] invalid output file"
                           << "task:" << taskId
                           << "result:" << resultPath
                           << "exists:" << resultInfo.exists()
                           << "size:" << resultInfo.size();
                failTask(taskId, tr("AI推理结果无效或未生成输出文件"));
                return;
            }
            completeTask(taskId, resultPath);
        } else {
            failTask(taskId, error);
        }
    });

    m_aiEngineConnections[taskId] = conns;
}

void ProcessingController::disconnectAiEngineForTask(const QString& taskId)
{
    if (!m_aiEngineConnections.contains(taskId)) {
        return;
    }
    for (const auto& conn : m_aiEngineConnections[taskId]) {
        disconnect(conn);
    }
    m_aiEngineConnections.remove(taskId);
}

qint64 ProcessingController::estimateMemoryUsage(const QString& filePath, MediaType type) const
{
    return m_resourceManager->estimateMemoryUsage(filePath, type);
}

void ProcessingController::registerTaskContext(const QString& taskId, const QString& messageId,
                                                const QString& sessionId, const QString& fileId,
                                                qint64 estimatedMemoryMB)
{
    TaskContext ctx(taskId, messageId, sessionId, fileId);
    ctx.estimatedMemoryMB = estimatedMemoryMB;
    ctx.state = TaskState::Pending;
    
    m_taskContexts[taskId] = ctx;
    m_taskCoordinator->registerTask(ctx);
}

void ProcessingController::launchAiInference(AIEngine* engine, const QString& taskId, 
                                              const QString& inputPath, const QString& outputPath, 
                                              const Message& message)
{
    if (!engine || taskId.isEmpty()) {
        failTask(taskId, tr("无效的推理参数"));
        return;
    }

    const ModelInfo modelInfo = m_modelRegistry->getModelInfo(message.aiParams.modelId);
    engine->setUseGpu(message.aiParams.useGpu);
    engine->clearParameters();
    if (message.aiParams.tileSize > 0) {
        engine->setParameter("tileSize", message.aiParams.tileSize);
    }
    for (auto it = message.aiParams.modelParams.begin();
         it != message.aiParams.modelParams.end(); ++it) {
        engine->setParameter(it.key(), it.value());
    }

    const QVariantMap effectiveParams = engine->getEffectiveParams();

    const bool isVideo = ImageUtils::isVideoFile(inputPath);
    if (isVideo) {
        launchAIVideoProcessor(engine, taskId, inputPath, outputPath, message, modelInfo, effectiveParams);
    } else {
        engine->processAsync(inputPath, outputPath);
    }
}

void ProcessingController::launchAIVideoProcessor(AIEngine* engine, const QString& taskId,
                                                   const QString& inputPath, const QString& outputPath,
                                                   const Message& message, const ModelInfo& modelInfo,
                                                   const QVariantMap& effectiveParams)
{
    auto processor = QSharedPointer<AIVideoProcessor>::create();
    processor->setAIEngine(engine);
    processor->setModelInfo(modelInfo);
    
    VideoProcessingConfig config;
    config.tileSize = effectiveParams.value("tileSize", modelInfo.tileSize).toInt();
    config.outscale = effectiveParams.value("outscale", modelInfo.scaleFactor).toDouble();
    config.useGpu = message.aiParams.useGpu && engine->gpuAvailable();
    config.quality = 18;
    processor->setConfig(config);
    
    connect(processor.data(), &AIVideoProcessor::progressChanged,
            this, [this, taskId](double progress) {
        updateTaskProgress(taskId, static_cast<int>(progress * 100));
    }, Qt::QueuedConnection);
    
    connect(processor.data(), &AIVideoProcessor::completed,
            this, [this, taskId, engine](bool success, const QString& result, const QString& error) {
        m_activeAIVideoProcessors.remove(taskId);
        m_aiEnginePool->release(taskId);
        disconnectAiEngineForTask(taskId);
        
        if (success) {
            completeTask(taskId, result);
        } else {
            failTask(taskId, error);
        }
    }, Qt::QueuedConnection);
    
    connect(processor.data(), &AIVideoProcessor::warning,
            this, [this, taskId](const QString& warning) {
        qWarning() << "[ProcessingController][AI] video processing warning:" << warning
                   << "task:" << taskId;
    }, Qt::QueuedConnection);
    
    m_activeAIVideoProcessors[taskId] = processor;
    processor->processAsync(inputPath, outputPath);
}

QVariantMap ProcessingController::getTaskProgress(const QString& taskId) const
{
    if (!m_progressManager || !m_progressManager->hasProvider(taskId)) {
        QVariantMap result;
        result[QStringLiteral("progress")] = 0;
        result[QStringLiteral("stage")] = QString();
        result[QStringLiteral("estimatedRemainingSec")] = -1;
        result[QStringLiteral("isValid")] = false;
        return result;
    }
    
    return m_progressManager->getProgressInfo(taskId);
}

QVariantMap ProcessingController::getMessageProgress(const QString& messageId) const
{
    QStringList taskIds;
    
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId) {
            taskIds.append(task.taskId);
        }
    }
    
    if (taskIds.isEmpty() || !m_progressManager) {
        QVariantMap result;
        result[QStringLiteral("progress")] = 0;
        result[QStringLiteral("stage")] = QString();
        result[QStringLiteral("estimatedRemainingSec")] = -1;
        result[QStringLiteral("isValid")] = false;
        return result;
    }
    
    ProgressManager::ProgressInfo info = m_progressManager->getAggregatedProgress(taskIds);
    QVariantMap result;
    result[QStringLiteral("progress")] = info.progress;
    result[QStringLiteral("stage")] = info.stage;
    result[QStringLiteral("estimatedRemainingSec")] = info.estimatedRemainingSec;
    result[QStringLiteral("isValid")] = info.isValid;
    return result;
}

} // namespace EnhanceVision
