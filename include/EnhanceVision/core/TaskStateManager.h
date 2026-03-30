/**
 * @file TaskStateManager.h
 * @brief 任务状态管理器 - 统一管理所有任务的生命周期状态
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_TASKSTATEMANAGER_H
#define ENHANCEVISION_TASKSTATEMANAGER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QList>
#include <QDateTime>

namespace EnhanceVision {

/**
 * @brief 活动任务信息结构
 */
struct ActiveTaskInfo {
    QString taskId;
    QString messageId;
    QString sessionId;
    QString fileId;
    qint64 startTimeMs = 0;
    qint64 lastUpdateMs = 0;
    int lastProgress = 0;
    
    ActiveTaskInfo() = default;
    
    ActiveTaskInfo(const QString& tid, const QString& mid, 
                   const QString& sid, const QString& fid)
        : taskId(tid)
        , messageId(mid)
        , sessionId(sid)
        , fileId(fid)
        , startTimeMs(QDateTime::currentMSecsSinceEpoch())
        , lastUpdateMs(startTimeMs)
        , lastProgress(0)
    {}
};

/**
 * @brief 任务状态管理器
 * 
 * 职责:
 * 1. 维护活动任务注册表 (正在执行的任务)
 * 2. 区分"正在执行"与"真正中断"的任务
 * 3. 提供任务状态查询接口
 * 4. 支持跨会话任务状态同步
 * 
 * 使用场景:
 * - ProcessingController 在任务开始时注册，结束时注销
 * - SessionController 在会话切换时查询任务是否正在执行
 * - 用于解决会话切换导致任务被误判为中断的问题
 */
class TaskStateManager : public QObject {
    Q_OBJECT

public:
    static TaskStateManager* instance();
    
    void registerActiveTask(const QString& taskId, const QString& messageId,
                           const QString& sessionId, const QString& fileId);
    
    void unregisterActiveTask(const QString& taskId);
    
    bool isTaskActive(const QString& taskId) const;
    
    bool isMessageProcessing(const QString& messageId) const;
    
    bool isSessionProcessing(const QString& sessionId) const;
    
    ActiveTaskInfo getActiveTaskInfo(const QString& taskId) const;
    
    QList<ActiveTaskInfo> getActiveTasksForSession(const QString& sessionId) const;
    
    QList<ActiveTaskInfo> getActiveTasksForMessage(const QString& messageId) const;
    
    QList<ActiveTaskInfo> getAllActiveTasks() const;
    
    void updateTaskProgress(const QString& taskId, int progress);
    
    bool isTaskStale(const QString& taskId, qint64 timeoutMs = 30000) const;
    
    bool validateTaskConsistency(const QString& messageId, 
                                  const QString& expectedStatus) const;
    
    void cleanupStaleTasks(qint64 timeoutMs = 30000);
    
    int activeTaskCount() const;
    
    int sessionActiveTaskCount(const QString& sessionId) const;
    
    void clear();

signals:
    void taskRegistered(const QString& taskId, const QString& messageId, 
                       const QString& sessionId);
    void taskUnregistered(const QString& taskId);
    void taskProgressUpdated(const QString& taskId, int progress);
    void taskStaleDetected(const QString& taskId);

private:
    explicit TaskStateManager(QObject* parent = nullptr);
    ~TaskStateManager() override = default;
    
    Q_DISABLE_COPY(TaskStateManager)

    mutable QMutex m_mutex;
    QHash<QString, ActiveTaskInfo> m_activeTasks;
    QHash<QString, QSet<QString>> m_messageToTasks;
    QHash<QString, QSet<QString>> m_sessionToTasks;
    
    static TaskStateManager* s_instance;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_TASKSTATEMANAGER_H
