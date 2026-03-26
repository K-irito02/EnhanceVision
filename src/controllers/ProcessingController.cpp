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
#include "EnhanceVision/core/VideoProcessor.h"
#include <QUuid>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <algorithm>

namespace EnhanceVision {

namespace {
constexpr qint64 kPerfLogThresholdMs = 8;

inline void logPerfIfSlow(const char* tag, qint64 elapsedMs)
{
    if (elapsedMs >= kPerfLogThresholdMs) {
        qInfo() << "[Perf][ProcessingController]" << tag << "cost:" << elapsedMs << "ms";
    }
}
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
    m_maxConcurrentTasks = SettingsController::instance()->maxConcurrentTasks();
    m_baseMaxConcurrentTasks = m_maxConcurrentTasks;
    m_interactionPriorityMaxConcurrentTasks = m_maxConcurrentTasks;
    m_importBurstMaxConcurrentTasks = m_maxConcurrentTasks;
    m_threadPool->setMaxThreadCount(m_maxConcurrentTasks);
    m_threadPool->setThreadPriority(QThread::LowPriority);

    connect(SettingsController::instance(), &SettingsController::maxConcurrentTasksChanged,
            this, [this]() {
        m_baseMaxConcurrentTasks = SettingsController::instance()->maxConcurrentTasks();
        refreshEffectiveConcurrencyLimit();
    });
    
    connect(m_taskCoordinator, &TaskCoordinator::taskOrphaned,
            this, &ProcessingController::onTaskOrphaned);
    
    connect(m_resourceManager, &ResourceManager::resourcePressureChanged,
            this, &ProcessingController::onResourcePressureChanged);
    
    connect(m_resourceManager, &ResourceManager::resourceExhausted,
            this, &ProcessingController::resourceExhausted);
    
    m_resourceManager->startMonitoring(2000);

    m_sessionSyncTimer = new QTimer(this);
    m_sessionSyncTimer->setSingleShot(true);
    m_sessionSyncTimer->setInterval(1800);
    connect(m_sessionSyncTimer, &QTimer::timeout, this, [this]() {
        if (m_sessionController) {
            QElapsedTimer perfTimer;
            perfTimer.start();
            m_sessionController->syncCurrentMessagesToSession();
            logPerfIfSlow("syncCurrentMessagesToSession", perfTimer.elapsed());
        }
    });

    m_memorySyncTimer = new QTimer(this);
    m_memorySyncTimer->setSingleShot(true);
    m_memorySyncTimer->setInterval(100);
    connect(m_memorySyncTimer, &QTimer::timeout, this, &ProcessingController::flushSessionMemorySync);

    // 初始化 AI 推理引擎和模型注册表
    m_modelRegistry = new ModelRegistry(this);
    m_aiEngine = new AIEngine(this);

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

    // 连接 AI 推理信号
    connect(m_aiEngine, &AIEngine::progressChanged, this, [this](double progress) {
        // 仅更新当前激活的 AI 任务，避免跨会话/跨任务串扰
        if (m_activeAiTaskId.isEmpty()) {
            return;
        }

        for (auto& task : m_tasks) {
            if (task.taskId == m_activeAiTaskId && task.status == ProcessingStatus::Processing) {
                updateTaskProgress(task.taskId, static_cast<int>(progress * 100));
                return;
            }
        }
    });

    connect(m_aiEngine, &AIEngine::videoProcessingWarning, this, [this](const QString& warning) {
        qWarning() << "[ProcessingController][AI] video processing warning:" << warning;
        if (!m_activeAiTaskId.isEmpty() && m_taskMessages.contains(m_activeAiTaskId)) {
            const QString sessionId = resolveSessionIdForMessage(m_taskMessages[m_activeAiTaskId].id);
            updateErrorForSessionMessage(sessionId, m_taskMessages[m_activeAiTaskId].id, warning);
        }
    });

