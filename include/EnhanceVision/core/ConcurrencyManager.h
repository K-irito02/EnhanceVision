/**
 * @file ConcurrencyManager.h
 * @brief 并发管理器 - 统一门面，整合优先级队列、调度、超时、重试、死锁检测
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_CONCURRENCYMANAGER_H
#define ENHANCEVISION_CONCURRENCYMANAGER_H

#include <QObject>
#include <QHash>
#include <QString>
#include "EnhanceVision/core/PriorityTaskQueue.h"
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class TaskTimeoutWatchdog;
class TaskRetryPolicy;
class PriorityAdjuster;
class DeadlockDetector;
class ResourceManager;

struct SessionSlot {
    QString sessionId;
    int activeTasks = 0;
    int maxTasks = 0;
};

struct MessageSlot {
    QString messageId;
    QString sessionId;
    int activeTasks = 0;
    int maxTasks = 0;
};

class ConcurrencyManager : public QObject
{
    Q_OBJECT

public:
    explicit ConcurrencyManager(QObject* parent = nullptr);
    ~ConcurrencyManager() override;

    // 任务入队
    void enqueueTask(const PriorityTaskEntry& entry);
    void removeTask(const QString& taskId);

    // 调度：获取下一个可执行的任务（考虑并发限制）
    bool hasSchedulableTask(int maxSessions, int maxFilesPerMessage) const;
    PriorityTaskEntry scheduleNext(int maxSessions, int maxFilesPerMessage);

    // 并发槽位管理
    void acquireSlot(const QString& taskId, const QString& sessionId, const QString& messageId);
    void releaseSlot(const QString& taskId, const QString& sessionId, const QString& messageId);

    int activeTasksForSession(const QString& sessionId) const;
    int activeTasksForMessage(const QString& messageId) const;
    int activeSessionCount() const;

    // 会话切换
    void onSessionChanged(const QString& activeSessionId);

    // 子组件访问
    PriorityTaskQueue* queue();
    TaskTimeoutWatchdog* watchdog();
    TaskRetryPolicy* retryPolicy();
    PriorityAdjuster* adjuster();
    DeadlockDetector* deadlockDetector();

    // 生命周期
    void start();
    void stop();

    // 队列信息
    int pendingTaskCount() const;
    int totalActiveSlots() const;

signals:
    void taskScheduled(const QString& taskId);
    void taskTimedOut(const QString& taskId);
    void taskRetryScheduled(const QString& taskId, int delayMs, int attempt);
    void taskRetriesExhausted(const QString& taskId, int totalAttempts, const QString& lastError);
    void deadlockDetected(const QStringList& participants);
    void deadlockRecoveryRequested(const QString& taskIdToCancel);
    void starvationDetected(const QString& taskId, qint64 waitMs);

private:
    bool canScheduleInSession(const QString& sessionId, int maxSessions, int maxFilesPerMessage) const;
    bool canScheduleInMessage(const QString& messageId, int maxFilesPerMessage) const;

    PriorityTaskQueue m_queue;
    TaskTimeoutWatchdog* m_watchdog;
    TaskRetryPolicy* m_retryPolicy;
    PriorityAdjuster* m_adjuster;
    DeadlockDetector* m_deadlockDetector;

    QHash<QString, int> m_activeTasksPerSession;
    QHash<QString, int> m_activeTasksPerMessage;
    QHash<QString, QString> m_taskToSession;
    QHash<QString, QString> m_taskToMessage;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_CONCURRENCYMANAGER_H
