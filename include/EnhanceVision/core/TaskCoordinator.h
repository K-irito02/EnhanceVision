/**
 * @file TaskCoordinator.h
 * @brief 任务协调器 - 管理任务与会话/消息的关联关系，协调取消和清理操作
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_TASKCOORDINATOR_H
#define ENHANCEVISION_TASKCOORDINATOR_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QWaitCondition>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

/**
 * @brief 任务协调器
 * 
 * 负责：
 * - 维护 sessionId/messageId/taskId 的关联关系
 * - 协调任务取消和资源清理
 * - 处理孤儿任务（关联的消息/会话已删除）
 * - 提供任务状态查询
 */
class TaskCoordinator : public QObject {
    Q_OBJECT

public:
    static TaskCoordinator* instance();

    void registerTask(const TaskContext& context);
    void unregisterTask(const QString& taskId);
    
    void cancelMessageTasks(const QString& messageId);
    void cancelSessionTasks(const QString& sessionId);
    void cancelTask(const QString& taskId);
    
    bool isOrphaned(const QString& taskId) const;
    bool isMessageOrphaned(const QString& messageId) const;
    bool isSessionOrphaned(const QString& sessionId) const;
    
    TaskContext getTaskContext(const QString& taskId) const;
    QSet<QString> messageTaskIds(const QString& messageId) const;
    QSet<QString> sessionTaskIds(const QString& sessionId) const;
    
    void markMessageOrphaned(const QString& messageId);
    void markSessionOrphaned(const QString& sessionId);
    
    int pendingTaskCount(const QString& messageId) const;
    int runningTaskCount(const QString& messageId) const;
    
    void updateTaskState(const QString& taskId, TaskState state);
    TaskState getTaskState(const QString& taskId) const;
    
    void setCancelToken(const QString& taskId, std::shared_ptr<std::atomic<bool>> token);
    void setPauseToken(const QString& taskId, std::shared_ptr<std::atomic<bool>> token);
    
    void triggerCancellation(const QString& taskId);
    void triggerPause(const QString& taskId, bool paused);
    
    bool waitForCancellation(const QString& taskId, int timeoutMs = 5000);
    bool waitForAllMessageTasksCancelled(const QString& messageId, int timeoutMs = 5000);
    bool waitForAllSessionTasksCancelled(const QString& sessionId, int timeoutMs = 5000);
    
    QList<QString> allTaskIds() const;
    int totalTaskCount() const;
    void clear();

signals:
    void taskRegistered(const QString& taskId);
    void taskUnregistered(const QString& taskId);
    void taskOrphaned(const QString& taskId);
    void taskCancelled(const QString& taskId, const QString& reason);
    void taskStateChanged(const QString& taskId, TaskState newState);
    
    void messageTasksCancelled(const QString& messageId);
    void sessionTasksCancelled(const QString& sessionId);

private:
    explicit TaskCoordinator(QObject* parent = nullptr);
    ~TaskCoordinator() override = default;
    
    Q_DISABLE_COPY(TaskCoordinator)

    mutable QMutex m_mutex;
    QHash<QString, TaskContext> m_taskContexts;
    QHash<QString, QSet<QString>> m_sessionToTasks;
    QHash<QString, QSet<QString>> m_messageToTasks;
    
    QSet<QString> m_orphanedMessages;
    QSet<QString> m_orphanedSessions;
    
    QHash<QString, QWaitCondition*> m_cancelWaitConditions;
    
    static TaskCoordinator* s_instance;
    
    void cleanupWaitCondition(const QString& taskId);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_TASKCOORDINATOR_H