    connect(m_aiEngine, &AIEngine::processFileCompleted, this,
            [this](bool success, const QString& resultPath, const QString& error) {
        QString finishedTaskId = m_activeAiTaskId;

        // 容错恢复：若 activeTaskId 丢失，尝试从当前 processing 的 AI 任务中恢复
        if (finishedTaskId.isEmpty()) {
            QString recoveredTaskId;
            int processingAiCount = 0;
            for (const auto& task : m_tasks) {
                if (task.status != ProcessingStatus::Processing) {
                    continue;
                }
                if (!m_taskMessages.contains(task.taskId)) {
                    continue;
                }
                const Message& msg = m_taskMessages[task.taskId];
                if (msg.mode == ProcessingMode::AIInference) {
                    recoveredTaskId = task.taskId;
                    processingAiCount++;
                    if (processingAiCount > 1) {
                        break;
                    }
                }
            }

            if (processingAiCount == 1 && !recoveredTaskId.isEmpty()) {
                finishedTaskId = recoveredTaskId;
                qWarning() << "[ProcessingController][AI] recovered missing active task id"
                           << "recoveredTask:" << finishedTaskId;
            } else {
                qWarning() << "[ProcessingController][AI] process finished but no active task"
                           << "success:" << success
                           << "result:" << resultPath
                           << "error:" << error
                           << "processingAiCount:" << processingAiCount;
                return;
            }
        }

        if (m_activeAiTaskId == finishedTaskId) {
            m_activeAiTaskId.clear();
        }

        qInfo() << "[ProcessingController][AI] process finished"
                << "task:" << finishedTaskId
                << "success:" << success
                << "result:" << resultPath
                << "error:" << error;

        if (success) {
            QFileInfo resultInfo(resultPath);
            if (resultPath.isEmpty() || !resultInfo.exists() || resultInfo.size() <= 0) {
                qWarning() << "[ProcessingController][AI] invalid output file"
                           << "task:" << finishedTaskId
                           << "result:" << resultPath
                           << "exists:" << resultInfo.exists()
                           << "size:" << resultInfo.size();
                failTask(finishedTaskId, tr("AI推理结果无效或未生成输出文件"));
                return;
            }
            completeTask(finishedTaskId, resultPath);
        } else {
            failTask(finishedTaskId, error);
        }
    });
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

int ProcessingController::maxConcurrentTasks() const
{
    return m_maxConcurrentTasks;
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
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QString taskId = m_tasks[i].taskId;

        if (m_tasks[i].status == ProcessingStatus::Pending) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            emit taskCancelled(taskId);
            m_tasks.removeAt(i);
            continue;
        }

        if (m_tasks[i].status == ProcessingStatus::Processing) {
            gracefulCancel(taskId);
        }
    }

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
}

void ProcessingController::forceCancelAllTasks()
{
    qWarning() << "[ProcessingController] Force cancelling all tasks";

    // 强制取消 AI 推理
    if (m_aiEngine) {
        m_aiEngine->forceCancel();
    }

    // 清空线程池队列
    if (m_threadPool) {
        m_threadPool->clear();
    }

    // 取消所有任务
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QString taskId = m_tasks[i].taskId;

        m_tasks[i].status = ProcessingStatus::Cancelled;
        cleanupTask(taskId);
        emit taskCancelled(taskId);
    }
    m_tasks.clear();

    m_activeAiTaskId.clear();

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();

    qWarning() << "[ProcessingController] All tasks force cancelled";
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

    if (m_modelRegistry->hasModel(message.aiParams.modelId) && !m_preloadedModelIds.contains(message.aiParams.modelId)) {
        requestModelPreload(message.aiParams.modelId);
    }

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

    while (m_currentProcessingCount < m_maxConcurrentTasks) {
        QueueTask* nextTask = nullptr;
        for (auto& task : m_tasks) {
            if (task.status == ProcessingStatus::Pending) {
                if (m_taskMessages.contains(task.taskId)) {
                    const Message& msg = m_taskMessages[task.taskId];
                    if (msg.mode == ProcessingMode::AIInference && !m_activeAiTaskId.isEmpty()) {
                        continue;
                    }
                }
                nextTask = &task;
                break;
            }
        }

        if (!nextTask) {
            break;
        }

        const QString candidateTaskId = nextTask->taskId;
        if (!tryStartTask(*nextTask)) {
            bool stillPending = false;
            for (const auto& task : m_tasks) {
                if (task.taskId == candidateTaskId && task.status == ProcessingStatus::Pending) {
                    stillPending = true;
                    break;
                }
            }

            if (stillPending) {
                break;
            }

            continue;
        }
    }

    bool hasPendingTasks = false;
    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Pending) {
            hasPendingTasks = true;
            break;
        }
    }

    if (!hasPendingTasks && m_currentProcessingCount == 0 && m_importBurstMode) {
        setImportBurstMode(false);
    }
}

