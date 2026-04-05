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
#include "EnhanceVision/services/AutoSaveService.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include "EnhanceVision/core/TaskCoordinator.h"
#include "EnhanceVision/core/ResourceManager.h"
#include "EnhanceVision/core/AIEnginePool.h"
#include "EnhanceVision/core/inference/InferenceTypes.h"
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
    , m_sessionSyncHelper(new SessionSyncHelper(this))
    , m_progressSyncHelper(new ProgressSyncHelper(this))
{
    m_threadPool->setMaxThreadCount(1);
    m_threadPool->setThreadPriority(QThread::LowPriority);
    
    connect(m_taskCoordinator, &TaskCoordinator::taskOrphaned,
            this, &ProcessingController::onTaskOrphaned);
    
    connect(m_resourceManager, &ResourceManager::resourcePressureChanged,
            this, &ProcessingController::onResourcePressureChanged);
    
    connect(m_resourceManager, &ResourceManager::resourceExhausted,
            this, &ProcessingController::resourceExhausted);
    
    m_resourceManager->startMonitoring(2000);

    m_progressSyncHelper->setSessionSyncHelper(m_sessionSyncHelper);

    m_modelRegistry = new ModelRegistry(this);
    m_aiEnginePool = new AIEnginePool(1, m_modelRegistry, this);
    m_aiEngine = nullptr;
    
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
}

ProcessingController::~ProcessingController()
{
    cancelAllTasks();
    // m_aiEngine 已废弃，使用 m_aiEnginePool 管理引擎
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
    // 返回池中第一个可用引擎（如果池非空）
    if (m_aiEnginePool && m_aiEnginePool->poolSize() > 0) {
        return m_aiEnginePool->firstEngine();
    }
    return nullptr;
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
    m_sessionSyncHelper->setMessageModel(messageModel);
    m_progressSyncHelper->setMessageModel(messageModel);
}

void ProcessingController::setSessionController(SessionController* sessionController)
{
    m_sessionController = sessionController;
    m_sessionSyncHelper->setSessionController(sessionController);
    m_progressSyncHelper->setSessionController(sessionController);
}

void ProcessingController::cancelTask(const QString& taskId)
{
    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
        qInfo() << "[ProcessingController] cancelTask ignored for"
                << taskId << "lifecycle:" << static_cast<int>(lifecycle);
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
        return;
    }

    qWarning() << "[ProcessingController] cancelTask: task not found:" << taskId;
}

void ProcessingController::cancelAllTasks()
{
    CancelResult total;
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        CancelResult r = cancelAndRemoveTask(i, CancelMode::ForceProcessing);
        total.cancelledPending += r.cancelledPending;
        total.cancelledProcessing += r.cancelledProcessing;
    }

    adjustProcessingCount(-total.cancelledProcessing);
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
}

void ProcessingController::forceCancelAllTasks()
{
    qWarning() << "[ProcessingController] Force cancelling all tasks"
               << "currentTaskCount:" << m_tasks.size()
               << "processingCount:" << m_currentProcessingCount;

    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Processing) {
            cancelAIEngineForTask(task.taskId);
        }
    }

    if (m_threadPool) {
        m_threadPool->clear();
    }

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QString taskId = m_tasks[i].taskId;
        m_tasks[i].status = ProcessingStatus::Cancelled;
        cleanupTask(taskId);
        emit taskCancelled(taskId);
    }
    m_tasks.clear();

    adjustProcessingCount(-m_currentProcessingCount);

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
        createAndRegisterTask(message, mediaFile, sessionId);
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

bool ProcessingController::hasActiveTaskForFile(const QString& messageId, const QString& fileId) const
{
    if (messageId.isEmpty() || fileId.isEmpty()) {
        return false;
    }

    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && task.fileId == fileId &&
            (task.status == ProcessingStatus::Pending || task.status == ProcessingStatus::Processing)) {
            return true;
        }
    }

    return false;
}

