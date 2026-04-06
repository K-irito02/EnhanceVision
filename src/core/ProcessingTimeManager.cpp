/**
 * @file ProcessingTimeManager.cpp
 * @brief 处理任务时间管理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ProcessingTimeManager.h"
#include "EnhanceVision/models/MessageModel.h"
#include <QDebug>

namespace EnhanceVision {

ProcessingTimeManager::ProcessingTimeManager(QObject *parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
{
    connect(m_updateTimer, &QTimer::timeout, 
            this, &ProcessingTimeManager::onTimerTick);
    m_updateTimer->start(1000);
    
    qInfo() << "[ProcessingTimeManager] Initialized, timer started";
}

ProcessingTimeManager::~ProcessingTimeManager()
{
    m_updateTimer->stop();
    qInfo() << "[ProcessingTimeManager] Destroyed, timer stopped";
}

void ProcessingTimeManager::setMessageModel(MessageModel* model)
{
    m_messageModel = model;
}

void ProcessingTimeManager::registerTask(const QString& messageId, 
                                          qint64 predictedTotalSec,
                                          qint64 processingStartTime)
{
    TaskTimeInfo info;
    info.messageId = messageId;
    info.predictedTotalSec = predictedTotalSec;
    info.processingStartTime = (processingStartTime > 0) 
        ? processingStartTime 
        : QDateTime::currentMSecsSinceEpoch();
    info.elapsedSec = 0;
    info.remainingSec = predictedTotalSec;
    info.isOvertime = false;

    m_tasks.insert(messageId, info);
    
    qInfo() << "[ProcessingTimeManager] Registered task:" << messageId
            << "predictedTotalSec:" << predictedTotalSec
            << "startTime:" << info.processingStartTime;
}

void ProcessingTimeManager::unregisterTask(const QString& messageId)
{
    if (m_tasks.remove(messageId) > 0) {
        qInfo() << "[ProcessingTimeManager] Unregistered task:" << messageId;
    }
}

TaskTimeInfo ProcessingTimeManager::getTaskTimeInfo(const QString& messageId) const
{
    return m_tasks.value(messageId);
}

bool ProcessingTimeManager::isTaskRegistered(const QString& messageId) const
{
    return m_tasks.contains(messageId);
}

QList<QString> ProcessingTimeManager::registeredTaskIds() const
{
    return m_tasks.keys();
}

void ProcessingTimeManager::clear()
{
    m_tasks.clear();
    m_pausedTasks.clear();
    m_pausedElapsedSec.clear();
    qInfo() << "[ProcessingTimeManager] Cleared all tasks";
}

void ProcessingTimeManager::pauseTask(const QString& messageId)
{
    if (!m_tasks.contains(messageId)) {
        return;
    }
    
    if (m_pausedTasks.contains(messageId)) {
        return;  // 已经暂停
    }
    
    // 记录暂停时的已用时间
    TaskTimeInfo& info = m_tasks[messageId];
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    info.elapsedSec = (now - info.processingStartTime) / 1000;
    m_pausedElapsedSec[messageId] = info.elapsedSec;
    m_pausedTasks.insert(messageId);
    
    qInfo() << "[ProcessingTimeManager] Paused task:" << messageId
            << "elapsedSec:" << info.elapsedSec;
}

void ProcessingTimeManager::resumeTask(const QString& messageId)
{
    if (!m_tasks.contains(messageId)) {
        return;
    }
    
    if (!m_pausedTasks.contains(messageId)) {
        return;  // 没有暂停
    }
    
    // 恢复时调整开始时间，使已用时间保持连续
    TaskTimeInfo& info = m_tasks[messageId];
    qint64 pausedElapsed = m_pausedElapsedSec.value(messageId, 0);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    // 新的开始时间 = 当前时间 - 暂停时的已用时间
    info.processingStartTime = now - (pausedElapsed * 1000);
    
    m_pausedTasks.remove(messageId);
    m_pausedElapsedSec.remove(messageId);
    
    qInfo() << "[ProcessingTimeManager] Resumed task:" << messageId
            << "adjustedStartTime:" << info.processingStartTime;
}

bool ProcessingTimeManager::isTaskPaused(const QString& messageId) const
{
    return m_pausedTasks.contains(messageId);
}

void ProcessingTimeManager::onTimerTick()
{
    if (m_tasks.isEmpty()) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        TaskTimeInfo& info = it.value();
        
        // 【修复】跳过暂停的任务，不更新其时间
        if (m_pausedTasks.contains(info.messageId)) {
            continue;
        }
        
        info.elapsedSec = (now - info.processingStartTime) / 1000;
        
        info.remainingSec = info.predictedTotalSec - info.elapsedSec;
        
        info.isOvertime = info.remainingSec < 0;

        emit timeInfoUpdated(info.messageId, 
                             info.elapsedSec, 
                             info.remainingSec, 
                             info.isOvertime);

        if (m_messageModel) {
            m_messageModel->updateTimeInfo(info.messageId, 
                                           info.elapsedSec, 
                                           info.remainingSec, 
                                           info.isOvertime);
        }
    }
}

} // namespace EnhanceVision