QString ProcessingController::generateTaskId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ProcessingController::requestModelPreload(const QString& modelId)
{
    if (modelId.isEmpty() || !m_aiEngine) {
        return;
    }

    if (m_preloadedModelIds.contains(modelId) || m_pendingPreloadModelIds.contains(modelId)) {
        return;
    }

    m_pendingPreloadModelIds.insert(modelId);

    // 预加载必须在主线程执行（m_aiEngine 是主线程 QObject）
    // 使用 QTimer::singleShot 异步但在主线程中调用，避免阻塞当前调用栈
    QTimer::singleShot(0, this, [this, modelId]() {
        if (!m_pendingPreloadModelIds.contains(modelId)) {
            return; // 已被取消或重复
        }

        // AIEngine 推理中禁止 loadModel，这里直接放弃本次预加载，避免产生无效告警。
        if (!m_activeAiTaskId.isEmpty() || m_aiEngine->isProcessing()) {
            m_pendingPreloadModelIds.remove(modelId);
            return;
        }

        const bool loaded = m_aiEngine->loadModel(modelId);
        m_pendingPreloadModelIds.remove(modelId);
        if (loaded) {
            m_preloadedModelIds.insert(modelId);
            qInfo() << "[Perf][ProcessingController] model preloaded:" << modelId;
        }
    });
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
    m_currentProcessingCount++;

    emit taskStarted(task.taskId);
    emit currentProcessingCountChanged();
    syncModelTask(task);

    const QString startedSessionId = resolveSessionIdForMessage(task.messageId);
    updateFileStatusForSessionMessage(startedSessionId, task.messageId, task.fileId,
        ProcessingStatus::Processing, QString());
    syncMessageStatus(task.messageId, startedSessionId);
    syncMessageProgress(task.messageId, startedSessionId);

    QString taskId = task.taskId;
    
    // 检查是否为 AI 推理模式
    if (m_taskMessages.contains(taskId)) {
        const Message& message = m_taskMessages[taskId];
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

            m_activeAiTaskId = taskId;

            // ── 模型加载在主线程完成，然后启动推理工作线程 ──────────────
            // m_aiEngine 是主线程 QObject，不能在 threadPool 线程直接调用其方法。
            // 先在主线程加载模型（通常已预加载，耗时极短），再派发推理到工作线程。
            QElapsedTimer aiStartTimer;
            aiStartTimer.start();

            if (!m_aiEngine->loadModel(modelId)) {
                failTask(taskId, tr("模型加载失败: %1").arg(modelId));
                return;
            }

            const qint64 loadModelCostMs = aiStartTimer.elapsed();
            if (loadModelCostMs >= 16) {
                qInfo() << "[Perf][ProcessingController] startTask loadModel cost:" << loadModelCostMs << "ms"
                        << "model:" << message.aiParams.modelId;
            }

            const ModelInfo modelInfo = m_modelRegistry->getModelInfo(message.aiParams.modelId);
            m_aiEngine->setUseGpu(message.aiParams.useGpu);
            // 先清空参数，再按顺序设置，避免多文件处理时上一个任务的参数残留
            m_aiEngine->clearParameters();
            // tileSize=0 表示自动模式，不强制写入（让 AIEngine 内部自动计算）
            if (message.aiParams.tileSize > 0) {
                m_aiEngine->setParameter("tileSize", message.aiParams.tileSize);
            }
            for (auto it = message.aiParams.modelParams.begin();
                 it != message.aiParams.modelParams.end(); ++it) {
                m_aiEngine->setParameter(it.key(), it.value());
            }

            const QVariantMap effectiveParams = m_aiEngine->getEffectiveParams();
            qInfo() << "[ProcessingController][AI] launch"
                    << "task:" << taskId
                    << "model:" << message.aiParams.modelId
                    << "input:" << inputPath
                    << "output:" << outputPath
                    << "gpuRequested:" << message.aiParams.useGpu
                    << "gpuEffective:" << (m_aiEngine->gpuAvailable() && m_aiEngine->useGpu())
                    << "tileSizeRequested:" << message.aiParams.tileSize
                    << "tileSizeEffective:" << effectiveParams.value("tileSize", modelInfo.tileSize).toInt()
                    << "outscaleEffective:" << effectiveParams.value("outscale", modelInfo.scaleFactor).toDouble();

            // 推理派发到工作线程（processAsync 内部使用 m_inferenceMutex 保证串行）
            m_aiEngine->processAsync(inputPath, outputPath);

            logPerfIfSlow("startTask", perfTimer.elapsed());
            return;
        }
    }

    QTimer::singleShot(10, this, [this, taskId]() {
        completeTask(taskId, "");
    });

    logPerfIfSlow("startTask", perfTimer.elapsed());
}