void ProcessingController::removeStaleTasksForFile(const QString& messageId, const QString& fileId)
{
    if (messageId.isEmpty() || fileId.isEmpty()) {
        return;
    }

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QueueTask& task = m_tasks[i];
        if (task.messageId == messageId && task.fileId == fileId &&
            (task.status == ProcessingStatus::Failed ||
             task.status == ProcessingStatus::Cancelled ||
             task.status == ProcessingStatus::Completed)) {
            QString staleTaskId = task.taskId;
            cleanupTask(staleTaskId);
            m_tasks.removeAt(i);
        }
    }

    updateQueuePositions();
    syncModelTasks();
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
    bool hasSkippedAITask = false;
    for (auto& task : m_tasks) {
        if (task.status != ProcessingStatus::Pending) {
            continue;
        }
        
        // 检查 AI 推理引擎池可用性
        if (m_taskMessages.contains(task.taskId)) {
            const Message& msg = m_taskMessages[task.taskId];
            if (msg.mode == ProcessingMode::AIInference && m_aiEnginePool->availableCount() <= 0) {
                hasSkippedAITask = true;
                continue;
            }
        }
        
        nextTask = &task;
        break;
    }

    if (!nextTask) {
        // 【修复】如果有等待 AI 引擎的任务被跳过，安排延迟重试
        // 避免引擎释放延迟导致任务永远无法启动
        if (hasSkippedAITask) {
            QTimer::singleShot(200, this, &ProcessingController::processNextTask);
        }
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
                failTask(taskId, tr("Media file not found"));
                return;
            }
            
            if (ImageUtils::isImageFile(inputPath)) {
                QString processedDir = SettingsController::instance()->getShaderImagePath();
                QDir().mkpath(processedDir);
                QString outputPath = processedDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
                
                auto processor = QSharedPointer<ImageProcessor>::create();
                m_activeImageProcessors[taskId] = processor;
                
                // 捕获 processor 共享指针，防止 terminateTask 移除后 use-after-free
                connect(processor.data(), &ImageProcessor::progressChanged,
                        this, [this, taskId, processor](int progress, const QString&) {
                    Q_UNUSED(processor);  // 保持引用计数
                    updateTaskProgress(taskId, progress);
                }, Qt::QueuedConnection);
                
                connect(processor.data(), &ImageProcessor::finished,
                        this, [this, taskId, processor](bool success, const QString& resultPath, const QString& error) {
                    Q_UNUSED(processor);  // 保持引用计数直到回调完成
                    m_activeImageProcessors.remove(taskId);

                    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
                    if (lifecycle == TaskLifecycle::Canceling ||
                        lifecycle == TaskLifecycle::Draining ||
                        lifecycle == TaskLifecycle::Cleaning ||
                        lifecycle == TaskLifecycle::Dead) {
                        qInfo() << "[ProcessingController][Image] finished signal ignored for"
                                << taskId << "lifecycle:" << static_cast<int>(lifecycle);
                        return;
                    }

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
                failTask(taskId, tr("No AI model specified"));
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
                failTask(taskId, tr("Media file not found"));
                return;
            }

            // 生成输出路径（AI图像和视频分别存储到不同目录）
            const bool isVideo = ImageUtils::isVideoFile(inputPath);
            const QString processedDir = isVideo 
                ? SettingsController::instance()->getAIVideoPath()
                : SettingsController::instance()->getAIImagePath();
            if (!QDir().mkpath(processedDir)) {
                failTask(taskId, tr("Cannot create AI output directory: %1").arg(processedDir));
                return;
            }

            // 视频文件输出为 .mp4，图像文件保持原格式（默认 png）
            QFileInfo fileInfo(inputPath);
            const QString suffix = isVideo ? QStringLiteral("mp4")
                                           : (!fileInfo.suffix().isEmpty() ? fileInfo.suffix() : QStringLiteral("png"));
            const QString uniqueToken = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") +
                                        QStringLiteral("_") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
            const QString outputPath = processedDir + "/" +
                                       fileInfo.completeBaseName() + "_ai_enhanced_" + uniqueToken + "." + suffix;

            // 从引擎池获取可用引擎（根据用户选择的后端类型）
            const BackendType requestedBackend = message.aiParams.useGpu
                ? BackendType::NCNN_Vulkan
                : BackendType::NCNN_CPU;
            AIEngine* engine = m_aiEnginePool->acquireWithBackend(taskId, requestedBackend);
            if (!engine) {
                // 引擎池暂时耗尽，延迟重试而不是立即失败
                qInfo() << "[ProcessingController] AI engine pool temporarily exhausted for task:" << taskId
                        << ", will retry in 500ms";
                // 将任务状态改回 Pending，稍后重试
                for (auto& t : m_tasks) {
                    if (t.taskId == taskId) {
                        t.status = ProcessingStatus::Pending;
                        break;
                    }
                }
                adjustProcessingCount(-1);
                QTimer::singleShot(500, this, &ProcessingController::processNextTask);
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

                    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
                    if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
                        qInfo() << "[ProcessingController][AI] modelLoadCompleted ignored for"
                                << taskId << "lifecycle:" << static_cast<int>(lifecycle);
                        return;
                    }

                    if (!success) {
                        qWarning() << "[ProcessingController][AI] async model load failed"
                                   << "task:" << taskId
                                   << "model:" << loadedModelId
                                   << "error:" << error;
                        failTask(taskId, error.isEmpty() ? tr("Model loading failed: %1").arg(loadedModelId) : error);
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
        TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
        if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
            return;
        }
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
                    syncMessageProgress(task.messageId);
                }
            }
            break;
        }
    }
}

