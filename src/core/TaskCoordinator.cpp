/**
 * @file TaskCoordinator.cpp
 * @brief 任务协调器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/TaskCoordinator.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

namespace EnhanceVision {

TaskCoordinator* TaskCoordinator::s_instance = nullptr;

TaskCoordinator* TaskCoordinator::instance() {
    if (!s_instance) {
        s_instance = new TaskCoordinator(QCoreApplication::instance());
    }
    return s_instance;
}

TaskCoordinator::TaskCoordinator(QObject* parent)
    : QObject(parent)
{
}

void TaskCoordinator::registerTask(const TaskContext& context) {
    QMutexLocker locker(&m_mutex);
    
    m_taskContexts[context.taskId] = context;
    
    if (!context.sessionId.isEmpty()) {
        m_sessionToTasks[context.sessionId].insert(context.taskId);
    }
    
    if (!context.messageId.isEmpty()) {
        m_messageToTasks[context.messageId].insert(context.taskId);
    }
    
    m_cancelWaitConditions[context.taskId] = new QWaitCondition();
    
    emit taskRegistered(context.taskId);
}

void TaskCoordinator::unregisterTask(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_taskContexts.contains(taskId)) {
        return;
    }
    
    TaskContext context = m_taskContexts.take(taskId);
    
    if (!context.sessionId.isEmpty() && m_sessionToTasks.contains(context.sessionId)) {
        m_sessionToTasks[context.sessionId].remove(taskId);
        if (m_sessionToTasks[context.sessionId].isEmpty()) {
            m_sessionToTasks.remove(context.sessionId);
        }
    }
    
    if (!context.messageId.isEmpty() && m_messageToTasks.contains(context.messageId)) {
        m_messageToTasks[context.messageId].remove(taskId);
        if (m_messageToTasks[context.messageId].isEmpty()) {
            m_messageToTasks.remove(context.messageId);
        }
    }
    
    cleanupWaitCondition(taskId);
    
    emit taskUnregistered(taskId);
}

void TaskCoordinator::cancelMessageTasks(const QString& messageId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_messageToTasks.contains(messageId)) {
        return;
    }
    
    QSet<QString> taskIds = m_messageToTasks[messageId];
    
    for (const QString& taskId : taskIds) {
        if (m_taskContexts.contains(taskId)) {
            TaskContext& ctx = m_taskContexts[taskId];
            
            if (ctx.cancelToken) {
                ctx.cancelToken->store(true);
            }
            
            ctx.state = TaskState::Cancelling;
            
            if (m_cancelWaitConditions.contains(taskId)) {
                m_cancelWaitConditions[taskId]->wakeAll();
            }
            
            emit taskCancelled(taskId, "message_deleted");
            emit taskStateChanged(taskId, TaskState::Cancelling);
        }
    }
    
    emit messageTasksCancelled(messageId);
}

void TaskCoordinator::cancelSessionTasks(const QString& sessionId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessionToTasks.contains(sessionId)) {
        return;
    }
    
    QSet<QString> taskIds = m_sessionToTasks[sessionId];
    
    for (const QString& taskId : taskIds) {
        if (m_taskContexts.contains(taskId)) {
            TaskContext& ctx = m_taskContexts[taskId];
            
            if (ctx.cancelToken) {
                ctx.cancelToken->store(true);
            }
            
            ctx.state = TaskState::Cancelling;
            
            if (m_cancelWaitConditions.contains(taskId)) {
                m_cancelWaitConditions[taskId]->wakeAll();
            }
            
            emit taskCancelled(taskId, "session_deleted");
            emit taskStateChanged(taskId, TaskState::Cancelling);
        }
    }
    
    emit sessionTasksCancelled(sessionId);
}

void TaskCoordinator::cancelTask(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_taskContexts.contains(taskId)) {
        return;
    }
    
    TaskContext& ctx = m_taskContexts[taskId];
    
    if (ctx.cancelToken) {
        ctx.cancelToken->store(true);
    }
    
    ctx.state = TaskState::Cancelling;
    
    if (m_cancelWaitConditions.contains(taskId)) {
        m_cancelWaitConditions[taskId]->wakeAll();
    }
    
    emit taskCancelled(taskId, "user_cancelled");
    emit taskStateChanged(taskId, TaskState::Cancelling);
}

bool TaskCoordinator::isOrphaned(const QString& taskId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_taskContexts.contains(taskId)) {
        return true;
    }
    
    const TaskContext& ctx = m_taskContexts[taskId];
    
    if (ctx.state == TaskState::Orphaned) {
        return true;
    }
    
    if (!ctx.messageId.isEmpty() && m_orphanedMessages.contains(ctx.messageId)) {
        return true;
    }
    
    if (!ctx.sessionId.isEmpty() && m_orphanedSessions.contains(ctx.sessionId)) {
        return true;
    }
    
    return false;
}

bool TaskCoordinator::isMessageOrphaned(const QString& messageId) const {
    QMutexLocker locker(&m_mutex);
    return m_orphanedMessages.contains(messageId);
}

bool TaskCoordinator::isSessionOrphaned(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    return m_orphanedSessions.contains(sessionId);
}

TaskContext TaskCoordinator::getTaskContext(const QString& taskId) const {
    QMutexLocker locker(&m_mutex);
    return m_taskContexts.value(taskId);
}

QSet<QString> TaskCoordinator::messageTaskIds(const QString& messageId) const {
    QMutexLocker locker(&m_mutex);
    return m_messageToTasks.value(messageId);
}

QSet<QString> TaskCoordinator::sessionTaskIds(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    return m_sessionToTasks.value(sessionId);
}

void TaskCoordinator::markMessageOrphaned(const QString& messageId) {
    QMutexLocker locker(&m_mutex);
    
    m_orphanedMessages.insert(messageId);
    
    if (m_messageToTasks.contains(messageId)) {
        for (const QString& taskId : m_messageToTasks[messageId]) {
            if (m_taskContexts.contains(taskId)) {
                m_taskContexts[taskId].state = TaskState::Orphaned;
                emit taskOrphaned(taskId);
                emit taskStateChanged(taskId, TaskState::Orphaned);
            }
        }
    }
}

void TaskCoordinator::markSessionOrphaned(const QString& sessionId) {
    QMutexLocker locker(&m_mutex);
    
    m_orphanedSessions.insert(sessionId);
    
    if (m_sessionToTasks.contains(sessionId)) {
        for (const QString& taskId : m_sessionToTasks[sessionId]) {
            if (m_taskContexts.contains(taskId)) {
                m_taskContexts[taskId].state = TaskState::Orphaned;
                emit taskOrphaned(taskId);
                emit taskStateChanged(taskId, TaskState::Orphaned);
            }
        }
    }
}

int TaskCoordinator::pendingTaskCount(const QString& messageId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_messageToTasks.contains(messageId)) {
        return 0;
    }
    
    int count = 0;
    for (const QString& taskId : m_messageToTasks[messageId]) {
        if (m_taskContexts.contains(taskId) && 
            m_taskContexts[taskId].state == TaskState::Pending) {
            count++;
        }
    }
    return count;
}

int TaskCoordinator::runningTaskCount(const QString& messageId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_messageToTasks.contains(messageId)) {
        return 0;
    }
    
    int count = 0;
    for (const QString& taskId : m_messageToTasks[messageId]) {
        if (m_taskContexts.contains(taskId) && 
            m_taskContexts[taskId].state == TaskState::Running) {
            count++;
        }
    }
    return count;
}

void TaskCoordinator::updateTaskState(const QString& taskId, TaskState state) {
    QMutexLocker locker(&m_mutex);
    
    if (m_taskContexts.contains(taskId)) {
        m_taskContexts[taskId].state = state;
        emit taskStateChanged(taskId, state);
    }
}

TaskState TaskCoordinator::getTaskState(const QString& taskId) const {
    QMutexLocker locker(&m_mutex);
    
    if (m_taskContexts.contains(taskId)) {
        return m_taskContexts[taskId].state;
    }
    return TaskState::Orphaned;
}

void TaskCoordinator::setCancelToken(const QString& taskId, std::shared_ptr<std::atomic<bool>> token) {
    QMutexLocker locker(&m_mutex);
    
    if (m_taskContexts.contains(taskId)) {
        m_taskContexts[taskId].cancelToken = token;
    }
}

void TaskCoordinator::setPauseToken(const QString& taskId, std::shared_ptr<std::atomic<bool>> token) {
    QMutexLocker locker(&m_mutex);
    
    if (m_taskContexts.contains(taskId)) {
        m_taskContexts[taskId].pauseToken = token;
    }
}

void TaskCoordinator::triggerCancellation(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_taskContexts.contains(taskId)) {
        TaskContext& ctx = m_taskContexts[taskId];
        if (ctx.cancelToken) {
            ctx.cancelToken->store(true);
        }
        ctx.state = TaskState::Cancelling;
        
        if (m_cancelWaitConditions.contains(taskId)) {
            m_cancelWaitConditions[taskId]->wakeAll();
        }
    }
}

void TaskCoordinator::triggerPause(const QString& taskId, bool paused) {
    QMutexLocker locker(&m_mutex);
    
    if (m_taskContexts.contains(taskId)) {
        TaskContext& ctx = m_taskContexts[taskId];
        if (ctx.pauseToken) {
            ctx.pauseToken->store(paused);
        }
        ctx.state = paused ? TaskState::Paused : TaskState::Running;
    }
}

bool TaskCoordinator::waitForCancellation(const QString& taskId, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs) {
        {
            QMutexLocker locker(&m_mutex);
            if (!m_taskContexts.contains(taskId)) {
                return true;
            }
            
            TaskState state = m_taskContexts[taskId].state;
            if (state == TaskState::Cancelled || 
                state == TaskState::Completed ||
                state == TaskState::Failed ||
                state == TaskState::Orphaned) {
                return true;
            }
        }
        
        QCoreApplication::processEvents();
        QThread::usleep(50000);
    }
    
    return false;
}

bool TaskCoordinator::waitForAllMessageTasksCancelled(const QString& messageId, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs) {
        {
            QMutexLocker locker(&m_mutex);
            
            if (!m_messageToTasks.contains(messageId)) {
                return true;
            }
            
            bool allDone = true;
            for (const QString& taskId : m_messageToTasks[messageId]) {
                if (m_taskContexts.contains(taskId)) {
                    TaskState state = m_taskContexts[taskId].state;
                    if (state != TaskState::Cancelled && 
                        state != TaskState::Completed &&
                        state != TaskState::Failed &&
                        state != TaskState::Orphaned) {
                        allDone = false;
                        break;
                    }
                }
            }
            
            if (allDone) {
                return true;
            }
        }
        
        QCoreApplication::processEvents();
        QThread::usleep(50000);
    }
    
    return false;
}

bool TaskCoordinator::waitForAllSessionTasksCancelled(const QString& sessionId, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs) {
        {
            QMutexLocker locker(&m_mutex);
            
            if (!m_sessionToTasks.contains(sessionId)) {
                return true;
            }
            
            bool allDone = true;
            for (const QString& taskId : m_sessionToTasks[sessionId]) {
                if (m_taskContexts.contains(taskId)) {
                    TaskState state = m_taskContexts[taskId].state;
                    if (state != TaskState::Cancelled && 
                        state != TaskState::Completed &&
                        state != TaskState::Failed &&
                        state != TaskState::Orphaned) {
                        allDone = false;
                        break;
                    }
                }
            }
            
            if (allDone) {
                return true;
            }
        }
        
        QCoreApplication::processEvents();
        QThread::usleep(50000);
    }
    
    return false;
}

QList<QString> TaskCoordinator::allTaskIds() const {
    QMutexLocker locker(&m_mutex);
    return m_taskContexts.keys();
}

int TaskCoordinator::totalTaskCount() const {
    QMutexLocker locker(&m_mutex);
    return m_taskContexts.size();
}

void TaskCoordinator::clear() {
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_cancelWaitConditions.begin(); it != m_cancelWaitConditions.end(); ++it) {
        delete it.value();
    }
    m_cancelWaitConditions.clear();
    
    m_taskContexts.clear();
    m_sessionToTasks.clear();
    m_messageToTasks.clear();
    m_orphanedMessages.clear();
    m_orphanedSessions.clear();
}

void TaskCoordinator::cleanupWaitCondition(const QString& taskId) {
    if (m_cancelWaitConditions.contains(taskId)) {
        delete m_cancelWaitConditions.take(taskId);
    }
}

} // namespace EnhanceVision