void ProcessingController::updateTaskProgress(const QString& taskId, int progress)
{
    QElapsedTimer perfTimer;
    perfTimer.start();

    constexpr int kProgressEmitStep = 2;
    constexpr qint64 kProgressEmitIntervalMs = 66;

    const int normalizedProgress = qBound(0, progress, 100);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            const int previousProgress = task.progress;
            task.progress = normalizedProgress;

            const int lastReported = m_lastReportedTaskProgress.value(taskId, -1);
            const qint64 lastUpdateMs = m_lastTaskProgressUpdateMs.value(taskId, 0);

            const bool firstEmit = (lastReported < 0);
            const bool reachedTerminal = (normalizedProgress >= 100);
            const bool crossedStep = (lastReported < 0) || (normalizedProgress - lastReported >= kProgressEmitStep);
            const bool timeoutReached = (nowMs - lastUpdateMs) >= kProgressEmitIntervalMs;

            if (firstEmit || reachedTerminal || crossedStep || timeoutReached || normalizedProgress < lastReported) {
                m_lastReportedTaskProgress[taskId] = normalizedProgress;
                m_lastTaskProgressUpdateMs[taskId] = nowMs;

                emit taskProgress(taskId, normalizedProgress);
                syncModelTask(task);

                if (normalizedProgress != previousProgress) {
                    constexpr qint64 kMessageProgressSyncIntervalMs = 80;
                    const qint64 lastMessageSyncMs = m_lastMessageProgressSyncMs.value(task.messageId, 0);
                    const bool shouldSyncMessageProgress = (normalizedProgress >= 100) ||
                                                           ((nowMs - lastMessageSyncMs) >= kMessageProgressSyncIntervalMs);
                    if (shouldSyncMessageProgress) {
                        syncMessageProgress(task.messageId);
                    }
                }
            }
            break;
        }
    }

    logPerfIfSlow("updateTaskProgress", perfTimer.elapsed());
}