void ProcessingController::completeTask(const QString& taskId, const QString& resultPath)
{
    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
        qInfo() << "[ProcessingController] completeTask ignored for"
                << taskId << "lifecycle:" << static_cast<int>(lifecycle);
        return;
    }

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

                if (!resultPath.isEmpty()) {
                    AutoSaveService::instance()->autoSaveResult(taskId, resultPath);
                }

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
                                    failTask(taskId, tr("AI inference result invalid or output file missing"));
                                    return;
                                }
                                
                                // 【修复】AI 图像推理完成后，同步释放引擎，
                                // 确保 finalizeTask → processNextTask 时引擎已可用
                                disconnectAiEngineForTask(taskId);
                                m_aiEnginePool->release(taskId);
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
                    failTask(taskId, tr("Media file to update not found"));
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

    QString processedDir = SettingsController::instance()->getShaderVideoPath();
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

            TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
            if (lifecycle == TaskLifecycle::Canceling ||
                lifecycle == TaskLifecycle::Draining ||
                lifecycle == TaskLifecycle::Cleaning ||
                lifecycle == TaskLifecycle::Dead) {
                qInfo() << "[ProcessingController][Shader] video finishCb ignored for"
                        << taskId << "lifecycle:" << static_cast<int>(lifecycle);

                if (lifecycle == TaskLifecycle::Draining) {
                    m_dyingVideoProcessors.remove(taskId);
                    releaseTaskResources(taskId);
                    if (m_currentProcessingCount > 0) { m_currentProcessingCount--; }
                    emit currentProcessingCountChanged();
                    processNextTask();
                }

                return;
            }

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
                    updateErrorForSessionMessage(sessionId, messageId, tr("Video output file is invalid"));
                } else {
                    // 成功后生成缩略图
                    if (ThumbnailProvider::instance()) {
                        const QString thumbId = "processed_" + fileId;
                        ThumbnailProvider::instance()->generateThumbnailAsync(
                            outputPath, thumbId, QSize(512, 512));
                    }
                    
                    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                        ProcessingStatus::Completed, outputPath);
                    
                    AutoSaveService::instance()->autoSaveResult(taskId, outputPath);
                }
            } else {
                updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                    ProcessingStatus::Failed, QString());
                updateErrorForSessionMessage(sessionId, messageId,
                    error.isEmpty() ? tr("Video shader processing failed") : error);
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
    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
        qInfo() << "[ProcessingController] failTask ignored for"
                << taskId << "lifecycle:" << static_cast<int>(lifecycle);
        return;
    }

    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            const QString messageId = task.messageId;
            const QString fileId = task.fileId;
            const QString sessionId = resolveSessionIdForMessage(messageId);

            // 【修复】AI 推理任务失败时，在 finalizeTask 前同步释放引擎
            // 确保后续任务能获取到空闲引擎
            if (m_taskMessages.contains(taskId)) {
                const Message& msg = m_taskMessages[taskId];
                if (msg.mode == ProcessingMode::AIInference) {
                    disconnectAiEngineForTask(taskId);
                    m_aiEnginePool->release(taskId);
                }
            }

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
        message.aiParams.useGpu = params.value("useGpu", false).toBool();
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
    qInfo() << "[ProcessingController] retryFailedFiles called for messageId:" << messageId;
    
    if (!m_messageModel) {
        qWarning() << "[ProcessingController] retryFailedFiles: MessageModel not set";
        return;
    }

    Message message = m_messageModel->messageById(messageId);
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] retryFailedFiles: Message not found:" << messageId;
        return;
    }
    
    qInfo() << "[ProcessingController] retryFailedFiles: Message found, status:" 
            << static_cast<int>(message.status) << "files:" << message.mediaFiles.size();

    QList<MediaFile> filesToRetry;
    for (const auto& file : message.mediaFiles) {
        qInfo() << "[ProcessingController] retryFailedFiles: File" << file.id 
                << "status:" << static_cast<int>(file.status);
        if (file.status == ProcessingStatus::Failed || file.status == ProcessingStatus::Cancelled) {
            if (!hasActiveTaskForFile(messageId, file.id)) {
                filesToRetry.append(file);
            } else {
                qInfo() << "[ProcessingController] retryFailedFiles: Skipping file with active task:" << file.id;
            }
        }
    }

    if (filesToRetry.isEmpty()) {
        qWarning() << "[ProcessingController] retryFailedFiles: No failed files to retry";
        return;
    }
    
    qInfo() << "[ProcessingController] retryFailedFiles: Found" << filesToRetry.size() << "files to retry";

    m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Pending));
    m_messageModel->updateErrorMessage(messageId, QString());

    for (const auto& file : filesToRetry) {
        m_messageModel->updateFileStatus(messageId, file.id,
            static_cast<int>(ProcessingStatus::Pending), QString());
    }

    const QString sessionId = resolveSessionIdForMessage(messageId);
    
    for (const auto& file : filesToRetry) {
        removeStaleTasksForFile(messageId, file.id);
        createAndRegisterTask(message, file, sessionId);
    }

    updateQueuePositions();
    emit queueSizeChanged();

    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
}

