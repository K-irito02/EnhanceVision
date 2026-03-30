#include "EnhanceVision/controllers/ProgressSyncHelper.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/controllers/SessionSyncHelper.h"
#include "EnhanceVision/models/MessageModel.h"
#include "EnhanceVision/models/ProcessingModel.h"
#include "EnhanceVision/core/TaskCoordinator.h"
#include <QDateTime>
#include <QDebug>

namespace EnhanceVision {

ProgressSyncHelper::ProgressSyncHelper(QObject* parent)
    : QObject(parent)
{
}

ProgressSyncHelper::~ProgressSyncHelper() = default;

void ProgressSyncHelper::setSessionController(SessionController* controller)
{
    m_sessionController = controller;
}

void ProgressSyncHelper::setMessageModel(MessageModel* model)
{
    m_messageModel = model;
}

void ProgressSyncHelper::setTaskCoordinator(TaskCoordinator* coordinator)
{
    m_taskCoordinator = coordinator;
}

void ProgressSyncHelper::setSessionSyncHelper(SessionSyncHelper* helper)
{
    m_sessionSyncHelper = helper;
}

void ProgressSyncHelper::updateFileStatusForSessionMessage(const QString& sessionId, const QString& messageId,
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
                    qWarning() << "[ProgressSyncHelper] updateMessageInSession failed"
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
            qWarning() << "[ProgressSyncHelper] message not found in session while updating file status"
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

    if (sessionUpdated && m_sessionSyncHelper && !sessionId.isEmpty() &&
        m_sessionController && m_sessionController->activeSessionId() == sessionId) {
        m_sessionSyncHelper->requestSessionMemorySync(messageId);
    }
}

void ProgressSyncHelper::updateErrorForSessionMessage(const QString& sessionId, const QString& messageId,
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

    if (m_sessionSyncHelper) {
        m_sessionSyncHelper->syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, errorMessage]() {
            if (m_messageModel) {
                m_messageModel->updateErrorMessage(messageId, errorMessage);
            }
        });
    }
}

void ProgressSyncHelper::updateProgressForSessionMessage(const QString& sessionId, const QString& messageId, int progress)
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

    if (m_sessionSyncHelper) {
        m_sessionSyncHelper->syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, message]() {
            if (m_messageModel) {
                m_messageModel->updateProgress(messageId, message.progress);
            }
        });
    }
}

void ProgressSyncHelper::updateStatusForSessionMessage(const QString& sessionId, const QString& messageId, ProcessingStatus status)
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

    if (m_sessionSyncHelper) {
        m_sessionSyncHelper->syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, status]() {
            if (m_messageModel) {
                m_messageModel->updateStatus(messageId, static_cast<int>(status));
            }
        });
    }
}

void ProgressSyncHelper::updateQueuePositionForSessionMessage(const QString& sessionId, const QString& messageId, int queuePosition)
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

    if (m_sessionSyncHelper) {
        m_sessionSyncHelper->syncVisibleMessageIfNeeded(sessionId, messageId, [this, messageId, queuePosition]() {
            if (m_messageModel) {
                m_messageModel->updateQueuePosition(messageId, queuePosition);
            }
        });
    }
}

void ProgressSyncHelper::syncMessageProgress(const QString& messageId, const QString& sessionId,
                                             const QList<QueueTask>& tasks,
                                             const QHash<QString, TaskContext>& taskContexts)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kMessageProgressSyncDebounceMs = 250;

    const qint64 lastSyncMs = m_lastMessageProgressSyncMs.value(messageId, 0);
    if (lastSyncMs > 0 && (nowMs - lastSyncMs) < kMessageProgressSyncDebounceMs) {
        return;
    }

    m_lastMessageProgressSyncMs[messageId] = nowMs;

    const QString targetSessionId = resolveSessionIdForMessage(messageId, sessionId, taskContexts);
    if (targetSessionId.isEmpty()) {
        return;
    }

    int totalTasks = 0;
    int totalProgress = 0;

    for (const auto& task : tasks) {
        if (task.messageId == messageId) {
            TaskContext ctx = m_taskCoordinator ? m_taskCoordinator->getTaskContext(task.taskId) : TaskContext();
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
        }
    }

    int overallProgress = 0;
    if (totalTasks > 0) {
        overallProgress = totalProgress / totalTasks;
    }
    
    overallProgress = qBound(0, overallProgress, 100);
    
    updateProgressForSessionMessage(targetSessionId, messageId, overallProgress);
}

void ProgressSyncHelper::syncMessageStatus(const QString& messageId, const QString& sessionId,
                                           const QList<QueueTask>& tasks,
                                           const QHash<QString, TaskContext>& taskContexts)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kMessageStatusSyncDebounceMs = 300;

    const qint64 lastSyncMs = m_lastMessageStatusSyncMs.value(messageId, 0);
    if (lastSyncMs > 0 && (nowMs - lastSyncMs) < kMessageStatusSyncDebounceMs) {
        return;
    }

    m_lastMessageStatusSyncMs[messageId] = nowMs;

    const QString targetSessionId = resolveSessionIdForMessage(messageId, sessionId, taskContexts);
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
    for (const auto& task : tasks) {
        if (task.messageId == messageId && task.status == ProcessingStatus::Pending) {
            TaskContext ctx = m_taskCoordinator ? m_taskCoordinator->getTaskContext(task.taskId) : TaskContext();
            if (!ctx.sessionId.isEmpty() && ctx.sessionId != targetSessionId) {
                continue;
            }
            queuePos = task.position + 1;
            break;
        }
    }
    updateQueuePositionForSessionMessage(targetSessionId, messageId, queuePos);
}

QString ProgressSyncHelper::resolveSessionIdForMessage(const QString& messageId, const QString& fallbackSessionId,
                                                       const QHash<QString, TaskContext>& taskContexts) const
{
    if (!fallbackSessionId.isEmpty()) {
        return fallbackSessionId;
    }

    QString resolvedSessionId;
    for (auto it = taskContexts.constBegin(); it != taskContexts.constEnd(); ++it) {
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

Message ProgressSyncHelper::messageForSession(const QString& sessionId, const QString& messageId) const
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

} // namespace EnhanceVision