void ProcessingController::completeTask(const QString& taskId, const QString& resultPath)
{
    QElapsedTimer perfTimer;
    perfTimer.start();

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
                logPerfIfSlow("completeTask", perfTimer.elapsed());
                return;
            } else {
                m_tasks[i].status = ProcessingStatus::Completed;
                m_tasks[i].progress = 100;
                emit taskCompleted(taskId, resultPath);

                const Message msg = messageForSession(sessionId, messageId);

                bool fileHandled = false;
                for (const MediaFile& mf : msg.mediaFiles) {
                    if (mf.id == fileId) {
                        if (msg.mode == ProcessingMode::Shader && ImageUtils::isImageFile(mf.filePath)) {
                            QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed";
                            QDir().mkpath(processedDir);
                            QString processedPath = processedDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";

                            PendingExport pending;
                            pending.taskId = taskId;
                            pending.messageId = messageId;
                            pending.sessionId = sessionId;
                            pending.fileId = fileId;
                            pending.originalPath = mf.filePath;
                            pending.outputPath = processedPath;
                            pending.shaderParams = shaderParamsToVariantMap(msg.shaderParams);

                            if (m_taskContexts.contains(taskId)) {
                                pending.estimatedMemoryMB = m_taskContexts[taskId].estimatedMemoryMB;
                                pending.estimatedGpuMemoryMB = m_taskContexts[taskId].estimatedGpuMemoryMB;
                            }

                            m_pendingExports[taskId] = pending;
                            emit requestShaderExport(taskId, mf.filePath, pending.shaderParams, processedPath);
                            fileHandled = true;
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

                            cleanupTask(taskId);
                            m_currentProcessingCount--;
                            emit currentProcessingCountChanged();
                            syncModelTaskById(taskId);

                            syncMessageProgress(messageId, sessionId);
                            syncMessageStatus(messageId, sessionId);
                            if (m_sessionController) {
                                requestSessionSync();
                            }
                            processNextTask();
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

    logPerfIfSlow("completeTask", perfTimer.elapsed());
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

    cleanupTask(exportId);
    m_currentProcessingCount--;
    emit currentProcessingCountChanged();
    syncModelTaskById(exportId);

    syncMessageProgress(pending.messageId, pending.sessionId);
    syncMessageStatus(pending.messageId, pending.sessionId);
    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
}

void ProcessingController::processShaderVideoThumbnailAsync(const QString& taskId,
                                                            const QString& messageId,
                                                            const QString& fileId,
                                                            const QString& filePath,
                                                            const ShaderParams& shaderParams)
{
    const QString sessionId = resolveSessionIdForMessage(messageId);

    // 生成输出路径
    QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed/shader_video";
    QDir().mkpath(processedDir);
    QFileInfo fi(filePath);
    QString outputPath = processedDir + "/" + fi.completeBaseName()
                         + "_shader_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
                         + "." + fi.suffix();

    qInfo() << "[ProcessingController][Shader] launching video processing"
            << "task:" << taskId << "input:" << filePath << "output:" << outputPath;

    // 先在线程池中生成缩略图，然后启动视频处理
    m_threadPool->start([this, taskId, sessionId, messageId, fileId, filePath, outputPath, shaderParams]() {
        // 生成处理后的缩略图（用于消息卡片预览）
        QImage videoThumb = ImageUtils::generateVideoThumbnail(filePath, QSize(512, 512));
        QImage processedThumb;
        if (!videoThumb.isNull()) {
            processedThumb = ImageUtils::applyShaderEffects(
                videoThumb,
                shaderParams.brightness,
                shaderParams.contrast,
                shaderParams.saturation,
                shaderParams.hue,
                shaderParams.exposure,
                shaderParams.gamma,
                shaderParams.temperature,
                shaderParams.tint,
                shaderParams.vignette,
                shaderParams.highlights,
                shaderParams.shadows,
                shaderParams.sharpness,
                shaderParams.blur,
                shaderParams.denoise
            );
        }

        // 设置缩略图（先在主线程设置）
        QMetaObject::invokeMethod(this, [this, fileId, processedThumb]() {
            if (ThumbnailProvider::instance() && !processedThumb.isNull()) {
                const QString thumbId = "processed_" + fileId;
                ThumbnailProvider::instance()->setThumbnail(thumbId, processedThumb);
            }
        }, Qt::QueuedConnection);

        // 实际处理视频：使用 VideoProcessor 逐帧应用 shader 效果
        // processVideoAsync 内部使用 QtConcurrent::run，通过回调接收结果
        auto* videoProcessor = new VideoProcessor();

        auto progressCb = [this, taskId](int progress, const QString&) {
            int mappedProgress = 10 + progress * 85 / 100;
            QMetaObject::invokeMethod(this, [this, taskId, mappedProgress]() {
                updateTaskProgress(taskId, mappedProgress);
            }, Qt::QueuedConnection);
        };

        auto finishCb = [this, videoProcessor, taskId, sessionId, messageId, fileId, filePath, outputPath](
                             bool success, const QString& resultPath, const QString& error) {
            QMetaObject::invokeMethod(this, [this, videoProcessor, taskId, sessionId, messageId,
                                              fileId, filePath, outputPath, success, resultPath, error]() {
                qInfo() << "[ProcessingController][Shader] video processing finished"
                        << "task:" << taskId << "success:" << success
                        << "result:" << (success ? outputPath : "") << "error:" << error;

                videoProcessor->deleteLater();

                if (m_taskCoordinator->isOrphaned(taskId)) {
                    cleanupTask(taskId);
                    m_currentProcessingCount--;
                    emit currentProcessingCountChanged();
                    syncModelTaskById(taskId);
                    processNextTask();
                    return;
                }

                if (success) {
                    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                        ProcessingStatus::Completed, outputPath);
                } else {
                    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                        ProcessingStatus::Failed, QString());
                    updateErrorForSessionMessage(sessionId, messageId,
                        error.isEmpty() ? tr("视频Shader处理失败") : error);
                }

                cleanupTask(taskId);
                m_currentProcessingCount--;
                emit currentProcessingCountChanged();
                syncModelTaskById(taskId);
                syncMessageProgress(messageId, sessionId);
                syncMessageStatus(messageId, sessionId);
                if (m_sessionController) {
                    requestSessionSync();
                }
                processNextTask();
            }, Qt::QueuedConnection);
        };

        videoProcessor->processVideoAsync(filePath, outputPath, shaderParams, progressCb, finishCb);
    });
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

            cleanupTask(taskId);
            m_currentProcessingCount--;
            emit currentProcessingCountChanged();
            syncModelTaskById(taskId);

            if (!m_taskCoordinator->isOrphaned(taskId)) {
                updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                    ProcessingStatus::Failed, QString());
                updateErrorForSessionMessage(sessionId, messageId, error);
            }

            syncMessageProgress(messageId, sessionId);
            syncMessageStatus(messageId, sessionId);

            if (m_sessionController) {
                requestSessionSync();
            }

            processNextTask();
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

    setImportBurstMode(files.size() > 1);

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
    
    Message message = m_messageModel->messageById(messageId);
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
    
    qDebug() << "[ProcessingController] Retrying single file:" << fileIndex << "for message:" << messageId 
             << "current status:" << static_cast<int>(file.status);
    
    if (message.status == ProcessingStatus::Failed || message.status == ProcessingStatus::Pending) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Processing));
    }
    m_messageModel->updateErrorMessage(messageId, QString());
    
    // 重置文件状态
    m_messageModel->updateFileStatus(messageId, file.id,
        static_cast<int>(ProcessingStatus::Pending), QString());
    
    // 添加单个任务
    const QString sessionId = resolveSessionIdForMessage(messageId);
    
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
    
    // 同步到会话
    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
}