void ProcessingController::handleOrphanedVideoTask(const QString& taskId)
{
    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Draining || lifecycle == TaskLifecycle::Cleaning || lifecycle == TaskLifecycle::Dead) {
        return;
    }

    qWarning() << "[ProcessingController] Orphaned video task cleanup:" << taskId;

    auto processor = m_activeAIVideoProcessors.take(taskId);
    if (!processor) {
        processor = m_dyingProcessors.take(taskId);
    }
    if (processor) {
        processor->cancel();
    }

    disconnectAiEngineForTask(taskId);

    m_aiEnginePool->release(taskId);

    m_taskLifecycle[taskId] = TaskLifecycle::Dead;
    m_dyingProcessors.remove(taskId);

    adjustProcessingCount(-1);

    qInfo() << "[ProcessingController] Orphaned video task cleanup completed:" << taskId;
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

    if (hasActiveTaskForFile(messageId, file.id)) {
        qWarning() << "[ProcessingController] retrySingleFailedFile: File already has active task:" << file.id;
        return;
    }

    removeStaleTasksForFile(messageId, file.id);
    
    if (message.status == ProcessingStatus::Failed || message.status == ProcessingStatus::Cancelled) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Pending));
    }
    m_messageModel->updateErrorMessage(messageId, QString());
    
    m_messageModel->updateFileStatus(messageId, file.id,
        static_cast<int>(ProcessingStatus::Pending), QString());
    
    createAndRegisterTask(message, file, sessionId);

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

    Message message = m_sessionController->messageInSession(targetSessionId, messageId);
    if (message.id.isEmpty()) {
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

    message.status = ProcessingStatus::Pending;
    message.errorMessage.clear();
    for (auto& file : message.mediaFiles) {
        if (file.status == ProcessingStatus::Pending || file.status == ProcessingStatus::Processing) {
            file.status = ProcessingStatus::Pending;
        }
    }
    m_sessionController->updateMessageInSession(targetSessionId, message);

    if (m_messageModel && m_sessionController->activeSessionId() == targetSessionId) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Pending));
        m_messageModel->updateErrorMessage(messageId, QString());
        for (const auto& file : filesToRetry) {
            m_messageModel->updateFileStatus(messageId, file.id,
                static_cast<int>(ProcessingStatus::Pending), QString());
        }
    }

    for (const auto& file : filesToRetry) {
        createAndRegisterTask(message, file, targetSessionId);
    }

    updateQueuePositions();
    emit queueSizeChanged();

    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
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
    return m_progressSyncHelper->resolveSessionIdForMessage(messageId, fallbackSessionId, m_taskContexts);
}

Message ProcessingController::messageForSession(const QString& sessionId, const QString& messageId) const
{
    return m_progressSyncHelper->messageForSession(sessionId, messageId);
}

void ProcessingController::updateFileStatusForSessionMessage(const QString& sessionId, const QString& messageId,
                                                             const QString& fileId, ProcessingStatus status,
                                                             const QString& resultPath)
{
    m_progressSyncHelper->updateFileStatusForSessionMessage(sessionId, messageId, fileId, status, resultPath);
}

void ProcessingController::updateErrorForSessionMessage(const QString& sessionId, const QString& messageId,
                                                        const QString& errorMessage)
{
    m_progressSyncHelper->updateErrorForSessionMessage(sessionId, messageId, errorMessage);
}

void ProcessingController::updateProgressForSessionMessage(const QString& sessionId, const QString& messageId, int progress)
{
    m_progressSyncHelper->updateProgressForSessionMessage(sessionId, messageId, progress);
}

void ProcessingController::updateStatusForSessionMessage(const QString& sessionId, const QString& messageId, ProcessingStatus status)
{
    m_progressSyncHelper->updateStatusForSessionMessage(sessionId, messageId, status);
}

void ProcessingController::updateQueuePositionForSessionMessage(const QString& sessionId, const QString& messageId, int queuePosition)
{
    m_progressSyncHelper->updateQueuePositionForSessionMessage(sessionId, messageId, queuePosition);
}

