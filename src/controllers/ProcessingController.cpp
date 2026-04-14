/**
 * @file ProcessingController.cpp
 * @brief 处理控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/FileController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/core/ProcessingTimeManager.h"
#include "EnhanceVision/core/TaskTimeEstimator.h"
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
#include <QEventLoop>
#include <QFile>
#include <QThread>
#include <QtConcurrent>
#include <algorithm>

namespace EnhanceVision {

namespace {
constexpr qint64 kPerfLogThresholdMs = 8;

QList<QSharedPointer<AIVideoProcessor>>& shutdownLeakedAiVideoProcessors()
{
    static QList<QSharedPointer<AIVideoProcessor>> leakedProcessors;
    return leakedProcessors;
}

QList<QSharedPointer<VideoProcessor>>& shutdownLeakedVideoProcessors()
{
    static QList<QSharedPointer<VideoProcessor>> leakedProcessors;
    return leakedProcessors;
}

QList<QSharedPointer<ImageProcessor>>& shutdownLeakedImageProcessors()
{
    static QList<QSharedPointer<ImageProcessor>> leakedProcessors;
    return leakedProcessors;
}

void stashProcessorForShutdown(const QSharedPointer<AIVideoProcessor>& processor)
{
    if (processor) {
        shutdownLeakedAiVideoProcessors().append(processor);
    }
}

void stashProcessorForShutdown(const QSharedPointer<VideoProcessor>& processor)
{
    if (processor) {
        shutdownLeakedVideoProcessors().append(processor);
    }
}

void stashProcessorForShutdown(const QSharedPointer<ImageProcessor>& processor)
{
    if (processor) {
        shutdownLeakedImageProcessors().append(processor);
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
    
    connect(m_aiEnginePool, &AIEnginePool::engineReady, this, &ProcessingController::processNextTask, Qt::QueuedConnection);
    
    m_progressManager = std::make_unique<ProgressManager>(this);
    
    // 【资源清理】定期清理已完成的dying处理器，释放内存
    QTimer* dyingProcessorCleanupTimer = new QTimer(this);
    connect(dyingProcessorCleanupTimer, &QTimer::timeout, this, [this]() {
        cleanupDyingProcessors();
    });
    dyingProcessorCleanupTimer->start(5000);  // 每5秒检查一次

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
    prepareForShutdown();
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

bool ProcessingController::prepareForShutdown(int threadPoolWaitMs)
{
    if (m_shutdownInProgress) {
        return waitForThreadPoolIdle(threadPoolWaitMs);
    }

    m_shutdownInProgress = true;
    qInfo() << "[ProcessingController] Preparing for shutdown"
            << "taskCount:" << m_tasks.size()
            << "processingCount:" << m_currentProcessingCount;

    forceCancelAllTasks();

    const bool cleanupOk = waitForBackgroundCleanup(qMin(threadPoolWaitMs, 800));
    const bool threadPoolOk = waitForThreadPoolIdle(threadPoolWaitMs);
    return cleanupOk && threadPoolOk;
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

void ProcessingController::setProcessingTimeManager(ProcessingTimeManager* manager)
{
    m_processingTimeManager = manager;
}

void ProcessingController::cancelTask(const QString& taskId)
{
    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Canceling ||
        lifecycle == TaskLifecycle::Draining ||
        lifecycle == TaskLifecycle::Cleaning ||
        lifecycle == TaskLifecycle::Dead) {
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
    qInfo() << "[ProcessingController] Force cancelling all tasks"
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

    qInfo() << "[ProcessingController] All tasks force cancelled successfully";
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
    
    // 【新增】检查是否需要将新消息设置为等待状态或暂停状态
    int pauseMode = SettingsController::instance()->pauseMode();
    bool shouldWait = false;
    bool shouldPause = false;  // 模式2：默认暂停
    
    if (pauseMode == 1) {  // 顺序暂停模式
        // 检查是否有正在处理的任务
        if (m_currentProcessingCount > 0) {
            shouldWait = true;
        }
        // 检查是否有暂停的消息卡片
        if (!m_pausedMessageIds.isEmpty()) {
            shouldWait = true;
        }
        // 检查是否有其他待处理的消息（FIFO顺序）
        for (const auto& task : m_tasks) {
            if (task.status == ProcessingStatus::Pending || task.status == ProcessingStatus::Paused) {
                if (task.messageId != message.id) {
                    shouldWait = true;
                    break;
                }
            }
        }
    } else if (pauseMode == 2) {
        shouldPause = true;
        m_pausedMessageIds.insert(message.id);
    }
    
    for (const auto& mediaFile : message.mediaFiles) {
        createAndRegisterTask(message, mediaFile, sessionId);
    }

    if (shouldPause) {
        bool firstFilePaused = false;
        for (auto& task : m_tasks) {
            if (task.messageId == message.id && task.status == ProcessingStatus::Pending) {
                if (!firstFilePaused) {
                    task.status = ProcessingStatus::Paused;
                    updateFileStatusForSessionMessage(sessionId, message.id, task.fileId,
                        ProcessingStatus::Paused, QString());
                    firstFilePaused = true;
                }
            }
        }
    }

    syncMessageStatus(message.id, sessionId);
    updateQueuePositions();
    emit queueSizeChanged();

    // 模式2下不自动触发处理（因为默认暂停）
    if (!shouldPause) {
        processNextTask();
    }

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

bool ProcessingController::hasProcessingOrPendingTasks() const
{
    return !getPauseModeSwitchBlockers().isEmpty();
}

QVariantList ProcessingController::getPauseModeSwitchBlockers() const
{
    struct MessageState {
        QString sessionId;
        bool hasProcessing = false;
        bool hasPending = false;
        bool hasPaused = false;
    };

    struct SessionSummary {
        QString sessionId;
        QString sessionName;
        QSet<QString> processingMessages;
        QSet<QString> pendingMessages;
        QSet<QString> pausedMessages;
    };

    QHash<QString, SessionSummary> summaries;
    QHash<QString, MessageState> messageStates;
    QStringList sessionOrder;
    auto ensureSummary = [this, &summaries, &sessionOrder](const QString& sessionId) -> SessionSummary& {
        const QString normalizedSessionId = sessionId.isEmpty()
            ? (m_sessionController ? m_sessionController->activeSessionId() : QString())
            : sessionId;

        if (!summaries.contains(normalizedSessionId)) {
            SessionSummary summary;
            summary.sessionId = normalizedSessionId;
            if (m_sessionController && !normalizedSessionId.isEmpty()) {
                summary.sessionName = m_sessionController->getSessionName(normalizedSessionId);
            }
            if (summary.sessionName.trimmed().isEmpty()) {
                summary.sessionName = normalizedSessionId.isEmpty()
                    ? tr("当前会话")
                    : tr("未命名会话");
            }
            summaries.insert(normalizedSessionId, summary);
            sessionOrder.append(normalizedSessionId);
        }
        return summaries[normalizedSessionId];
    };

    for (const auto& task : m_tasks) {
        if (task.messageId.isEmpty()) {
            continue;
        }

        if (task.status != ProcessingStatus::Processing &&
            task.status != ProcessingStatus::Pending &&
            task.status != ProcessingStatus::Paused) {
            continue;
        }

        MessageState& state = messageStates[task.messageId];
        if (state.sessionId.isEmpty()) {
            state.sessionId = resolveSessionIdForMessage(task.messageId);
        }

        if (task.status == ProcessingStatus::Processing) {
            state.hasProcessing = true;
        } else if (task.status == ProcessingStatus::Pending) {
            state.hasPending = true;
        } else if (task.status == ProcessingStatus::Paused) {
            state.hasPaused = true;
        }
    }

    for (const QString& messageId : m_pausedMessageIds) {
        if (messageId.isEmpty() || !messageStates.contains(messageId)) {
            continue;
        }
        messageStates[messageId].hasPaused = true;
    }

    for (auto it = messageStates.cbegin(); it != messageStates.cend(); ++it) {
        const QString& messageId = it.key();
        const MessageState& state = it.value();

        SessionSummary& summary = ensureSummary(state.sessionId);
        if (state.hasPaused) {
            summary.pausedMessages.insert(messageId);
        } else if (state.hasProcessing) {
            summary.processingMessages.insert(messageId);
        } else if (state.hasPending) {
            summary.pendingMessages.insert(messageId);
        }
    }

    QVariantList result;
    for (const QString& sessionId : sessionOrder) {
        const SessionSummary& summary = summaries[sessionId];
        const int processingCount = summary.processingMessages.size();
        const int pendingCount = summary.pendingMessages.size();
        const int pausedCount = summary.pausedMessages.size();
        const int totalCount = processingCount + pendingCount + pausedCount;

        if (totalCount == 0) {
            continue;
        }

        QVariantMap item;
        item[QStringLiteral("sessionId")] = summary.sessionId;
        item[QStringLiteral("sessionName")] = summary.sessionName;
        item[QStringLiteral("processingCount")] = processingCount;
        item[QStringLiteral("pendingCount")] = pendingCount;
        item[QStringLiteral("pausedCount")] = pausedCount;
        item[QStringLiteral("totalCount")] = totalCount;
        result.append(item);
    }

    return result;
}

RecoverySnapshot ProcessingController::buildRecoverySnapshot() const
{
    RecoverySnapshot snapshot;
    snapshot.snapshotVersion = 1;
    snapshot.pauseModeAtShutdown = currentPauseMode();
    snapshot.currentProcessingMessageId = m_currentProcessingMessageId;
    snapshot.capturedAtMs = QDateTime::currentMSecsSinceEpoch();
    snapshot.shutdownReason = SettingsController::instance()->lastExitReason();

    if (!m_sessionController || !m_sessionController->sessionModel()) {
        return snapshot;
    }

    SessionModel* sessionModel = m_sessionController->sessionModel();
    for (int sessionIndex = 0; sessionIndex < sessionModel->rowCount(); ++sessionIndex) {
        const Session session = sessionModel->sessionAt(sessionIndex);
        for (int messageIndex = 0; messageIndex < session.messages.size(); ++messageIndex) {
            const Message& message = session.messages.at(messageIndex);

            RecoverableMessageState messageState;
            messageState.sessionId = session.id;
            messageState.sessionName = session.name;
            messageState.messageId = message.id;
            messageState.orderIndex = messageIndex;

            bool hasProcessingFile = false;
            bool hasPendingFile = false;
            bool hasPausedFile = false;

            for (int fileIndex = 0; fileIndex < message.mediaFiles.size(); ++fileIndex) {
                const MediaFile& file = message.mediaFiles.at(fileIndex);
                if (file.status != ProcessingStatus::Pending &&
                    file.status != ProcessingStatus::Processing &&
                    file.status != ProcessingStatus::Paused) {
                    continue;
                }

                RecoverableFileState fileState;
                fileState.fileId = file.id;
                fileState.preShutdownStatus = file.status;
                fileState.orderIndex = fileIndex;
                messageState.fileStates.append(fileState);

                if (file.status == ProcessingStatus::Processing) {
                    hasProcessingFile = true;
                } else if (file.status == ProcessingStatus::Paused) {
                    hasPausedFile = true;
                } else {
                    hasPendingFile = true;
                }
            }

            if (messageState.fileStates.isEmpty() && !m_pausedMessageIds.contains(message.id)) {
                continue;
            }

            if (snapshot.pauseModeAtShutdown == 2) {
                // 模式2下优先以“用户是否显式暂停/是否已激活”判定意图，避免单个Paused文件标记误判整卡片。
                if (m_pausedMessageIds.contains(message.id)) {
                    messageState.preShutdownStatus = ProcessingStatus::Paused;
                    messageState.runIntent = RecoveryRunIntent::Paused;
                } else if (m_activatedMessageIds.contains(message.id) ||
                           message.id == m_currentProcessingMessageId ||
                           message.status == ProcessingStatus::Processing ||
                           hasProcessingFile) {
                    messageState.preShutdownStatus = ProcessingStatus::Processing;
                    messageState.runIntent = RecoveryRunIntent::Active;
                } else if (hasPendingFile || hasPausedFile || message.status == ProcessingStatus::Pending) {
                    messageState.preShutdownStatus = ProcessingStatus::Pending;
                    messageState.runIntent = RecoveryRunIntent::Queued;
                } else {
                    continue;
                }
            } else if (m_pausedMessageIds.contains(message.id) || message.status == ProcessingStatus::Paused || hasPausedFile) {
                messageState.preShutdownStatus = ProcessingStatus::Paused;
                messageState.runIntent = RecoveryRunIntent::Paused;
                if (snapshot.pauseModeAtShutdown == 1 && snapshot.queueBlockerMessageId.isEmpty()) {
                    snapshot.queueBlockerMessageId = message.id;
                }
            } else if (message.id == m_currentProcessingMessageId ||
                       message.status == ProcessingStatus::Processing ||
                       hasProcessingFile) {
                messageState.preShutdownStatus = ProcessingStatus::Processing;
                messageState.runIntent = RecoveryRunIntent::Active;
            } else if (hasPendingFile || message.status == ProcessingStatus::Pending) {
                messageState.preShutdownStatus = ProcessingStatus::Pending;
                messageState.runIntent = RecoveryRunIntent::Queued;
            } else {
                continue;
            }

            snapshot.messages.append(messageState);
        }
    }

    return snapshot;
}

bool ProcessingController::restoreFromRecoverySnapshot(const RecoverySnapshot& snapshot)
{
    if (!m_messageModel || !m_sessionController || !snapshot.isValid()) {
        return false;
    }

    if (!m_tasks.isEmpty()) {
        qWarning() << "[ProcessingController] restoreFromRecoverySnapshot aborted because runtime queue is not empty";
        return false;
    }

    SettingsController::instance()->setPauseMode(snapshot.pauseModeAtShutdown);

    m_pausedMessageIds.clear();
    m_priorityResumeMessageIds.clear();
    m_currentProcessingMessageId.clear();
    m_activatedMessageIds.clear();
    m_globalPauseEnabled = false;

    QList<RecoverableMessageState> messages = snapshot.messages;
    std::sort(messages.begin(), messages.end(), [](const RecoverableMessageState& lhs, const RecoverableMessageState& rhs) {
        return lhs.orderIndex < rhs.orderIndex;
    });

    QString resolvedCurrentMessageId;
    QList<QString> activeMessageOrder;
    int restoredTaskCount = 0;

    for (RecoverableMessageState& messageState : messages) {
        QString sessionId = messageState.sessionId;
        if (sessionId.isEmpty()) {
            sessionId = resolveSessionIdForMessage(messageState.messageId);
        }

        Message message = messageForSession(sessionId, messageState.messageId);
        if (message.id.isEmpty()) {
            continue;
        }

        std::sort(messageState.fileStates.begin(), messageState.fileStates.end(),
                  [](const RecoverableFileState& lhs, const RecoverableFileState& rhs) {
            return lhs.orderIndex < rhs.orderIndex;
        });

        bool hasPausedTask = false;
        int restoredTasksForMessage = 0;

        for (const RecoverableFileState& fileState : messageState.fileStates) {
            MediaFile targetFile;
            bool foundFile = false;
            for (const MediaFile& file : message.mediaFiles) {
                if (file.id == fileState.fileId) {
                    targetFile = file;
                    foundFile = true;
                    break;
                }
            }
            if (!foundFile) {
                continue;
            }

            const QString taskId = createAndRegisterTask(message, targetFile, sessionId);
            for (QueueTask& task : m_tasks) {
                if (task.taskId != taskId) {
                    continue;
                }

                ProcessingStatus restoredStatus = ProcessingStatus::Pending;
                if (snapshot.pauseModeAtShutdown == 2) {
                    if (messageState.runIntent == RecoveryRunIntent::Paused &&
                        fileState.preShutdownStatus == ProcessingStatus::Paused) {
                        restoredStatus = ProcessingStatus::Paused;
                    }
                } else {
                    restoredStatus = (fileState.preShutdownStatus == ProcessingStatus::Paused)
                        ? ProcessingStatus::Paused
                        : ProcessingStatus::Pending;
                }
                task.status = restoredStatus;
                if (restoredStatus == ProcessingStatus::Paused) {
                    hasPausedTask = true;
                }
                break;
            }

            ProcessingStatus fileDisplayStatus = ProcessingStatus::Pending;
            if (snapshot.pauseModeAtShutdown == 2) {
                if (messageState.runIntent == RecoveryRunIntent::Paused &&
                    fileState.preShutdownStatus == ProcessingStatus::Paused) {
                    fileDisplayStatus = ProcessingStatus::Paused;
                }
            } else {
                fileDisplayStatus = (fileState.preShutdownStatus == ProcessingStatus::Paused)
                    ? ProcessingStatus::Paused
                    : ProcessingStatus::Pending;
            }

            updateFileStatusForSessionMessage(sessionId, message.id, targetFile.id,
                                              fileDisplayStatus,
                                              targetFile.resultPath);
            restoredTaskCount += 1;
            restoredTasksForMessage += 1;
        }

        if (restoredTasksForMessage == 0) {
            continue;
        }

        ProcessingStatus messageStatus = hasPausedTask ? ProcessingStatus::Paused : ProcessingStatus::Pending;
        if (snapshot.pauseModeAtShutdown == 2) {
            messageStatus = (messageState.runIntent == RecoveryRunIntent::Paused)
                ? ProcessingStatus::Paused
                : ProcessingStatus::Pending;
        }
        updateStatusForSessionMessage(sessionId, message.id, messageStatus);
        updateProgressForSessionMessage(sessionId, message.id, 0);
        updateQueuePositionForSessionMessage(sessionId, message.id, -1);

        if (snapshot.pauseModeAtShutdown == 2) {
            if (messageState.runIntent == RecoveryRunIntent::Paused) {
                m_pausedMessageIds.insert(message.id);
            } else if (messageState.runIntent == RecoveryRunIntent::Active) {
                m_activatedMessageIds.insert(message.id);
                activeMessageOrder.append(message.id);
            }
        } else if (messageState.runIntent == RecoveryRunIntent::Paused || messageStatus == ProcessingStatus::Paused) {
            m_pausedMessageIds.insert(message.id);
        } else if (messageState.runIntent == RecoveryRunIntent::Active) {
            activeMessageOrder.append(message.id);
        }

        if (resolvedCurrentMessageId.isEmpty() &&
            !snapshot.currentProcessingMessageId.isEmpty() &&
            message.id == snapshot.currentProcessingMessageId) {
            resolvedCurrentMessageId = message.id;
        }
    }

    if (restoredTaskCount == 0) {
        return false;
    }

    if (resolvedCurrentMessageId.isEmpty() && !activeMessageOrder.isEmpty()) {
        resolvedCurrentMessageId = activeMessageOrder.first();
    }

    if (snapshot.pauseModeAtShutdown == 0 || snapshot.pauseModeAtShutdown == 2) {
        m_currentProcessingMessageId = resolvedCurrentMessageId;
    }

    if (snapshot.pauseModeAtShutdown == 2) {
        m_priorityResumeMessageIds = activeMessageOrder;
    }

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();

    QTimer::singleShot(0, this, &ProcessingController::processNextTask);
    return true;
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
    if (m_shutdownInProgress) {
        return;
    }

    if (m_queueStatus != QueueStatus::Running) {
        return;
    }

    validateAndRepairQueueState();

    if (m_currentProcessingCount >= 1) {
        return;
    }
    
    int pauseMode = SettingsController::instance()->pauseMode();
    
    // ========== 模式1的阻塞检查 ==========
    if (pauseMode == 1 && !m_pausedMessageIds.isEmpty()) {
        // 顺序暂停模式：只要存在暂停消息，整体队列必须阻塞。
        return;
    }
    
    // ========== 模式2：检查是否有已激活的消息可处理 ==========
    if (pauseMode == 2) {
        // 模式2：如果没有已激活的消息，不处理任何任务
        if (m_activatedMessageIds.isEmpty() && m_priorityResumeMessageIds.isEmpty()) {
            return;
        }
        
        // 如果优先队列为空，但有已激活的消息，确保它们的任务是 Pending 状态
        if (m_priorityResumeMessageIds.isEmpty() && !m_activatedMessageIds.isEmpty()) {
            for (const QString& activatedMsgId : m_activatedMessageIds) {
                // 跳过在暂停集合中的消息
                if (m_pausedMessageIds.contains(activatedMsgId)) continue;
                
                QString sessionId = resolveSessionIdForMessage(activatedMsgId);
                for (auto& task : m_tasks) {
                    if (task.messageId == activatedMsgId && task.status == ProcessingStatus::Paused) {
                        task.status = ProcessingStatus::Pending;
                        updateFileStatusForSessionMessage(sessionId, activatedMsgId, task.fileId,
                            ProcessingStatus::Pending, QString());
                    }
                }
            }
        }
    }

    // ========== 查找下一个待处理的任务 ==========
    QueueTask* nextTask = nullptr;
    bool hasSkippedAITask = false;
    bool hasSkippedPausedTask = false;
    
    // 模式0和模式2：检查优先恢复队列
    // 只有当当前消息的所有任务都完成后，才使用优先恢复队列
    QString priorityMessageId;
    bool currentMessageHasPendingTasks = false;  // 当前消息是否还有待处理任务
    
    // 检查当前正在处理的消息是否还有其他待处理任务（模式0和模式2使用）
    if ((pauseMode == 0 || pauseMode == 2) && !m_currentProcessingMessageId.isEmpty()) {
        for (const auto& task : m_tasks) {
            if (task.messageId == m_currentProcessingMessageId && 
                task.status == ProcessingStatus::Pending) {
                currentMessageHasPendingTasks = true;
                break;
            }
        }
        
        // 如果当前消息没有待处理任务了，清除跟踪
        if (!currentMessageHasPendingTasks) {
            m_currentProcessingMessageId.clear();
        }
    }
    
    if ((pauseMode == 0 || pauseMode == 2) && !m_priorityResumeMessageIds.isEmpty()) {
        if (!m_currentProcessingMessageId.isEmpty() && currentMessageHasPendingTasks) {
            priorityMessageId = m_currentProcessingMessageId;
        } else {
            for (const QString& msgId : m_priorityResumeMessageIds) {
                if (m_pausedMessageIds.contains(msgId)) continue;
                
                for (auto& task : m_tasks) {
                    if (task.messageId == msgId && 
                        (task.status == ProcessingStatus::Pending || task.status == ProcessingStatus::Paused)) {
                        priorityMessageId = msgId;
                        break;
                    }
                }
                if (!priorityMessageId.isEmpty()) break;
            }
            
            if (priorityMessageId.isEmpty()) {
                m_priorityResumeMessageIds.clear();
            } else if (pauseMode == 2 && !m_pausedMessageIds.contains(priorityMessageId)) {
                // 模式2：只有当消息不在暂停集合中时，才恢复暂停的任务
                QString sessionId = resolveSessionIdForMessage(priorityMessageId);
                for (auto& task : m_tasks) {
                    if (task.messageId == priorityMessageId && task.status == ProcessingStatus::Paused) {
                        bool hasActiveVideoProcessor = m_activeVideoProcessors.contains(task.taskId) ||
                                                       m_activeAIVideoProcessors.contains(task.taskId);
                        if (hasActiveVideoProcessor) {
                            // 有活动的视频处理器，恢复它
                            if (m_activeAIVideoProcessors.contains(task.taskId)) {
                                m_activeAIVideoProcessors[task.taskId]->resume();
                            }
                            if (m_activeVideoProcessors.contains(task.taskId)) {
                                m_activeVideoProcessors[task.taskId]->resume();
                            }
                            task.status = ProcessingStatus::Processing;
                            m_currentProcessingCount++;
                            emit currentProcessingCountChanged();
                            TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
                            m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB);
                            updateFileStatusForSessionMessage(sessionId, priorityMessageId, task.fileId,
                                ProcessingStatus::Processing, QString());
                            return;  // 已恢复处理，直接返回
                        } else {
                            // 没有活动的视频处理器，恢复为 Pending
                            task.status = ProcessingStatus::Pending;
                            updateFileStatusForSessionMessage(sessionId, priorityMessageId, task.fileId,
                                ProcessingStatus::Pending, QString());
                        }
                    }
                }
            }
        }
    }
    
    for (auto& task : m_tasks) {
        if (task.status != ProcessingStatus::Pending) {
            continue;
        }
        
        // 检查消息是否被暂停
        if (m_pausedMessageIds.contains(task.messageId)) {
            // 所有模式：跳过暂停消息的任务
            hasSkippedPausedTask = true;
            continue;
        }
        
        // 模式2：按优先队列顺序处理已激活的消息
        if (pauseMode == 2) {
            // 模式2始终使用优先队列来确定处理顺序
            if (!priorityMessageId.isEmpty()) {
                if (task.messageId != priorityMessageId) {
                    continue;  // 只处理优先消息的任务
                }
            } else if (!m_priorityResumeMessageIds.isEmpty()) {
                // 有优先队列但没有找到优先消息（可能是暂停的），跳过
                continue;
            } else if (!m_activatedMessageIds.contains(task.messageId)) {
                // 优先队列为空，检查是否是已激活的消息
                continue;
            }
        }
        
        // 模式0：如果有优先消息或正在等待当前消息完成，只处理指定消息的任务
        if (pauseMode == 0 && !priorityMessageId.isEmpty()) {
            if (task.messageId != priorityMessageId) {
                continue;
            }
        }
        
        // 检查 AI 推理引擎池可用性
        if (m_taskMessages.contains(task.taskId)) {
            const Message& msg = m_taskMessages[task.taskId];
            int availableCount = m_aiEnginePool->availableCount();
            if (msg.mode == ProcessingMode::AIInference && availableCount <= 0) {
                hasSkippedAITask = true;
                continue;
            }
        }

        nextTask = &task;
        break;
    }
    
    // 模式0和模式2：如果优先消息的任务都处理完了，从优先队列中移除
    if ((pauseMode == 0 || pauseMode == 2) && !priorityMessageId.isEmpty() && !nextTask) {
        m_priorityResumeMessageIds.removeOne(priorityMessageId);
        
        // 继续寻找下一个任务
        if (!m_priorityResumeMessageIds.isEmpty() || hasSkippedPausedTask) {
            QTimer::singleShot(100, this, &ProcessingController::processNextTask);
            return;
        }
        
        // 模式2：如果还有已激活的消息，继续处理
        if (pauseMode == 2 && !m_activatedMessageIds.isEmpty()) {
            QTimer::singleShot(100, this, &ProcessingController::processNextTask);
            return;
        }
    }

    if (!nextTask) {
        if (hasSkippedAITask) {
            if (m_aiEnginePool->hasDrainingEngine()) {
            } else {
                QTimer::singleShot(200, this, &ProcessingController::processNextTask);
            }
        }
        return;
    }
    
    if (!tryStartTask(*nextTask)) {
        if (m_aiEnginePool->hasDrainingEngine()) {
        } else {
            QTimer::singleShot(100, this, &ProcessingController::processNextTask);
        }
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

    // 注册消息到 ProcessingTimeManager（用于后台倒计时）
    if (m_processingTimeManager && !m_processingTimeManager->isTaskRegistered(task.messageId)) {
        if (m_taskMessages.contains(task.taskId)) {
            const Message& message = m_taskMessages[task.taskId];
            qint64 predictedTotalSec = message.predictedTotalSec;
            
            // 如果没有预测时间，使用 TaskTimeEstimator 计算
            if (predictedTotalSec <= 0) {
                QVariantList files;
                for (const auto& mf : message.mediaFiles) {
                    QVariantMap fileMap;
                    fileMap["width"] = mf.resolution.width() > 0 ? mf.resolution.width() : 1920;
                    fileMap["height"] = mf.resolution.height() > 0 ? mf.resolution.height() : 1080;
                    fileMap["isVideo"] = mf.type == MediaType::Video;
                    fileMap["durationMs"] = mf.duration;
                    // 【改进】使用更保守的默认帧率估算，避免低估视频处理时间
                    // 大多数视频为 24-30fps，使用 25fps 作为保守估计
                    fileMap["fps"] = 25.0;
                    files.append(fileMap);
                }
                
                bool useGpu = false;
                QString modelId;
                if (message.mode == ProcessingMode::AIInference) {
                    useGpu = message.aiParams.useGpu;
                    modelId = message.aiParams.modelId;
                }
                
                predictedTotalSec = static_cast<qint64>(
                    TaskTimeEstimator::instance()->estimateMessageTotalTime(
                        static_cast<int>(message.mode), useGpu, modelId, files
                    )
                );
                
                // 确保至少 1 秒
                if (predictedTotalSec <= 0) {
                    predictedTotalSec = 60;
                }
            }
            
            m_processingTimeManager->registerTask(
                task.messageId, 
                predictedTotalSec,
                QDateTime::currentMSecsSinceEpoch()
            );
            
            // 更新 MessageModel 中的预测时间
            if (m_messageModel) {
                m_messageModel->updatePredictedTotalSec(task.messageId, predictedTotalSec);
            }
        }
    }

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
                        return;
                    }

                    bool isPaused = false;
                    for (const auto& task : m_tasks) {
                        if (task.taskId == taskId && task.status == ProcessingStatus::Paused) {
                            isPaused = true;
                            break;
                        }
                    }
                    if (isPaused) {
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
                TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
                m_resourceManager->release(taskId);
                
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
                
                // 同步进度到 ProcessingTimeManager 触发动态时间修正
                if (m_processingTimeManager && m_processingTimeManager->isTaskRegistered(task.messageId)) {
                    double progressRatio = normalizedProgress / 100.0;
                    m_processingTimeManager->updateTaskProgress(task.messageId, progressRatio);
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
        return;
    }

    bool taskFound = false;
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            taskFound = true;
            QString messageId = m_tasks[i].messageId;
            QString fileId = m_tasks[i].fileId;
            const QString sessionId = resolveSessionIdForMessage(messageId);

            // 【修复】检查任务是否处于暂停状态
            if (m_tasks[i].status == ProcessingStatus::Paused) {
                return;
            }
            
            const Message msg = messageForSession(sessionId, messageId);
            
            // 【修复】检查是否是 Shader 模式的视频，如果是则不设置 Completed 状态
            // 因为视频处理才刚刚开始
            bool isShaderVideo = false;
            for (const MediaFile& mf : msg.mediaFiles) {
                if (mf.id == fileId && msg.mode == ProcessingMode::Shader && ImageUtils::isVideoFile(mf.filePath)) {
                    isShaderVideo = true;
                    break;
                }
            }
            
            if (isShaderVideo) {
                // Shader 视频处理：保持 Processing 状态，启动视频处理器
                for (const MediaFile& mf : msg.mediaFiles) {
                    if (mf.id == fileId) {
                        processShaderVideoThumbnailAsync(
                            taskId,
                            messageId,
                            fileId,
                            mf.filePath,
                            msg.shaderParams
                        );
                        break;
                    }
                }
                return;  // 不执行后续的 Completed 状态设置
            }
            
            // 非 Shader 视频的处理逻辑
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
            
            break;
        }
    }
    
    if (!taskFound) {
        qWarning() << "[ProcessingController][completeTask] Task not found in m_tasks:" << taskId;
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

    // 【修复】将任务状态重新设置为 Processing，因为视频处理才刚刚开始
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.status = ProcessingStatus::Processing;
            task.progress = 5;  // 初始进度
            break;
        }
    }
    
    // 更新文件状态为 Processing
    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
        ProcessingStatus::Processing, QString());

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

            // 【修复】检查任务是否处于暂停状态
            // 如果任务被暂停，但视频处理已完成，仍然需要更新状态为完成
            bool isPaused = false;
            for (const auto& task : m_tasks) {
                if (task.taskId == taskId && task.status == ProcessingStatus::Paused) {
                    isPaused = true;
                    break;
                }
            }
            if (isPaused) {
            }

            if (success) {
                // 验证输出文件完整性
                QFileInfo outputInfo(outputPath);
                if (!outputInfo.exists() || outputInfo.size() == 0) {
                    qWarning() << "[ProcessingController][Shader] output file invalid"
                               << "path:" << outputPath
                               << "exists:" << outputInfo.exists()
                               << "size:" << outputInfo.size();
                    
                    // 【修复】更新任务状态为 Failed
                    for (auto& task : m_tasks) {
                        if (task.taskId == taskId) {
                            task.status = ProcessingStatus::Failed;
                            break;
                        }
                    }
                    
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
                    
                    // 【修复】更新任务状态为 Completed
                    for (auto& task : m_tasks) {
                        if (task.taskId == taskId) {
                            task.status = ProcessingStatus::Completed;
                            task.progress = 100;
                            break;
                        }
                    }
                    
                    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                        ProcessingStatus::Completed, outputPath);
                    
                    AutoSaveService::instance()->autoSaveResult(taskId, outputPath);
                }
            } else {
                // 【修复】更新任务状态为 Failed
                for (auto& task : m_tasks) {
                    if (task.taskId == taskId) {
                        task.status = ProcessingStatus::Failed;
                        break;
                    }
                }
                
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
    
    int pauseMode = SettingsController::instance()->pauseMode();
    
    // 模式2：重试时默认暂停状态
    ProcessingStatus defaultStatus = (pauseMode == 2) ? ProcessingStatus::Paused : ProcessingStatus::Pending;
    
    m_messageModel->updateStatus(messageId, static_cast<int>(defaultStatus));
    m_messageModel->updateProgress(messageId, 0);
    m_messageModel->updateErrorMessage(messageId, QString());
    
    // 重置所有文件状态
    for (const auto& file : message.mediaFiles) {
        m_messageModel->updateFileStatus(messageId, file.id, 
            static_cast<int>(defaultStatus), QString());
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
            if (!hasActiveTaskForFile(messageId, file.id)) {
                filesToRetry.append(file);
            }
        }
    }

    if (filesToRetry.isEmpty()) {
        qWarning() << "[ProcessingController] retryFailedFiles: No failed files to retry";
        return;
    }

    int pauseMode = SettingsController::instance()->pauseMode();
    
    ProcessingStatus defaultStatus = (pauseMode == 2) ? ProcessingStatus::Paused : ProcessingStatus::Pending;
    
    m_messageModel->updateStatus(messageId, static_cast<int>(defaultStatus));
    m_messageModel->updateErrorMessage(messageId, QString());

    for (const auto& file : filesToRetry) {
        m_messageModel->updateFileStatus(messageId, file.id,
            static_cast<int>(defaultStatus), QString());
    }

    const QString sessionId = resolveSessionIdForMessage(messageId);
    
    if (pauseMode == 2) {
        m_pausedMessageIds.insert(messageId);
    }
    
    for (const auto& file : filesToRetry) {
        removeStaleTasksForFile(messageId, file.id);
        createAndRegisterTask(message, file, sessionId);
    }
    
    if (pauseMode == 2) {
        bool firstFilePaused = false;
        for (auto& task : m_tasks) {
            if (task.messageId == messageId && task.status == ProcessingStatus::Pending) {
                if (!firstFilePaused) {
                    task.status = ProcessingStatus::Paused;
                    m_messageModel->updateFileStatus(messageId, task.fileId,
                        static_cast<int>(ProcessingStatus::Paused), QString());
                    firstFilePaused = true;
                }
            }
        }
    }

    updateQueuePositions();
    emit queueSizeChanged();

    if (m_sessionController) {
        requestSessionSync();
    }

    // 模式2下不自动触发处理（因为默认暂停）
    if (pauseMode != 2) {
        processNextTask();
    }
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
    m_progressSyncHelper->syncMessageStatus(messageId, sessionId, m_tasks, m_taskContexts, m_pausedMessageIds);
}

int ProcessingController::resourcePressure() const
{
    return m_resourcePressure;
}

void ProcessingController::cancelMessageTasks(const QString& messageId)
{
    
    m_taskCoordinator->cancelMessageTasks(messageId);
    
    // 【修复】检查是否需要传递暂停状态
    bool wasPaused = m_pausedMessageIds.contains(messageId);
    if (wasPaused) {
        m_pausedMessageIds.remove(messageId);
    }
    
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
    
    // 暂停状态传递：根据模式决定是否传递暂停状态
    if (wasPaused) {
        int pauseMode = SettingsController::instance()->pauseMode();
        switch (pauseMode) {
            case 0:  // 模式一：单任务暂停，不传递暂停状态
                break;
                
            case 1:  // 模式二：顺序暂停，传递暂停状态给下一个消息
                {
                    QString nextMessageId = findNextPendingMessageId();
                    if (!nextMessageId.isEmpty()) {
                        m_pausedMessageIds.insert(nextMessageId);
                        
                        // 【修复】将下一个消息的首个 Pending 文件标记为暂停
                        QString nextSessionId = resolveSessionIdForMessage(nextMessageId);
                        for (auto& task : m_tasks) {
                            if (task.messageId == nextMessageId && task.status == ProcessingStatus::Pending) {
                                task.status = ProcessingStatus::Paused;
                                updateFileStatusForSessionMessage(nextSessionId, nextMessageId, task.fileId,
                                    ProcessingStatus::Paused, QString());
                                break;
                            }
                        }
                        syncMessageStatus(nextMessageId, nextSessionId);
                    }
                }
                break;
                
            case 2:  // 模式三：自由选择，保持全局暂停状态
                // 不传递暂停状态，但保持全局暂停
                break;
                
            default:
                break;
        }
    }
    
    emit messageTasksCancelled(messageId);
    
    // 立即尝试处理下一个任务，如果当前没有处理中的任务
    if (m_currentProcessingCount == 0) {
        QTimer::singleShot(100, this, &ProcessingController::processNextTask);
    } else {
        QTimer::singleShot(500, this, &ProcessingController::processNextTask);
    }
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
    
    // 【修复】检查消息是否还有剩余任务
    if (m_pausedMessageIds.contains(messageId)) {
        // 【新增】检查消息中是否还有任何未完成的任务（Pending 或 Paused）
        bool hasPausedFile = false;
        bool hasRemainingPendingTasks = false;
        QString nextPendingFileId;
        
        for (const auto& task : m_tasks) {
            if (task.messageId == messageId) {
                if (task.status == ProcessingStatus::Paused) {
                    hasPausedFile = true;
                } else if (task.status == ProcessingStatus::Pending) {
                    hasRemainingPendingTasks = true;
                    if (nextPendingFileId.isEmpty()) {
                        nextPendingFileId = task.fileId;
                    }
                }
            }
        }
        
        // 【修复】如果消息中还有暂停状态的文件，保持暂停状态，不传递
        if (hasPausedFile) {
            return;
        }
        
        if (hasRemainingPendingTasks && !nextPendingFileId.isEmpty()) {
            // 将暂停状态传递给下一个待处理文件
            QString sessionId = resolveSessionIdForMessage(messageId);
            for (auto& task : m_tasks) {
                if (task.messageId == messageId && task.fileId == nextPendingFileId) {
                    task.status = ProcessingStatus::Paused;
                    updateFileStatusForSessionMessage(sessionId, messageId, nextPendingFileId,
                        ProcessingStatus::Paused, QString());
                    break;
                }
            }
            
            // 消息仍有待处理任务，保持暂停状态，不启动新任务
            return;
        } else if (!hasRemainingPendingTasks && !hasPausedFile) {
            // 消息中没有任何未完成的任务，清理暂停状态
            m_pausedMessageIds.remove(messageId);
            m_priorityResumeMessageIds.removeAll(messageId);  // 同时从优先恢复队列中移除
            
            // 根据暂停模式决定是否传递暂停状态
            int pauseMode = SettingsController::instance()->pauseMode();
            
            if (pauseMode == 0) {
                // 模式0（单任务暂停）：不传递暂停状态，直接继续处理其他消息
                // 不 return，让后面的 processNextTask 继续处理
            } else {
                // 模式1和模式2：传递暂停状态给下一个消息
                QString nextMessageId = findNextPendingMessageId();
                if (!nextMessageId.isEmpty()) {
                    m_pausedMessageIds.insert(nextMessageId);
                    
                    // 将下一个消息的首个 Pending 文件标记为暂停
                    QString nextSessionId = resolveSessionIdForMessage(nextMessageId);
                    for (auto& task : m_tasks) {
                        if (task.messageId == nextMessageId && task.status == ProcessingStatus::Pending) {
                            task.status = ProcessingStatus::Paused;
                            updateFileStatusForSessionMessage(nextSessionId, nextMessageId, task.fileId,
                                ProcessingStatus::Paused, QString());
                                break;
                            }
                        }
                    syncMessageStatus(nextMessageId, nextSessionId);
                    
                    // 暂停状态已传递，不启动新任务
                    return;
                }
            }
        }
    }

    QTimer::singleShot(300, this, &ProcessingController::processNextTask);
}

void ProcessingController::cancelSessionTasks(const QString& sessionId)
{
    m_taskCoordinator->cancelSessionTasks(sessionId);
    
    // 【修复】收集该会话中所有暂停的消息ID，稍后移除
    QSet<QString> pausedMessageIdsToRemove;
    
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
        // 【修复】从暂停集合中移除被取消的消息
        m_pausedMessageIds.remove(messageId);
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

void ProcessingController::pauseMessageTasks(const QString& messageId)
{
    if (messageId.isEmpty()) return;
    
    // 防抖：300ms 内的重复点击忽略
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastPauseActionTime < 300) {
        return;
    }
    m_lastPauseActionTime = now;
    
    // 获取当前暂停模式
    int pauseMode = SettingsController::instance()->pauseMode();
    
    // 添加到暂停集合（所有模式都需要）
    m_pausedMessageIds.insert(messageId);
    
    // 收集该消息的任务信息
    QString sessionId = resolveSessionIdForMessage(messageId);
    QString processingFileId;  // 正在处理的文件ID
    QString firstPendingFileId;  // 第一个Pending文件ID
    QStringList taskIdsToRelease;  // 需要释放资源的任务ID
    int processingPausedCount = 0;
    
    for (auto& task : m_tasks) {
        if (task.messageId == messageId) {
            // 通知任务协调器暂停（所有模式都需要）
            m_taskCoordinator->triggerPause(task.taskId, true);
            
            // 记录正在处理的文件
            if (task.status == ProcessingStatus::Processing) {
                processingFileId = task.fileId;
                processingPausedCount++;
                taskIdsToRelease.append(task.taskId);
                
                // 暂停视频处理器（所有模式通用）
                if (m_activeAIVideoProcessors.contains(task.taskId)) {
                    m_activeAIVideoProcessors[task.taskId]->pause();
                }
                if (m_activeVideoProcessors.contains(task.taskId)) {
                    m_activeVideoProcessors[task.taskId]->pause();
                }
            }
            
            // 记录第一个Pending文件
            if (task.status == ProcessingStatus::Pending && firstPendingFileId.isEmpty()) {
                firstPendingFileId = task.fileId;
            }
        }
    }
    
    // ========== 根据暂停模式执行不同逻辑 ==========
    switch (pauseMode) {
        case 0: {
            // 模式0（单任务暂停）：
            // - 该消息所有任务从调度中跳过（通过 m_pausedMessageIds）
            // - 只标记一个文件为 Paused 状态（UI显示暂停图标）
            // - 其他任务保持 Pending 状态（不改变）
            // - 继续处理其他消息
            
            m_globalPauseEnabled = false;
            
            // 确定要标记为暂停的文件：优先正在处理的，否则第一个Pending
            QString pausedFileId = !processingFileId.isEmpty() ? processingFileId : firstPendingFileId;
            
            // 只标记一个文件为 Paused 状态
            for (auto& task : m_tasks) {
                if (task.messageId == messageId && task.fileId == pausedFileId) {
                    if (task.status == ProcessingStatus::Processing) {
                        // 减少处理计数
                        if (m_currentProcessingCount > 0) {
                            m_currentProcessingCount--;
                            emit currentProcessingCountChanged();
                        }
                        // 释放资源
                        m_resourceManager->release(task.taskId);
                        TaskStateManager::instance()->unregisterActiveTask(task.taskId);
                    }
                    task.status = ProcessingStatus::Paused;
                    updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                        ProcessingStatus::Paused, QString());
                    break;
                }
            }
            
            // 异步释放 AI 引擎
            if (!taskIdsToRelease.isEmpty()) {
                QThreadPool::globalInstance()->start([this, taskIdsToRelease]() {
                    for (const QString& taskId : taskIdsToRelease) {
                        m_aiEnginePool->release(taskId);
                    }
                    QMetaObject::invokeMethod(this, &ProcessingController::processNextTask, Qt::QueuedConnection);
                });
            } else {
                // 没有正在处理的任务，直接触发下一个任务
                QTimer::singleShot(100, this, &ProcessingController::processNextTask);
            }
            
            break;
        }
        
        case 1: {
            // 模式1（顺序暂停）：
            // - 暂停该消息所有任务
            // - 阻塞整个队列
            // - 标记正在处理的文件或第一个Pending文件为暂停状态
            
            m_globalPauseEnabled = false;
            
            // 标记正在处理的任务为 Paused
            for (auto& task : m_tasks) {
                if (task.messageId == messageId && task.status == ProcessingStatus::Processing) {
                    if (m_currentProcessingCount > 0) {
                        m_currentProcessingCount--;
                        emit currentProcessingCountChanged();
                    }
                    m_resourceManager->release(task.taskId);
                    TaskStateManager::instance()->unregisterActiveTask(task.taskId);
                    task.status = ProcessingStatus::Paused;
                    updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                        ProcessingStatus::Paused, QString());
                }
            }
            
            // 如果没有正在处理的任务，将第一个 Pending 任务标记为暂停
            if (processingPausedCount == 0 && !firstPendingFileId.isEmpty()) {
                for (auto& task : m_tasks) {
                    if (task.messageId == messageId && task.fileId == firstPendingFileId) {
                        task.status = ProcessingStatus::Paused;
                        updateFileStatusForSessionMessage(sessionId, messageId, firstPendingFileId,
                            ProcessingStatus::Paused, QString());
                        break;
                    }
                }
            }
            
            // 不处理下一个任务
            break;
        }
        
        case 2: {
            // 模式2（自由选择）：
            // - 暂停该消息，从激活集合中移除
            // - 继续处理其他已激活的消息
            // - 不使用全局暂停
            
            m_globalPauseEnabled = false;  // 不使用全局暂停
            m_activatedMessageIds.remove(messageId);  // 从激活集合中移除
            
            // 如果当前正在处理的消息就是被暂停的消息，清除跟踪
            if (m_currentProcessingMessageId == messageId) {
                m_currentProcessingMessageId.clear();
            }
            
            // 确定要标记为暂停的文件：优先正在处理的，否则第一个Pending/Paused文件
            QString pausedFileId = !processingFileId.isEmpty() ? processingFileId : firstPendingFileId;
            
            // 标记正在处理的任务为 Paused，释放资源
            for (auto& task : m_tasks) {
                if (task.messageId == messageId) {
                    if (task.status == ProcessingStatus::Processing) {
                        if (m_currentProcessingCount > 0) {
                            m_currentProcessingCount--;
                            emit currentProcessingCountChanged();
                        }
                        m_resourceManager->release(task.taskId);
                        TaskStateManager::instance()->unregisterActiveTask(task.taskId);
                        
                        // 暂停视频处理器
                        if (m_activeAIVideoProcessors.contains(task.taskId)) {
                            m_activeAIVideoProcessors[task.taskId]->pause();
                        }
                        if (m_activeVideoProcessors.contains(task.taskId)) {
                            m_activeVideoProcessors[task.taskId]->pause();
                        }
                        
                        task.status = ProcessingStatus::Paused;
                        updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                            ProcessingStatus::Paused, QString());
                    } else if (task.fileId == pausedFileId && task.status == ProcessingStatus::Pending) {
                        // 如果没有正在处理的任务，将指定文件标记为暂停
                        task.status = ProcessingStatus::Paused;
                        updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                            ProcessingStatus::Paused, QString());
                    }
                }
            }
            
            // 异步释放 AI 引擎，然后继续处理其他已激活的消息
            if (!taskIdsToRelease.isEmpty()) {
                QThreadPool::globalInstance()->start([this, taskIdsToRelease]() {
                    for (const QString& taskId : taskIdsToRelease) {
                        m_aiEnginePool->release(taskId);
                    }
                    QMetaObject::invokeMethod(this, &ProcessingController::processNextTask, Qt::QueuedConnection);
                });
            } else {
                // 没有正在处理的任务，直接触发下一个任务
                QTimer::singleShot(100, this, &ProcessingController::processNextTask);
            }
            
            // 发出取消激活信号通知 QML 更新状态
            emit messageDeactivated(messageId);
            
            break;
        }
        
        default:
            break;
    }
    
    // 暂停时间计时器（所有模式通用）
    if (m_processingTimeManager) {
        m_processingTimeManager->pauseTask(messageId);
    }
    
    syncMessageStatus(messageId, sessionId);
    
    // 发出暂停信号
    emit messageTasksPaused(messageId);
}

void ProcessingController::resumeMessageTasks(const QString& messageId)
{
    if (messageId.isEmpty()) return;
    
    // 防抖：300ms 内的重复点击忽略
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastPauseActionTime < 300) {
        return;
    }
    m_lastPauseActionTime = now;
    
    int pauseMode = SettingsController::instance()->pauseMode();
    
    // 从暂停集合中移除
    m_pausedMessageIds.remove(messageId);
    
    QString sessionId = resolveSessionIdForMessage(messageId);
    int resumedCount = 0;
    
    // ========== 根据暂停模式执行不同恢复逻辑 ==========
    switch (pauseMode) {
        case 0: {
            // 模式0（单任务暂停）：
            // - 添加到优先恢复队列
            // - 等当前消息处理完成后优先处理该消息
            // - 将暂停的文件恢复为 Pending
            
            // 检查当前是否有其他消息正在处理（通过任务状态检查）
            QString currentlyProcessingMsgId;
            for (const auto& task : m_tasks) {
                if (task.status == ProcessingStatus::Processing && task.messageId != messageId) {
                    currentlyProcessingMsgId = task.messageId;
                    break;
                }
            }
            
            // 如果有其他消息正在处理，确保 m_currentProcessingMessageId 被正确设置
            if (!currentlyProcessingMsgId.isEmpty()) {
                m_currentProcessingMessageId = currentlyProcessingMsgId;
            }
            
            // 添加到优先恢复队列（如果不存在）
            if (!m_priorityResumeMessageIds.contains(messageId)) {
                m_priorityResumeMessageIds.append(messageId);
            }
            
            // 检查是否有其他消息正在处理
            bool otherMessageProcessing = !currentlyProcessingMsgId.isEmpty();
            
            // 恢复该消息的任务
            for (auto& task : m_tasks) {
                if (task.messageId == messageId) {
                    m_taskCoordinator->triggerPause(task.taskId, false);
                    resumedCount++;
                    
                    // 将 Paused 状态的任务恢复为 Pending
                    // 模式0：如果有其他消息正在处理，不立即恢复为 Processing，而是保持 Pending
                    if (task.status == ProcessingStatus::Paused) {
                        bool hasActiveVideoProcessor = m_activeVideoProcessors.contains(task.taskId) ||
                                                       m_activeAIVideoProcessors.contains(task.taskId);
                        
                        if (hasActiveVideoProcessor && !otherMessageProcessing) {
                            // 有活动的视频处理器，且没有其他消息正在处理，恢复视频处理器并恢复为 Processing
                            if (m_activeAIVideoProcessors.contains(task.taskId)) {
                                m_activeAIVideoProcessors[task.taskId]->resume();
                            }
                            if (m_activeVideoProcessors.contains(task.taskId)) {
                                m_activeVideoProcessors[task.taskId]->resume();
                            }
                            
                            task.status = ProcessingStatus::Processing;
                            m_currentProcessingCount++;
                            emit currentProcessingCountChanged();
                            TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
                            m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB);
                            updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                                ProcessingStatus::Processing, QString());
                        } else {
                            // 恢复为 Pending，等待当前消息完成后再处理
                            // 不恢复视频处理器，等待 processNextTask 重新启动任务
                            task.status = ProcessingStatus::Pending;
                            updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                                ProcessingStatus::Pending, QString());
                        }
                    }
                }
            }
            
            break;
        }
        
        case 1: {
            // 模式1（顺序暂停）：
            // - 恢复后继续处理队列
            
            for (auto& task : m_tasks) {
                if (task.messageId == messageId) {
                    m_taskCoordinator->triggerPause(task.taskId, false);
                    resumedCount++;
                    
                    // 恢复视频处理器
                    if (m_activeAIVideoProcessors.contains(task.taskId)) {
                        m_activeAIVideoProcessors[task.taskId]->resume();
                    }
                    if (m_activeVideoProcessors.contains(task.taskId)) {
                        m_activeVideoProcessors[task.taskId]->resume();
                    }
                    
                    // 将 Paused 状态的任务恢复为 Pending
                    if (task.status == ProcessingStatus::Paused) {
                        bool hasActiveVideoProcessor = m_activeVideoProcessors.contains(task.taskId) ||
                                                       m_activeAIVideoProcessors.contains(task.taskId);
                        if (hasActiveVideoProcessor) {
                            task.status = ProcessingStatus::Processing;
                            m_currentProcessingCount++;
                            emit currentProcessingCountChanged();
                            TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
                            m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB);
                            updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                                ProcessingStatus::Processing, QString());
                        } else {
                            task.status = ProcessingStatus::Pending;
                            updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                                ProcessingStatus::Pending, QString());
                        }
                    }
                }
            }
            
            break;
        }
        
        case 2: {
            // 模式2（自由选择）：
            // - 添加到激活集合
            // - 使用优先恢复队列机制（类似模式0）
            // - 若有其他消息正在处理，等待其完成后再处理该消息
            
            m_globalPauseEnabled = false;  // 不使用全局暂停
            m_activatedMessageIds.insert(messageId);  // 添加到激活集合
            
            // 检查当前是否有其他消息正在处理
            QString currentlyProcessingMsgId;
            for (const auto& task : m_tasks) {
                if (task.status == ProcessingStatus::Processing && task.messageId != messageId) {
                    currentlyProcessingMsgId = task.messageId;
                    break;
                }
            }
            
            // 如果有其他消息正在处理，确保 m_currentProcessingMessageId 被正确设置
            if (!currentlyProcessingMsgId.isEmpty()) {
                m_currentProcessingMessageId = currentlyProcessingMsgId;
            }
            
            // 添加到优先恢复队列（如果不存在）
            // 始终添加到队列末尾，保持激活/恢复顺序
            if (!m_priorityResumeMessageIds.contains(messageId)) {
                m_priorityResumeMessageIds.append(messageId);
            }
            
            bool otherMessageProcessing = !currentlyProcessingMsgId.isEmpty();
            
            // 恢复该消息的任务
            for (auto& task : m_tasks) {
                if (task.messageId == messageId) {
                    m_taskCoordinator->triggerPause(task.taskId, false);
                    resumedCount++;
                    
                    // 将 Paused 状态的任务恢复为 Pending
                    if (task.status == ProcessingStatus::Paused) {
                        bool hasActiveVideoProcessor = m_activeVideoProcessors.contains(task.taskId) ||
                                                       m_activeAIVideoProcessors.contains(task.taskId);
                        
                        // 模式2：如果有其他消息正在处理，必须等待，不能直接恢复
                        if (otherMessageProcessing) {
                            // 有其他消息正在处理，将状态改为 Pending（等待处理）
                            // 这样 UI 会正确显示"暂停"按钮和"等待处理..."状态
                            task.status = ProcessingStatus::Pending;
                            updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                                ProcessingStatus::Pending, QString());
                        } else if (hasActiveVideoProcessor) {
                            // 没有其他消息正在处理，且有活动的视频处理器，恢复为 Processing
                            if (m_activeAIVideoProcessors.contains(task.taskId)) {
                                m_activeAIVideoProcessors[task.taskId]->resume();
                            }
                            if (m_activeVideoProcessors.contains(task.taskId)) {
                                m_activeVideoProcessors[task.taskId]->resume();
                            }
                            
                            task.status = ProcessingStatus::Processing;
                            m_currentProcessingCount++;
                            emit currentProcessingCountChanged();
                            TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
                            m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB);
                            updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                                ProcessingStatus::Processing, QString());
                        } else {
                            // 没有其他消息正在处理，也没有活动的视频处理器，恢复为 Pending
                            task.status = ProcessingStatus::Pending;
                            updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                                ProcessingStatus::Pending, QString());
                        }
                    }
                }
            }
            
            // 发出激活信号通知 QML 更新状态
            emit messageActivated(messageId);
            
            break;
        }
        
        default:
            break;
    }
    
    // 恢复时间计时器（所有模式通用）
    if (m_processingTimeManager) {
        m_processingTimeManager->resumeTask(messageId);
    }
    
    syncMessageStatus(messageId, sessionId);
    
    emit messageTasksResumed(messageId);
    
    // 触发处理下一个任务
    QTimer::singleShot(100, this, &ProcessingController::processNextTask);
}

bool ProcessingController::isMessagePaused(const QString& messageId) const
{
    return m_pausedMessageIds.contains(messageId);
}

bool ProcessingController::isGlobalPauseEnabled() const
{
    return m_globalPauseEnabled;
}

bool ProcessingController::isMessageActivated(const QString& messageId) const
{
    return m_activatedMessageIds.contains(messageId) || m_priorityResumeMessageIds.contains(messageId);
}

int ProcessingController::currentPauseMode() const
{
    return SettingsController::instance()->pauseMode();
}

void ProcessingController::clearAllPauseStates()
{
    
    // 恢复所有暂停的消息
    for (const QString& messageId : m_pausedMessageIds) {
        QString sessionId = resolveSessionIdForMessage(messageId);
        
        // 恢复该消息的所有任务
        for (auto& task : m_tasks) {
            if (task.messageId == messageId) {
                m_taskCoordinator->triggerPause(task.taskId, false);
                
                if (m_activeAIVideoProcessors.contains(task.taskId)) {
                    m_activeAIVideoProcessors[task.taskId]->resume();
                }
                if (m_activeVideoProcessors.contains(task.taskId)) {
                    m_activeVideoProcessors[task.taskId]->resume();
                }
                
                // 将 Paused 状态的任务恢复为 Pending
                if (task.status == ProcessingStatus::Paused) {
                    bool hasActiveVideoProcessor = m_activeVideoProcessors.contains(task.taskId) ||
                                                   m_activeAIVideoProcessors.contains(task.taskId);
                    if (hasActiveVideoProcessor) {
                        task.status = ProcessingStatus::Processing;
                        m_currentProcessingCount++;
                        emit currentProcessingCountChanged();
                        TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
                        m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB);
                        updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                            ProcessingStatus::Processing, QString());
                    } else {
                        task.status = ProcessingStatus::Pending;
                        updateFileStatusForSessionMessage(sessionId, messageId, task.fileId,
                            ProcessingStatus::Pending, QString());
                    }
                }
            }
        }
        
        // 恢复时间计时器
        if (m_processingTimeManager) {
            m_processingTimeManager->resumeTask(messageId);
        }
        
        syncMessageStatus(messageId, sessionId);
        
        emit messageTasksResumed(messageId);
    }
    
    m_pausedMessageIds.clear();
    m_priorityResumeMessageIds.clear();  // 清理优先恢复队列
    m_currentProcessingMessageId.clear();  // 清理当前处理消息跟踪
    m_activatedMessageIds.clear();  // 清理已激活消息集合（模式2）
    m_globalPauseEnabled = false;
    
    
    // 触发处理下一个任务
    QTimer::singleShot(100, this, &ProcessingController::processNextTask);
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
        if (message.mode == ProcessingMode::AIInference) {
            int available = m_aiEnginePool->availableCount();
            if (available <= 0) {
                return false;
            }
        }
    }
    
    TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);

    if (!m_resourceManager->canStartNewTask(ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB)) {
        return false;
    }
    
    if (!m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB)) {
        return false;
    }

    // 模式0和模式2：记录当前正在处理的消息ID
    int pauseMode = SettingsController::instance()->pauseMode();
    if (pauseMode == 0 || pauseMode == 2) {
        // 如果当前没有正在处理的消息，设置为当前任务的消息
        // 如果当前消息和任务消息相同，保持不变
        // 如果当前消息不同，说明是新消息开始处理，更新跟踪
        if (m_currentProcessingMessageId.isEmpty()) {
            m_currentProcessingMessageId = task.messageId;
        } else if (m_currentProcessingMessageId != task.messageId) {
            // 当前消息已完成，开始处理新消息
            m_currentProcessingMessageId = task.messageId;
        }
    }
    
    startTask(task);
    return true;
}

void ProcessingController::finalizeTask(const QString& taskId, const QString& sessionId, const QString& messageId)
{
    TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Draining || lifecycle == TaskLifecycle::Cleaning || lifecycle == TaskLifecycle::Dead) {
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
        
        // 检查消息的所有文件是否都已处理完成
        bool allFilesSettled = true;
        bool hasPendingFiles = false;
        const Message msg = messageForSession(sessionId, messageId);
        for (const MediaFile& mf : msg.mediaFiles) {
            if (mf.status == ProcessingStatus::Pending) {
                allFilesSettled = false;
                hasPendingFiles = true;
                break;
            }
            if (mf.status == ProcessingStatus::Processing) {
                allFilesSettled = false;
            }
            if (mf.status == ProcessingStatus::Paused || mf.status == ProcessingStatus::Recoverable) {
                allFilesSettled = false;
            }
        }
        
        // 如果所有文件都已处理完成，注销消息的时间管理
        if (allFilesSettled && m_processingTimeManager) {
            m_processingTimeManager->unregisterTask(messageId);
        }
        
        // 【修复】如果消息 ID 在暂停集合中，且还有待处理的文件，移除暂停状态
        // 这样下一个文件可以开始处理（修复顺序暂停模式下第二个视频无法处理的问题）
        bool wasPaused = m_pausedMessageIds.contains(messageId);
        if (wasPaused) {
            if (allFilesSettled) {
                // 所有文件都已处理完成，移除暂停状态
                m_pausedMessageIds.remove(messageId);
            } else if (hasPendingFiles) {
                // 还有待处理的文件，移除暂停状态让下一个文件开始处理
                m_pausedMessageIds.remove(messageId);
            }
        }
        
        // 【修复】顺序暂停模式：当消息的所有文件都处理完成后，根据当前消息的状态决定是否传递暂停状态
        // 如果当前消息是暂停状态，则将暂停状态传递给下一个消息
        // 如果当前消息是继续状态，则不传递暂停状态，让下一个消息继续处理
        if (allFilesSettled) {
            int pauseMode = SettingsController::instance()->pauseMode();
            
            if (pauseMode == 1 && wasPaused) {  // 顺序暂停模式，且当前消息是暂停状态
                QString nextMessageId = findNextPendingMessageId();
                if (!nextMessageId.isEmpty()) {
                    m_pausedMessageIds.insert(nextMessageId);
                    
                    // 将下一个消息的首个 Pending 文件标记为暂停
                    QString nextSessionId = resolveSessionIdForMessage(nextMessageId);
                    for (auto& task : m_tasks) {
                        if (task.messageId == nextMessageId && task.status == ProcessingStatus::Pending) {
                            task.status = ProcessingStatus::Paused;
                            updateFileStatusForSessionMessage(nextSessionId, nextMessageId, task.fileId,
                                ProcessingStatus::Paused, QString());
                            break;
                        }
                    }
                    
                    // 更新消息状态
                    syncMessageStatus(nextMessageId, nextSessionId);
                }
            }
        }

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
    fflush(stdout);
    
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

void ProcessingController::finalizeTaskTermination(const QString& taskId)
{
    m_aiEnginePool->release(taskId);

    if (m_orphanTimers.contains(taskId)) {
        QTimer* oldTimer = m_orphanTimers.take(taskId);
        if (oldTimer) {
            oldTimer->stop();
            oldTimer->deleteLater();
        }
    }

    releaseTaskResources(taskId);

}

bool ProcessingController::waitForBackgroundCleanup(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        cleanupDyingProcessors();
        if (m_dyingProcessors.isEmpty() && m_dyingVideoProcessors.isEmpty()) {
            return true;
        }

        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(10);
    }

    for (auto it = m_dyingProcessors.begin(); it != m_dyingProcessors.end(); ++it) {
        if (it.value()) {
            it.value()->waitForFinished(100);
            if (it.value()->isProcessing()) {
                stashProcessorForShutdown(it.value());
            }
        }
    }

    for (auto it = m_dyingVideoProcessors.begin(); it != m_dyingVideoProcessors.end(); ++it) {
        if (it.value() && it.value()->isProcessing()) {
            stashProcessorForShutdown(it.value());
        }
    }

    m_dyingProcessors.clear();
    m_dyingVideoProcessors.clear();

    qWarning() << "[ProcessingController] Background processor cleanup timed out";
    return false;
}

bool ProcessingController::waitForThreadPoolIdle(int timeoutMs)
{
    if (!m_threadPool) {
        return true;
    }

    const bool drained = m_threadPool->waitForDone(timeoutMs);
    if (!drained) {
        qWarning() << "[ProcessingController] Thread pool wait timed out during shutdown";
    }
    return drained;
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


        disconnectAiEngineForTask(taskId);
        TaskStateManager::instance()->unregisterActiveTask(taskId);
    
    // 【修复】释放资源管理器中的资源
    m_resourceManager->release(taskId);

        if (hasActiveAiVideo) {
            auto aiProc = m_activeAIVideoProcessors.take(taskId);
            if (aiProc) {
                aiProc->cancel();  // 确保处理器被取消
                if (m_shutdownInProgress) {
                    aiProc->waitForFinished(100);
                    if (aiProc->isProcessing()) {
                        stashProcessorForShutdown(aiProc);
                    }
                } else {
                    m_dyingProcessors[taskId] = aiProc;
                }
            }
        }

        if (hasActiveVideo) {
            auto vidProc = m_activeVideoProcessors.take(taskId);
            if (vidProc) {
                vidProc->cancel();  // 确保处理器被取消
                if (m_shutdownInProgress) {
                    if (vidProc->isProcessing()) {
                        stashProcessorForShutdown(vidProc);
                    }
                } else {
                    m_dyingVideoProcessors[taskId] = vidProc;
                }
            }
        }

        if (m_activeImageProcessors.contains(taskId)) {
            auto imgProcessor = m_activeImageProcessors.take(taskId);
            if (imgProcessor) {
                imgProcessor->cancel();
                if (m_shutdownInProgress && imgProcessor->isProcessing()) {
                    stashProcessorForShutdown(imgProcessor);
                }
            }
        }

        if (m_shutdownInProgress) {
            finalizeTaskTermination(taskId);
        }

        return;
    }

    m_taskLifecycle[taskId] = TaskLifecycle::Canceling;


    // 取消 AI 引擎处理（在断开连接前，确保引擎停止）
    AIEngine* engine = m_aiEnginePool->engineForTask(taskId);
    if (engine && engine->isProcessing()) {
        engine->cancelProcess();
    }

    disconnectAiEngineForTask(taskId);

    if (m_activeAIVideoProcessors.contains(taskId)) {
        auto processor = m_activeAIVideoProcessors.take(taskId);
        if (processor) {
            processor->cancel();
            if (m_shutdownInProgress) {
                processor->waitForFinished(100);
                if (processor->isProcessing()) {
                    stashProcessorForShutdown(processor);
                }
            } else {
                m_dyingProcessors[taskId] = processor;
            }
        }
    }

    cancelVideoProcessing(taskId);

    if (m_activeImageProcessors.contains(taskId)) {
        auto imgProcessor = m_activeImageProcessors.take(taskId);
        if (imgProcessor) {
            imgProcessor->cancel();
            if (m_shutdownInProgress && imgProcessor->isProcessing()) {
                stashProcessorForShutdown(imgProcessor);
            }
        }
    }

    TaskStateManager::instance()->unregisterActiveTask(taskId);

    // 【修复】立即释放资源，避免下一个任务启动时资源检查失败
    m_resourceManager->release(taskId);

    m_taskLifecycle[taskId] = TaskLifecycle::Cleaning;

    if (m_shutdownInProgress) {
        finalizeTaskTermination(taskId);
        return;
    }

    QTimer::singleShot(0, this, [this, taskId]() {
        finalizeTaskTermination(taskId);
    });
}

void ProcessingController::cleanupTask(const QString& taskId)
{
    terminateTask(taskId, "cleanupTask");
}

void ProcessingController::cleanupDyingProcessors()
{
    // 清理已完成处理的dying处理器，释放内存
    QStringList toRemove;
    
    // 检查AI视频处理器
    for (auto it = m_dyingProcessors.begin(); it != m_dyingProcessors.end(); ++it) {
        const QString& taskId = it.key();
        auto& processor = it.value();
        
        if (!processor || !processor->isProcessing()) {
            toRemove.append(taskId);
        }
    }
    
    // 检查普通视频处理器
    for (auto it = m_dyingVideoProcessors.begin(); it != m_dyingVideoProcessors.end(); ++it) {
        const QString& taskId = it.key();
        auto& processor = it.value();
        
        if (!processor || !processor->isProcessing()) {
            if (!toRemove.contains(taskId)) {
                toRemove.append(taskId);
            }
        }
    }
    
    // 执行清理
    for (const QString& taskId : toRemove) {
        if (m_dyingProcessors.contains(taskId)) {
            m_dyingProcessors.remove(taskId);
        }
        if (m_dyingVideoProcessors.contains(taskId)) {
            m_dyingVideoProcessors.remove(taskId);
        }
        
        // 确保引擎也被释放
        m_aiEnginePool->release(taskId);
        
        // 更新生命周期状态
        if (m_taskLifecycle.value(taskId) == TaskLifecycle::Draining) {
            m_taskLifecycle[taskId] = TaskLifecycle::Dead;
        }
    }
    
    if (!toRemove.isEmpty()) {
    }
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
            return;
        }
        
        // 检查任务是否处于暂停状态（被用户暂停导致的取消）
        for (const auto& task : m_tasks) {
            if (task.taskId == taskId && task.status == ProcessingStatus::Paused) {
                return;
            }
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
            return;
        }

        if (!m_activeAIVideoProcessors.contains(taskId) && !m_dyingProcessors.contains(taskId)) {
            return;
        }

        m_activeAIVideoProcessors.remove(taskId);
        disconnectAiEngineForTask(taskId);

        if (lifecycle == TaskLifecycle::Draining) {

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
    const QString messageId = m_tasks[index].messageId;
    const QString fileId = m_tasks[index].fileId;
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
    } else if (oldStatus == ProcessingStatus::Paused) {
        // 删除暂停的任务
        m_tasks[index].status = ProcessingStatus::Cancelled;
        terminateTask(taskId, "cancelAndRemoveTask-Paused");
        emit taskCancelled(taskId);
        m_tasks.removeAt(index);
        result.cancelledPending = 1;
        
        // 检查该消息是否还有其他待处理/暂停的任务
        bool hasOtherPendingTasks = false;
        QString nextPendingFileId;
        for (const auto& task : m_tasks) {
            if (task.messageId == messageId && 
                (task.status == ProcessingStatus::Pending || task.status == ProcessingStatus::Paused)) {
                hasOtherPendingTasks = true;
                if (task.status == ProcessingStatus::Pending && nextPendingFileId.isEmpty()) {
                    nextPendingFileId = task.fileId;
                }
                break;
            }
        }
        
        // 如果消息仍在暂停集合中，且有其他待处理任务，将下一个待处理任务标记为暂停
        if (m_pausedMessageIds.contains(messageId) && !nextPendingFileId.isEmpty()) {
            QString sessionId = resolveSessionIdForMessage(messageId);
            for (auto& task : m_tasks) {
                if (task.messageId == messageId && task.fileId == nextPendingFileId) {
                    task.status = ProcessingStatus::Paused;
                    updateFileStatusForSessionMessage(sessionId, messageId, nextPendingFileId,
                        ProcessingStatus::Paused, QString());
                    break;
                }
            }
        }
        
        // 如果没有其他待处理任务，从暂停集合中移除该消息
        if (!hasOtherPendingTasks && m_pausedMessageIds.contains(messageId)) {
            m_pausedMessageIds.remove(messageId);
        }
    }

    return result;
}

QString ProcessingController::findNextPendingMessageId() const
{
    // 按 FIFO 顺序查找第一个有待处理任务的消息
    QSet<QString> seenMessageIds;
    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Pending && !seenMessageIds.contains(task.messageId)) {
            // 跳过已经在暂停集合中的消息
            if (!m_pausedMessageIds.contains(task.messageId)) {
                return task.messageId;
            }
            seenMessageIds.insert(task.messageId);
        }
    }
    return QString();
}

} // namespace EnhanceVision
