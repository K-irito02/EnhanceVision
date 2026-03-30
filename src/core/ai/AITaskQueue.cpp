/**
 * @file AITaskQueue.cpp
 * @brief 线程安全的AI任务队列实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/ai/AITaskQueue.h"
#include <QDebug>
#include <algorithm>

namespace EnhanceVision {

AITaskQueue::AITaskQueue(QObject* parent)
    : QObject(parent)
    , m_shutdown(false)
{
}

AITaskQueue::~AITaskQueue()
{
    shutdown();
}

bool AITaskQueue::enqueue(const AITaskRequest& request)
{
    if (request.taskId.isEmpty()) {
        qWarning() << "[AITaskQueue] Cannot enqueue task with empty taskId";
        return false;
    }
    
    {
        QMutexLocker locker(&m_mutex);
        
        if (m_shutdown) {
            qWarning() << "[AITaskQueue] Cannot enqueue: queue is shutdown";
            return false;
        }
        
        // 检查是否已存在
        std::string taskIdStd = request.taskId.toStdString();
        if (m_taskIndex.find(taskIdStd) != m_taskIndex.end()) {
            qWarning() << "[AITaskQueue] Task already exists:" << request.taskId;
            return false;
        }
        
        // 创建任务项
        AITaskItem item(request);
        
        // 添加到队列
        m_queue.push_back(std::move(item));
        m_taskIndex[taskIdStd] = m_queue.size() - 1;
        
        qInfo() << "[AITaskQueue] Task enqueued:" << request.taskId
                << "queueSize:" << m_queue.size();
    }
    
    // 唤醒等待的消费者
    m_condition.wakeOne();
    
    // 发出信号
    emit taskEnqueued(request.taskId);
    emit queueSizeChanged(static_cast<int>(m_queue.size()));
    
    return true;
}

std::optional<AITaskItem> AITaskQueue::dequeue(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    
    // 等待任务或关闭
    while (m_queue.empty() && !m_shutdown) {
        if (timeoutMs < 0) {
            m_condition.wait(&m_mutex);
        } else {
            if (!m_condition.wait(&m_mutex, timeoutMs)) {
                // 超时
                return std::nullopt;
            }
        }
    }
    
    if (m_shutdown || m_queue.empty()) {
        return std::nullopt;
    }
    
    // 找到第一个非取消的任务
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (it->state == AITaskState::Queued) {
            AITaskItem item = std::move(*it);
            m_queue.erase(it);
            rebuildIndex();
            
            qInfo() << "[AITaskQueue] Task dequeued:" << item.request.taskId
                    << "remainingSize:" << m_queue.size();
            
            // 更新状态
            item.state = AITaskState::Preparing;
            item.stateChangeTime = QDateTime::currentDateTime();
            
            emit queueSizeChanged(static_cast<int>(m_queue.size()));
            
            return item;
        }
    }
    
    return std::nullopt;
}

std::optional<AITaskItem> AITaskQueue::peek() const
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto& item : m_queue) {
        if (item.state == AITaskState::Queued) {
            return item;
        }
    }
    
    return std::nullopt;
}

bool AITaskQueue::cancelByTaskId(const QString& taskId, AICancelReason reason)
{
    QMutexLocker locker(&m_mutex);
    
    std::string taskIdStd = taskId.toStdString();
    auto indexIt = m_taskIndex.find(taskIdStd);
    
    if (indexIt == m_taskIndex.end()) {
        return false;
    }
    
    size_t index = indexIt->second;
    if (index >= m_queue.size()) {
        qWarning() << "[AITaskQueue] Invalid index for task:" << taskId;
        return false;
    }
    
    AITaskItem& item = m_queue[index];
    
    // 只能取消等待中的任务
    if (item.state != AITaskState::Queued) {
        qInfo() << "[AITaskQueue] Cannot cancel task in state:" 
                << aiTaskStateToString(item.state) << "taskId:" << taskId;
        return false;
    }
    
    AITaskState oldState = item.state;
    item.state = AITaskState::Cancelled;
    item.cancelReason = reason;
    item.stateChangeTime = QDateTime::currentDateTime();
    
    qInfo() << "[AITaskQueue] Task cancelled:" << taskId << "reason:" << static_cast<int>(reason);
    
    locker.unlock();
    
    emit taskStateChanged(taskId, oldState, AITaskState::Cancelled);
    emit taskCancelled(taskId, reason);
    
    return true;
}

int AITaskQueue::cancelByMessageId(const QString& messageId, AICancelReason reason)
{
    QMutexLocker locker(&m_mutex);
    
    int cancelledCount = 0;
    QStringList taskIds;
    
    for (auto& item : m_queue) {
        if (item.request.messageId == messageId && item.state == AITaskState::Queued) {
            item.state = AITaskState::Cancelled;
            item.cancelReason = reason;
            item.stateChangeTime = QDateTime::currentDateTime();
            taskIds.append(item.request.taskId);
            cancelledCount++;
        }
    }
    
    locker.unlock();
    
    // 发出信号
    for (const QString& taskId : taskIds) {
        emit taskStateChanged(taskId, AITaskState::Queued, AITaskState::Cancelled);
        emit taskCancelled(taskId, reason);
    }
    
    qInfo() << "[AITaskQueue] Cancelled" << cancelledCount << "tasks for message:" << messageId;
    
    return cancelledCount;
}

int AITaskQueue::cancelBySessionId(const QString& sessionId, AICancelReason reason)
{
    QMutexLocker locker(&m_mutex);
    
    int cancelledCount = 0;
    QStringList taskIds;
    
    for (auto& item : m_queue) {
        if (item.request.sessionId == sessionId && item.state == AITaskState::Queued) {
            item.state = AITaskState::Cancelled;
            item.cancelReason = reason;
            item.stateChangeTime = QDateTime::currentDateTime();
            taskIds.append(item.request.taskId);
            cancelledCount++;
        }
    }
    
    locker.unlock();
    
    // 发出信号
    for (const QString& taskId : taskIds) {
        emit taskStateChanged(taskId, AITaskState::Queued, AITaskState::Cancelled);
        emit taskCancelled(taskId, reason);
    }
    
    qInfo() << "[AITaskQueue] Cancelled" << cancelledCount << "tasks for session:" << sessionId;
    
    return cancelledCount;
}

int AITaskQueue::cancelAll(AICancelReason reason)
{
    QMutexLocker locker(&m_mutex);
    
    int cancelledCount = 0;
    QStringList taskIds;
    
    for (auto& item : m_queue) {
        if (item.state == AITaskState::Queued) {
            item.state = AITaskState::Cancelled;
            item.cancelReason = reason;
            item.stateChangeTime = QDateTime::currentDateTime();
            taskIds.append(item.request.taskId);
            cancelledCount++;
        }
    }
    
    locker.unlock();
    
    // 发出信号
    for (const QString& taskId : taskIds) {
        emit taskStateChanged(taskId, AITaskState::Queued, AITaskState::Cancelled);
        emit taskCancelled(taskId, reason);
    }
    
    qInfo() << "[AITaskQueue] Cancelled all" << cancelledCount << "tasks";
    
    return cancelledCount;
}

bool AITaskQueue::contains(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_taskIndex.find(taskId.toStdString()) != m_taskIndex.end();
}

std::optional<AITaskState> AITaskQueue::getTaskState(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    
    std::string taskIdStd = taskId.toStdString();
    auto indexIt = m_taskIndex.find(taskIdStd);
    
    if (indexIt == m_taskIndex.end()) {
        return std::nullopt;
    }
    
    if (indexIt->second >= m_queue.size()) {
        return std::nullopt;
    }
    
    return m_queue[indexIt->second].state;
}

bool AITaskQueue::updateTaskState(const QString& taskId, AITaskState newState)
{
    QMutexLocker locker(&m_mutex);
    
    std::string taskIdStd = taskId.toStdString();
    auto indexIt = m_taskIndex.find(taskIdStd);
    
    if (indexIt == m_taskIndex.end()) {
        return false;
    }
    
    if (indexIt->second >= m_queue.size()) {
        return false;
    }
    
    AITaskItem& item = m_queue[indexIt->second];
    AITaskState oldState = item.state;
    item.state = newState;
    item.stateChangeTime = QDateTime::currentDateTime();
    
    locker.unlock();
    
    emit taskStateChanged(taskId, oldState, newState);
    
    return true;
}

int AITaskQueue::size() const
{
    QMutexLocker locker(&m_mutex);
    
    int count = 0;
    for (const auto& item : m_queue) {
        if (item.state == AITaskState::Queued) {
            count++;
        }
    }
    return count;
}

bool AITaskQueue::isEmpty() const
{
    return size() == 0;
}

int AITaskQueue::pendingCountForMessage(const QString& messageId) const
{
    QMutexLocker locker(&m_mutex);
    
    int count = 0;
    for (const auto& item : m_queue) {
        if (item.request.messageId == messageId && item.state == AITaskState::Queued) {
            count++;
        }
    }
    return count;
}

void AITaskQueue::clear()
{
    QMutexLocker locker(&m_mutex);
    
    // 取消所有任务
    QStringList taskIds;
    for (auto& item : m_queue) {
        if (item.state == AITaskState::Queued) {
            item.state = AITaskState::Cancelled;
            item.cancelReason = AICancelReason::SystemShutdown;
            taskIds.append(item.request.taskId);
        }
    }
    
    m_queue.clear();
    m_taskIndex.clear();
    
    locker.unlock();
    
    for (const QString& taskId : taskIds) {
        emit taskCancelled(taskId, AICancelReason::SystemShutdown);
    }
    
    emit queueSizeChanged(0);
    
    qInfo() << "[AITaskQueue] Queue cleared";
}

void AITaskQueue::shutdown()
{
    {
        QMutexLocker locker(&m_mutex);
        m_shutdown = true;
    }
    
    // 唤醒所有等待的线程
    m_condition.wakeAll();
    
    // 清理队列
    clear();
    
    qInfo() << "[AITaskQueue] Queue shutdown";
}

bool AITaskQueue::isShutdown() const
{
    QMutexLocker locker(&m_mutex);
    return m_shutdown;
}

void AITaskQueue::rebuildIndex()
{
    m_taskIndex.clear();
    for (size_t i = 0; i < m_queue.size(); ++i) {
        m_taskIndex[m_queue[i].request.taskId.toStdString()] = i;
    }
}

void AITaskQueue::emitTaskCancelled(const QString& taskId, AICancelReason reason)
{
    emit taskCancelled(taskId, reason);
}

} // namespace EnhanceVision