void ProcessingController::requestSessionSync()
{
    m_sessionSyncHelper->requestSessionSync();
}

void ProcessingController::syncMessageProgress(const QString& messageId, const QString& sessionId)
{
    m_progressSyncHelper->syncMessageProgress(messageId, sessionId, m_tasks, m_taskContexts);
}

void ProcessingController::syncMessageStatus(const QString& messageId, const QString& sessionId)
{
    m_progressSyncHelper->syncMessageStatus(messageId, sessionId, m_tasks, m_taskContexts);
}

int ProcessingController::resourcePressure() const
{
    return m_resourcePressure;
}

void ProcessingController::cancelMessageTasks(const QString& messageId)
{
    m_taskCoordinator->cancelMessageTasks(messageId);
    
    CancelResult total;
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].messageId == messageId) {
            CancelResult r = cancelAndRemoveTask(i, CancelMode::ForceProcessing);
            total.cancelledPending += r.cancelledPending;
            total.cancelledProcessing += r.cancelledProcessing;
        }
    }
    
    adjustProcessingCount(-total.cancelledProcessing);
    
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
    
    if (m_messageModel) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Cancelled));
    }
    
    emit messageTasksCancelled(messageId);
    
    QTimer::singleShot(500, this, &ProcessingController::processNextTask);
}

void ProcessingController::cancelMessageFileTasks(const QString& messageId, const QString& fileId)
{
    if (messageId.isEmpty() || fileId.isEmpty()) {
        return;
    }

    QString dedupeKey = messageId + ":" + fileId;
    if (m_pendingCancelKeys.contains(dedupeKey)) {
        return;
    }
    m_pendingCancelKeys.insert(dedupeKey);

    DeletionBatchItem item;
    item.fileId = fileId;

    m_deletionBatches[messageId].append(item);

    if (!m_batchProcessTimer) {
        m_batchProcessTimer = new QTimer(this);
        m_batchProcessTimer->setSingleShot(true);
        connect(m_batchProcessTimer, &QTimer::timeout, this, [this]() {
            QStringList messageIds = m_deletionBatches.keys();
            for (const QString& mid : messageIds) {
                processDeletionBatch(mid);
            }
            m_deletionBatches.clear();
        });
    }

    m_batchProcessTimer->start(150);
}

void ProcessingController::processDeletionBatch(const QString& messageId)
{
    if (!m_deletionBatches.contains(messageId)) {
        return;
    }

    QList<DeletionBatchItem> batch = m_deletionBatches.take(messageId);
    QSet<QString> batchFileIds;
    for (const auto& item : batch) {
        batchFileIds.insert(item.fileId);
    }

    qInfo() << "[ProcessingController] processDeletionBatch:" << messageId
            << "fileCount:" << batchFileIds.size();

    // 收集需要终止的任务及其状态
    struct TaskToTerminate {
        QString taskId;
        bool wasProcessing = false;
    };
    QList<TaskToTerminate> tasksToTerminate;

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QueueTask& task = m_tasks[i];
        if (task.messageId != messageId || !batchFileIds.contains(task.fileId)) {
            continue;
        }

        TaskToTerminate tt;
        tt.taskId = task.taskId;
        tt.wasProcessing = (task.status == ProcessingStatus::Processing);
        tasksToTerminate.append(tt);
        m_taskCoordinator->requestCancellation(task.taskId);
    }

    // 终止所有任务（设置 lifecycle 状态、取消处理器等）
    for (const auto& tt : tasksToTerminate) {
        terminateTask(tt.taskId, "batch-delete");
    }

    // 从任务列表移除，并统计需要调整的处理计数
    int removedCount = 0;
    int processingRemoved = 0;
    QSet<QString> terminateIds;
    for (const auto& tt : tasksToTerminate) {
        terminateIds.insert(tt.taskId);
    }

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (terminateIds.contains(m_tasks[i].taskId)) {
            if (m_tasks[i].status == ProcessingStatus::Processing) {
                processingRemoved++;
            }
            m_tasks[i].status = ProcessingStatus::Cancelled;
            emit taskCancelled(m_tasks[i].taskId);
            m_tasks.removeAt(i);
            removedCount++;
        }
    }

    // 对所有被终止的任务立即释放引擎和资源
    // （不仅是 Draining，Canceling/Cleaning 的异步释放可能来不及在 processNextTask 前完成）
    for (const auto& tt : tasksToTerminate) {
        TaskLifecycle lifecycle = m_taskLifecycle.value(tt.taskId, TaskLifecycle::Active);
        if (lifecycle == TaskLifecycle::Draining) {
            // Draining 的处理器已移到 dying 列表，清理它们
            m_dyingProcessors.remove(tt.taskId);
            m_dyingVideoProcessors.remove(tt.taskId);
        }
        // 所有被终止的任务都立即释放引擎（确保下一个任务能获取到空闲引擎）
        m_aiEnginePool->release(tt.taskId);
        releaseTaskResources(tt.taskId);
        m_taskLifecycle[tt.taskId] = TaskLifecycle::Dead;
    }

    // 所有被移除的处理中任务都需要减少计数
    if (processingRemoved > 0) {
        adjustProcessingCount(-processingRemoved);
    }

    if (removedCount > 0) {
        updateQueuePositions();
        syncModelTasks();
        emit queueSizeChanged();
    }

    for (const auto& item : batch) {
        QString dedupeKey = messageId + ":" + item.fileId;
        m_pendingCancelKeys.remove(dedupeKey);
    }

    QTimer::singleShot(300, this, &ProcessingController::processNextTask);
}

