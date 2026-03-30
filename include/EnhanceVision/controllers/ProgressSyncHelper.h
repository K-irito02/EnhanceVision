#ifndef ENHANCEVISION_PROGRESSSYNCHELPER_H
#define ENHANCEVISION_PROGRESSSYNCHELPER_H

#include <QObject>
#include <QHash>
#include <QList>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class SessionController;
class MessageModel;
class TaskCoordinator;
class SessionSyncHelper;

struct QueueTask;
struct TaskContext;

class ProgressSyncHelper : public QObject
{
    Q_OBJECT

public:
    explicit ProgressSyncHelper(QObject* parent = nullptr);
    ~ProgressSyncHelper() override;

    void setSessionController(SessionController* controller);
    void setMessageModel(MessageModel* model);
    void setTaskCoordinator(TaskCoordinator* coordinator);
    void setSessionSyncHelper(SessionSyncHelper* helper);

    void updateFileStatusForSessionMessage(const QString& sessionId, const QString& messageId,
                                           const QString& fileId, ProcessingStatus status,
                                           const QString& resultPath);
    
    void updateErrorForSessionMessage(const QString& sessionId, const QString& messageId,
                                      const QString& errorMessage);
    
    void updateProgressForSessionMessage(const QString& sessionId, const QString& messageId, int progress);
    
    void updateStatusForSessionMessage(const QString& sessionId, const QString& messageId, ProcessingStatus status);
    
    void updateQueuePositionForSessionMessage(const QString& sessionId, const QString& messageId, int queuePosition);

    void syncMessageProgress(const QString& messageId, const QString& sessionId,
                             const QList<QueueTask>& tasks,
                             const QHash<QString, TaskContext>& taskContexts);
    
    void syncMessageStatus(const QString& messageId, const QString& sessionId,
                           const QList<QueueTask>& tasks,
                           const QHash<QString, TaskContext>& taskContexts);

    QString resolveSessionIdForMessage(const QString& messageId, const QString& fallbackSessionId,
                                       const QHash<QString, TaskContext>& taskContexts) const;
    
    Message messageForSession(const QString& sessionId, const QString& messageId) const;

private:
    SessionController* m_sessionController = nullptr;
    MessageModel* m_messageModel = nullptr;
    TaskCoordinator* m_taskCoordinator = nullptr;
    SessionSyncHelper* m_sessionSyncHelper = nullptr;
    
    QHash<QString, qint64> m_lastMessageProgressSyncMs;
    QHash<QString, qint64> m_lastMessageStatusSyncMs;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROGRESSSYNCHELPER_H
