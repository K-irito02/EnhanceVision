/**
 * @file AITaskQueue.h
 * @brief 线程安全的AI任务队列
 * @author EnhanceVision Team
 * 
 * 提供线程安全的任务队列管理，支持：
 * - 任务入队/出队
 * - 优先级排序
 * - 任务取消
 * - 状态查询
 */

#ifndef ENHANCEVISION_AITASKQUEUE_H
#define ENHANCEVISION_AITASKQUEUE_H

#include "AIInferenceTypes.h"
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <deque>
#include <unordered_map>
#include <memory>
#include <optional>

namespace EnhanceVision {

/**
 * @brief 任务队列项
 */
struct AITaskItem {
    AITaskRequest request;
    AITaskState state;
    AICancelReason cancelReason;
    QDateTime queuedTime;
    QDateTime stateChangeTime;
    
    AITaskItem() 
        : state(AITaskState::Idle)
        , cancelReason(AICancelReason::None)
        , queuedTime(QDateTime::currentDateTime())
        , stateChangeTime(QDateTime::currentDateTime())
    {}
    
    explicit AITaskItem(const AITaskRequest& req)
        : request(req)
        , state(AITaskState::Queued)
        , cancelReason(AICancelReason::None)
        , queuedTime(QDateTime::currentDateTime())
        , stateChangeTime(QDateTime::currentDateTime())
    {}
};

/**
 * @brief 线程安全的AI任务队列
 * 
 * 特性：
 * - 严格FIFO顺序（同优先级）
 * - 支持按taskId/messageId/sessionId取消
 * - 阻塞式等待和超时等待
 * - 状态查询
 */
class AITaskQueue : public QObject
{
    Q_OBJECT
    
public:
    explicit AITaskQueue(QObject* parent = nullptr);
    ~AITaskQueue() override;
    
    /**
     * @brief 添加任务到队列
     * @param request 任务请求
     * @return true 如果成功入队
     */
    bool enqueue(const AITaskRequest& request);
    
    /**
     * @brief 取出下一个待处理任务（阻塞）
     * @param timeoutMs 超时时间，-1表示无限等待
     * @return 任务项，如果超时或队列关闭则返回nullopt
     */
    std::optional<AITaskItem> dequeue(int timeoutMs = -1);
    
    /**
     * @brief 查看队首任务但不移除
     * @return 任务项，如果队列为空则返回nullopt
     */
    std::optional<AITaskItem> peek() const;
    
    /**
     * @brief 按任务ID取消任务
     * @param taskId 任务ID
     * @param reason 取消原因
     * @return true 如果找到并取消了任务
     */
    bool cancelByTaskId(const QString& taskId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 按消息ID取消所有相关任务
     * @param messageId 消息ID
     * @param reason 取消原因
     * @return 取消的任务数量
     */
    int cancelByMessageId(const QString& messageId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 按会话ID取消所有相关任务
     * @param sessionId 会话ID
     * @param reason 取消原因
     * @return 取消的任务数量
     */
    int cancelBySessionId(const QString& sessionId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 取消所有待处理任务
     * @param reason 取消原因
     * @return 取消的任务数量
     */
    int cancelAll(AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 检查任务是否存在
     */
    bool contains(const QString& taskId) const;
    
    /**
     * @brief 获取任务状态
     */
    std::optional<AITaskState> getTaskState(const QString& taskId) const;
    
    /**
     * @brief 更新任务状态
     */
    bool updateTaskState(const QString& taskId, AITaskState newState);
    
    /**
     * @brief 获取队列大小
     */
    int size() const;
    
    /**
     * @brief 检查队列是否为空
     */
    bool isEmpty() const;
    
    /**
     * @brief 获取指定消息的待处理任务数
     */
    int pendingCountForMessage(const QString& messageId) const;
    
    /**
     * @brief 清空队列
     */
    void clear();
    
    /**
     * @brief 关闭队列（唤醒所有等待线程）
     */
    void shutdown();
    
    /**
     * @brief 检查队列是否已关闭
     */
    bool isShutdown() const;

signals:
    /**
     * @brief 任务入队信号
     */
    void taskEnqueued(const QString& taskId);
    
    /**
     * @brief 任务取消信号
     */
    void taskCancelled(const QString& taskId, AICancelReason reason);
    
    /**
     * @brief 任务状态变化信号
     */
    void taskStateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    
    /**
     * @brief 队列大小变化信号
     */
    void queueSizeChanged(int newSize);

private:
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    std::deque<AITaskItem> m_queue;
    std::unordered_map<std::string, size_t> m_taskIndex;  // taskId -> queue index
    bool m_shutdown;
    
    void rebuildIndex();
    void emitTaskCancelled(const QString& taskId, AICancelReason reason);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AITASKQUEUE_H