void ProcessingController::cancelSessionTasks(const QString& sessionId)
{
    m_taskCoordinator->cancelSessionTasks(sessionId);
    
    QSet<QString> messageIds;
    CancelResult total;
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        QString taskId = m_tasks[i].taskId;
        TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
        
        if (ctx.sessionId == sessionId) {
            messageIds.insert(m_tasks[i].messageId);
            CancelResult r = cancelAndRemoveTask(i, CancelMode::ForceProcessing);
            total.cancelledPending += r.cancelledPending;
            total.cancelledProcessing += r.cancelledProcessing;
        }
    }
    
    adjustProcessingCount(-total.cancelledProcessing);
    
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
    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Draining || lifecycle == TaskLifecycle::Cleaning || lifecycle == TaskLifecycle::Dead) {
        qInfo() << "[ProcessingController] finalizeTask skipped (already cleaning/dead):"
                << taskId << "lifecycle:" << static_cast<int>(lifecycle);
        return;
    }

    cleanupTask(taskId);

    if (m_currentProcessingCount > 0) {
        m_currentProcessingCount--;
    }

    emit currentProcessingCountChanged();
    syncModelTaskById(taskId);

    if (!messageId.isEmpty() && !sessionId.isEmpty()) {
        syncMessageProgress(messageId, sessionId);
        syncMessageStatus(messageId, sessionId);
    }

    if (m_sessionController) {
        m_sessionController->saveSessionsImmediately();
    }
    processNextTask();
}

void ProcessingController::gracefulCancel(const QString& taskId, int timeoutMs)
{
    Q_UNUSED(timeoutMs);

    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
        qInfo() << "[ProcessingController] gracefulCancel ignored for"
                << taskId << "lifecycle:" << static_cast<int>(lifecycle);
        return;
    }

    m_taskCoordinator->requestCancellation(taskId);

    QueueTask* targetTask = getTask(taskId);
    if (targetTask && targetTask->status == ProcessingStatus::Processing) {
        cancelAIEngineForTask(taskId);

        targetTask->status = ProcessingStatus::Cancelled;
        emit taskCancelled(taskId);

        const QString sessionId = resolveSessionIdForMessage(targetTask->messageId);
        updateFileStatusForSessionMessage(sessionId, targetTask->messageId, targetTask->fileId,
            ProcessingStatus::Cancelled, QString());
        syncMessageProgress(targetTask->messageId, sessionId);
        syncMessageStatus(targetTask->messageId, sessionId);

        cleanupTask(taskId);
        adjustProcessingCount(-1);

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
    
    if (wasProcessing) {
        adjustProcessingCount(-1);
    }
    
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
    
    processNextTask();
}

void ProcessingController::cancelVideoProcessing(const QString& taskId)
{
    QMutexLocker locker(&m_cancelMutex);

    if (m_cancellingTaskIds.contains(taskId)) {
        return;
    }
    m_cancellingTaskIds.insert(taskId);

    if (m_activeVideoProcessors.contains(taskId)) {
        auto processor = m_activeVideoProcessors.take(taskId);
        if (processor) {
            processor->cancel();
        }
    }

    m_cancellingTaskIds.remove(taskId);
}

void ProcessingController::releaseTaskResources(const QString& taskId)
{
    m_resourceManager->release(taskId);
    m_taskCoordinator->unregisterTask(taskId);
    m_pendingExports.remove(taskId);
    m_taskContexts.remove(taskId);
    m_lastReportedTaskProgress.remove(taskId);
    m_lastTaskProgressUpdateMs.remove(taskId);
    m_taskMessages.remove(taskId);
    m_pendingModelLoadTaskIds.remove(taskId);
    m_cleaningUpTaskIds.remove(taskId);
    m_taskLifecycle[taskId] = TaskLifecycle::Dead;
}