void ProcessingController::autoRetryInterruptedFiles(const QString& messageId, const QString& sessionId)
{
    if (!m_messageModel) {
        qWarning() << "[ProcessingController] autoRetryInterruptedFiles: MessageModel not set";
        return;
    }

    Message message = m_messageModel->messageById(messageId);
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] autoRetryInterruptedFiles: Message not found:" << messageId;
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

    m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Processing));
    m_messageModel->updateErrorMessage(messageId, QString());

    const QString targetSessionId = !sessionId.isEmpty()
        ? sessionId
        : resolveSessionIdForMessage(messageId);

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

void ProcessingController::setVisibleSessionUpdateFrozen(bool frozen)
{
    m_freezeVisibleSessionUpdates = frozen;
    if (!m_freezeVisibleSessionUpdates) {
        flushSessionMemorySync();
    }
}

bool ProcessingController::visibleSessionUpdateFrozen() const
{
    return m_freezeVisibleSessionUpdates;
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
    if (!m_sessionController || m_freezeVisibleSessionUpdates) {
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
    if (!m_messageModel || !m_sessionController || sessionId.isEmpty() || m_freezeVisibleSessionUpdates) {
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

    static qint64 s_lastSyncLogMs = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - s_lastSyncLogMs >= 2000) {
        s_lastSyncLogMs = nowMs;
        qInfo() << "[Perf][ProcessingController] requestSessionSync persist debounce trigger 1800ms";
    }
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
    constexpr qint64 kMessageProgressSyncDebounceMs = 100;

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
            if (task.status == ProcessingStatus::Completed) {
                totalProgress += 100;
            } else if (task.status == ProcessingStatus::Failed ||
                       task.status == ProcessingStatus::Cancelled) {
                totalProgress += 100;
            } else {
                totalProgress += task.progress;
            }
        }
    }

    const int overallProgress = (totalTasks > 0) ? (totalProgress / totalTasks) : 0;
    updateProgressForSessionMessage(targetSessionId, messageId, overallProgress);
}

void ProcessingController::syncMessageStatus(const QString& messageId, const QString& sessionId)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kMessageStatusSyncDebounceMs = 120;

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
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].messageId == messageId) {
            if (m_tasks[i].status == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(m_tasks[i].taskId);
                m_tasks.removeAt(i);
            } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                gracefulCancel(m_tasks[i].taskId);
            }
        }
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

    bool removedPendingTask = false;

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].messageId != messageId || m_tasks[i].fileId != fileId) {
            continue;
        }

        const QString taskId = m_tasks[i].taskId;
        m_taskCoordinator->requestCancellation(taskId);

        if (m_tasks[i].status == ProcessingStatus::Pending) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            m_tasks.removeAt(i);
            removedPendingTask = true;
        } else if (m_tasks[i].status == ProcessingStatus::Processing) {
            gracefulCancel(taskId);
        }
    }

    if (removedPendingTask) {
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
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        QString taskId = m_tasks[i].taskId;
        TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
        
        if (ctx.sessionId == sessionId) {
            messageIds.insert(m_tasks[i].messageId);
            
            if (m_tasks[i].status == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(taskId);
                m_tasks.removeAt(i);
            } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                gracefulCancel(taskId);
            }
        }
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
    setVisibleSessionUpdateFrozen(true);

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

    setInteractionPriorityMode(!oldSessionTaskIds.isEmpty() || !newSessionTaskIds.isEmpty());

    QTimer::singleShot(120, this, [this]() {
        setVisibleSessionUpdateFrozen(false);
    });
}

