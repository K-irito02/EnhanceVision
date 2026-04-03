/**
 * @file AITaskQueue.h
 * @brief 线程安全的 AI 任务队列
 * @author EnhanceVision Team
 * 
 * 提供线程安全的任务队列管理。
 */

#ifndef ENHANCEVISION_AITASKQUEUE_H
#define ENHANCEVISION_AITASKQUEUE_H

#include "AITaskTypes.h"
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <deque>
#include <unordered_map>
#include <memory>
#include <optional>

namespace EnhanceVision {

/**
 * @brief 线程安全的 AI 任务队列
 * 
 * 特性：
 * - 严格 FIFO 顺序
 * - 支持按 taskId/messageId/sessionId 取消
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
     * @param timeoutMs 超时时间，-1 表示无限等待
     * @return 任务项
     */
    std::optional<AITaskItem> dequeue(int timeoutMs = -1);
    
    /**
     * @brief 查看队首任务但不移除
     */
    std::optional<AITaskItem> peek() const;
    
    /**
     * @brief 按任务 ID 取消
     */
    bool cancelByTaskId(const QString& taskId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 按消息 ID 取消所有相关任务
     */
    int cancelByMessageId(const QString& messageId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 按会话 ID 取消所有相关任务
     */
    int cancelBySessionId(const QString& sessionId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 取消所有待处理任务
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
     * @brief 关闭队列
     */
    void shutdown();
    
    /**
     * @brief 检查队列是否已关闭
     */
    bool isShutdown() const;

signals:
    void taskEnqueued(const QString& taskId);
    void taskCancelled(const QString& taskId, AICancelReason reason);
    void taskStateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    void queueSizeChanged(int newSize);

private:
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    std::deque<AITaskItem> m_queue;
    std::unordered_map<std::string, size_t> m_taskIndex;
    bool m_shutdown = false;
    
    void rebuildIndex();
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AITASKQUEUE_H