void ProcessingController::terminateTask(const QString& taskId, const QString& reason)
{
    TaskLifecycle state = m_taskLifecycle.value(taskId, TaskLifecycle::Active);

    if (state == TaskLifecycle::Cleaning || state == TaskLifecycle::Dead) {
        return;
    }

    if (state == TaskLifecycle::Canceling || state == TaskLifecycle::Draining) {
        return;
    }

    bool hasActiveAiVideo = m_activeAIVideoProcessors.contains(taskId);
    bool hasActiveVideo = m_activeVideoProcessors.contains(taskId);

    if (hasActiveAiVideo || hasActiveVideo) {
        m_taskLifecycle[taskId] = TaskLifecycle::Draining;

        qInfo() << "[ProcessingController] terminateTask: DRAINING"
                << taskId << "reason:" << reason
                << "aiVideo:" << hasActiveAiVideo << "video:" << hasActiveVideo;

        disconnectAiEngineForTask(taskId);
        TaskStateManager::instance()->unregisterActiveTask(taskId);

        if (hasActiveAiVideo) {
            auto aiProc = m_activeAIVideoProcessors.take(taskId);
            if (aiProc) {
                aiProc->cancel();  // 确保处理器被取消
                m_dyingProcessors[taskId] = aiProc;
            }
        }

        if (hasActiveVideo) {
            auto vidProc = m_activeVideoProcessors.take(taskId);
            if (vidProc) {
                vidProc->cancel();  // 确保处理器被取消
                m_dyingVideoProcessors[taskId] = vidProc;
            }
        }

        if (m_activeImageProcessors.contains(taskId)) {
            auto imgProcessor = m_activeImageProcessors.take(taskId);
            if (imgProcessor) { imgProcessor->cancel(); }
        }

        return;
    }

    m_taskLifecycle[taskId] = TaskLifecycle::Canceling;

    qInfo() << "[ProcessingController] terminateTask:" << taskId << "reason:" << reason;

    // 取消 AI 引擎处理（在断开连接前，确保引擎停止）
    AIEngine* engine = m_aiEnginePool->engineForTask(taskId);
    if (engine && engine->isProcessing()) {
        qInfo() << "[ProcessingController] Cancelling AI engine for task:" << taskId;
        engine->cancelProcess();
    }

    disconnectAiEngineForTask(taskId);

    if (m_activeAIVideoProcessors.contains(taskId)) {
        auto processor = m_activeAIVideoProcessors.take(taskId);
        if (processor) {
            processor->cancel();
            m_dyingProcessors[taskId] = processor;
        }
    }

    cancelVideoProcessing(taskId);

    if (m_activeImageProcessors.contains(taskId)) {
        auto imgProcessor = m_activeImageProcessors.take(taskId);
        if (imgProcessor) { imgProcessor->cancel(); }
    }

    TaskStateManager::instance()->unregisterActiveTask(taskId);

    m_taskLifecycle[taskId] = TaskLifecycle::Cleaning;

    QTimer::singleShot(0, this, [this, taskId]() {
        m_aiEnginePool->release(taskId);

        if (m_orphanTimers.contains(taskId)) {
            QTimer* oldTimer = m_orphanTimers.take(taskId);
            if (oldTimer) {
                oldTimer->stop();
                oldTimer->deleteLater();
            }
        }

        releaseTaskResources(taskId);

        qInfo() << "[ProcessingController] terminateTask completed:" << taskId;
    });
}

