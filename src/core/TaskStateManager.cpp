/**
 * @file TaskStateManager.cpp
 * @brief 任务状态管理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/TaskStateManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QWriteLocker>
#include <QReadLocker>

namespace EnhanceVision {

TaskStateManager* TaskStateManager::s_instance = nullptr;

TaskStateManager* TaskStateManager::instance()
{
    if (!s_instance) {
        s_instance = new TaskStateManager(QCoreApplication::instance());
    }
    return s_instance;
}

TaskStateManager::TaskStateManager(QObject* parent)
    : QObject(parent)
{
}

void TaskStateManager::registerActiveTask(const QString& taskId, const QString& messageId,
                                          const QString& sessionId, const QString& fileId)
{
    if (taskId.isEmpty()) {
        qWarning() << "[TaskStateManager] registerActiveTask: taskId is empty";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_activeTasks.contains(taskId)) {
        qWarning() << "[TaskStateManager] Task already registered:" << taskId;
        return;
    }
    
    ActiveTaskInfo info(taskId, messageId, sessionId, fileId);
    m_activeTasks[taskId] = info;
    
    if (!messageId.isEmpty()) {
        m_messageToTasks[messageId].insert(taskId);
    }
    
    if (!sessionId.isEmpty()) {
        m_sessionToTasks[sessionId].insert(taskId);
    }
    
    locker.unlock();
    emit taskRegistered(taskId, messageId, sessionId);
}

void TaskStateManager::unregisterActiveTask(const QString& taskId)
{
    if (taskId.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (!m_activeTasks.contains(taskId)) {
        return;
    }
    
    ActiveTaskInfo info = m_activeTasks.take(taskId);
    
    if (!info.messageId.isEmpty() && m_messageToTasks.contains(info.messageId)) {
        m_messageToTasks[info.messageId].remove(taskId);
        if (m_messageToTasks[info.messageId].isEmpty()) {
            m_messageToTasks.remove(info.messageId);
        }
    }
    
    if (!info.sessionId.isEmpty() && m_sessionToTasks.contains(info.sessionId)) {
        m_sessionToTasks[info.sessionId].remove(taskId);
        if (m_sessionToTasks[info.sessionId].isEmpty()) {
            m_sessionToTasks.remove(info.sessionId);
        }
    }
    
    locker.unlock();
    emit taskUnregistered(taskId);
}

bool TaskStateManager::isTaskActive(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_activeTasks.contains(taskId);
}

bool TaskStateManager::isMessageProcessing(const QString& messageId) const
{
    QMutexLocker locker(&m_mutex);
    return m_messageToTasks.contains(messageId) && !m_messageToTasks[messageId].isEmpty();
}

bool TaskStateManager::isSessionProcessing(const QString& sessionId) const
{
    QMutexLocker locker(&m_mutex);
    return m_sessionToTasks.contains(sessionId) && !m_sessionToTasks[sessionId].isEmpty();
}

ActiveTaskInfo TaskStateManager::getActiveTaskInfo(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_activeTasks.value(taskId);
}

QList<ActiveTaskInfo> TaskStateManager::getActiveTasksForSession(const QString& sessionId) const
{
    QMutexLocker locker(&m_mutex);
    QList<ActiveTaskInfo> result;
    
    if (!m_sessionToTasks.contains(sessionId)) {
        return result;
    }
    
    const QSet<QString>& taskIds = m_sessionToTasks[sessionId];
    result.reserve(taskIds.size());
    
    for (const QString& taskId : taskIds) {
        if (m_activeTasks.contains(taskId)) {
            result.append(m_activeTasks[taskId]);
        }
    }
    
    return result;
}

QList<ActiveTaskInfo> TaskStateManager::getActiveTasksForMessage(const QString& messageId) const
{
    QMutexLocker locker(&m_mutex);
    QList<ActiveTaskInfo> result;
    
    if (!m_messageToTasks.contains(messageId)) {
        return result;
    }
    
    const QSet<QString>& taskIds = m_messageToTasks[messageId];
    result.reserve(taskIds.size());
    
    for (const QString& taskId : taskIds) {
        if (m_activeTasks.contains(taskId)) {
            result.append(m_activeTasks[taskId]);
        }
    }
    
    return result;
}

QList<ActiveTaskInfo> TaskStateManager::getAllActiveTasks() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeTasks.values();
}

void TaskStateManager::updateTaskProgress(const QString& taskId, int progress)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_activeTasks.contains(taskId)) {
        return;
    }
    
    ActiveTaskInfo& info = m_activeTasks[taskId];
    info.lastProgress = progress;
    info.lastUpdateMs = QDateTime::currentMSecsSinceEpoch();
    
    locker.unlock();
    emit taskProgressUpdated(taskId, progress);
}

bool TaskStateManager::isTaskStale(const QString& taskId, qint64 timeoutMs) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_activeTasks.contains(taskId)) {
        return true;
    }
    
    const ActiveTaskInfo& info = m_activeTasks[taskId];
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsedMs = nowMs - info.lastUpdateMs;
    
    return elapsedMs > timeoutMs;
}

int TaskStateManager::activeTaskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeTasks.size();
}

int TaskStateManager::sessionActiveTaskCount(const QString& sessionId) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_sessionToTasks.contains(sessionId)) {
        return 0;
    }
    return m_sessionToTasks[sessionId].size();
}

void TaskStateManager::clear()
{
    QMutexLocker locker(&m_mutex);
    
    m_activeTasks.clear();
    m_messageToTasks.clear();
    m_sessionToTasks.clear();
}

} // namespace EnhanceVision