void ProcessingController::setMaxConcurrentTasks(int max)
{
    if (max <= 0) {
        max = 1;
    }

    if (m_baseMaxConcurrentTasks != max) {
        m_baseMaxConcurrentTasks = max;
        refreshEffectiveConcurrencyLimit();
    }
}

void ProcessingController::setInteractionPriorityMode(bool enabled)
{
    if (m_interactionPriorityMode == enabled) {
        return;
    }

    m_interactionPriorityMode = enabled;
    refreshEffectiveConcurrencyLimit();
}

void ProcessingController::setImportBurstMode(bool enabled)
{
    if (m_importBurstMode == enabled) {
        return;
    }

    m_importBurstMode = enabled;
    refreshEffectiveConcurrencyLimit();
}

void ProcessingController::refreshEffectiveConcurrencyLimit()
{
    const int base = qMax(1, m_baseMaxConcurrentTasks);

    int interactionLimit = base;
    if (m_interactionPriorityMode) {
        interactionLimit = qMax(1, base - 1);
    }

    int importLimit = base;
    if (m_importBurstMode) {
        importLimit = qMax(1, base - 1);
    }

    m_interactionPriorityMaxConcurrentTasks = interactionLimit;
    m_importBurstMaxConcurrentTasks = importLimit;

    const int effective = qMax(1, qMin(base, qMin(interactionLimit, importLimit)));
    if (m_maxConcurrentTasks == effective) {
        return;
    }

    m_maxConcurrentTasks = effective;
    m_threadPool->setMaxThreadCount(m_maxConcurrentTasks);
    m_resourceManager->setMaxConcurrentTasks(m_maxConcurrentTasks);
    emit maxConcurrentTasksChanged();
}

void ProcessingController::setResourceQuota(const ResourceQuota& quota)
{
    m_resourceManager->setQuota(quota);
    m_baseMaxConcurrentTasks = qMax(1, quota.maxConcurrentTasks);
    refreshEffectiveConcurrencyLimit();
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
        if (message.mode == ProcessingMode::AIInference && !m_activeAiTaskId.isEmpty()) {
            qInfo() << "[ProcessingController][AI] waiting previous ai task to finish"
                    << "pendingTask:" << task.taskId
                    << "activeTask:" << m_activeAiTaskId;
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

void ProcessingController::gracefulCancel(const QString& taskId, int timeoutMs)
{
    Q_UNUSED(timeoutMs);

    m_taskCoordinator->requestCancellation(taskId);

    QueueTask* targetTask = getTask(taskId);
    if (targetTask && targetTask->status == ProcessingStatus::Processing) {
        // AI 推理任务需要主动通知 AIEngine 取消，否则任务可能持续占用 activeAiTaskId
        if (m_taskMessages.contains(taskId)) {
            const Message& msg = m_taskMessages[taskId];
            if (msg.mode == ProcessingMode::AIInference && m_aiEngine) {
                m_aiEngine->cancelProcess();
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
    cleanupTask(taskId);
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].taskId == taskId) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            m_tasks.removeAt(i);
            break;
        }
    }
    
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
}

void ProcessingController::cleanupTask(const QString& taskId)
{
    m_resourceManager->release(taskId);
    m_taskCoordinator->unregisterTask(taskId);
    m_pendingExports.remove(taskId);
    m_taskContexts.remove(taskId);
    m_lastReportedTaskProgress.remove(taskId);
    m_lastTaskProgressUpdateMs.remove(taskId);
    m_taskMessages.remove(taskId);

    if (m_activeAiTaskId == taskId) {
        m_activeAiTaskId.clear();
    }
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

} // namespace EnhanceVision