void ProcessingController::cleanupTask(const QString& taskId)
{
    terminateTask(taskId, "cleanupTask");
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
    }, Qt::QueuedConnection);

    conns << connect(engine, &AIEngine::processFileCompleted, this,
            [this, taskId](bool success, const QString& resultPath, const QString& error) {
        TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
        if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
            qInfo() << "[ProcessingController][AI] processFileCompleted ignored for"
                    << taskId << "lifecycle:" << static_cast<int>(lifecycle);
            return;
        }

        if (success) {
            QFileInfo resultInfo(resultPath);
            if (resultPath.isEmpty() || !resultInfo.exists() || resultInfo.size() <= 0) {
                qWarning() << "[ProcessingController][AI] invalid output file"
                           << "task:" << taskId
                           << "result:" << resultPath
                           << "exists:" << resultInfo.exists()
                           << "size:" << resultInfo.size();
                failTask(taskId, tr("AI inference result invalid or no output file generated"));
                return;
            }
            completeTask(taskId, resultPath);
        } else {
            failTask(taskId, error);
        }
    }, Qt::QueuedConnection);

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
        failTask(taskId, tr("Invalid inference parameters"));
        return;
    }

    const ModelInfo modelInfo = m_modelRegistry-> getModelInfo(message.aiParams.modelId);
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
    config.useGpu = message.aiParams.useGpu;
    config.quality = 18;
    processor->setConfig(config);
    
    connect(processor.data(), &AIVideoProcessor::progressChanged,
            this, [this, taskId](double progress) {
        updateTaskProgress(taskId, static_cast<int>(progress * 100));
    }, Qt::QueuedConnection);
    
    connect(processor.data(), &AIVideoProcessor::completed,
            this, [this, taskId](bool success, const QString& result, const QString& error) {
        TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);

        if (lifecycle == TaskLifecycle::Canceling ||
            lifecycle == TaskLifecycle::Cleaning ||
            lifecycle == TaskLifecycle::Dead) {
            qInfo() << "[ProcessingController][AI] completed signal ignored for"
                    << taskId << "lifecycle:" << static_cast<int>(lifecycle);
            return;
        }

        if (!m_activeAIVideoProcessors.contains(taskId) && !m_dyingProcessors.contains(taskId)) {
            return;
        }

        m_activeAIVideoProcessors.remove(taskId);
        disconnectAiEngineForTask(taskId);

        if (lifecycle == TaskLifecycle::Draining) {
            qInfo() << "[ProcessingController][AI] completed for DRAINING task"
                    << taskId << "success:" << success;

            m_dyingProcessors.remove(taskId);
            m_aiEnginePool->release(taskId);
            releaseTaskResources(taskId);

            if (m_currentProcessingCount > 0) { m_currentProcessingCount--; }
            emit currentProcessingCountChanged();
            processNextTask();
            return;
        }

        bool taskExists = false;
        for (const auto& t : m_tasks) {
            if (t.taskId == taskId) {
                taskExists = true;
                break;
            }
        }

        if (taskExists) {
            m_aiEnginePool->release(taskId);

            if (success) {
                completeTask(taskId, result);
            } else {
                failTask(taskId, error);
            }
        } else {
            m_aiEnginePool->release(taskId);
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

void ProcessingController::cancelAIEngineForTask(const QString& taskId)
{
    if (m_activeAIVideoProcessors.contains(taskId)) {
        auto processor = m_activeAIVideoProcessors.value(taskId);
        if (processor) {
            processor->cancel();
        }
    }
}

void ProcessingController::adjustProcessingCount(int delta)
{
    if (delta == 0) {
        return;
    }
    m_currentProcessingCount = qMax(0, m_currentProcessingCount + delta);
    emit currentProcessingCountChanged();
}

QString ProcessingController::createAndRegisterTask(const Message& message, const MediaFile& file,
                                                     const QString& sessionId)
{
    QueueTask task;
    task.taskId = generateTaskId();
    task.messageId = message.id;
    task.fileId = file.id;
    task.position = m_tasks.size();
    task.progress = 0;
    task.queuedAt = QDateTime::currentDateTime();
    task.status = ProcessingStatus::Pending;

    m_tasks.append(task);
    m_processingModel->addTask(task);
    m_taskToMessage[task.taskId] = message.id;
    m_taskMessages[task.taskId] = message;

    qint64 estimatedMemory = estimateMemoryUsage(file.filePath, file.type);
    registerTaskContext(task.taskId, message.id, sessionId, file.id, estimatedMemory);

    emit taskAdded(task.taskId);
    return task.taskId;
}

ProcessingController::CancelResult ProcessingController::cancelAndRemoveTask(int index, CancelMode mode)
{
    CancelResult result;

    if (index < 0 || index >= m_tasks.size()) {
        return result;
    }

    const QString taskId = m_tasks[index].taskId;
    const ProcessingStatus oldStatus = m_tasks[index].status;

    if (oldStatus == ProcessingStatus::Pending) {
        m_tasks[index].status = ProcessingStatus::Cancelled;
        terminateTask(taskId, "cancelAndRemoveTask-Pending");
        emit taskCancelled(taskId);
        m_tasks.removeAt(index);
        result.cancelledPending = 1;
    } else if (oldStatus == ProcessingStatus::Processing && mode == CancelMode::ForceProcessing) {
        m_taskCoordinator->requestCancellation(taskId);
        terminateTask(taskId, "cancelAndRemoveTask-Processing");
        m_tasks[index].status = ProcessingStatus::Cancelled;
        emit taskCancelled(taskId);
        m_tasks.removeAt(index);
        result.cancelledProcessing = 1;
    }

    return result;
}

} // namespace EnhanceVision
