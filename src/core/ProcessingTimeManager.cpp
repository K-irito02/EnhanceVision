/**
 * @file ProcessingTimeManager.cpp
 * @brief 处理任务时间管理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ProcessingTimeManager.h"
#include "EnhanceVision/core/TaskTimeEstimator.h"
#include "EnhanceVision/models/MessageModel.h"
#include "EnhanceVision/controllers/SessionController.h"
#include <QtMath>

namespace EnhanceVision {

ProcessingTimeManager::ProcessingTimeManager(QObject *parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
{
    connect(m_updateTimer, &QTimer::timeout, 
            this, &ProcessingTimeManager::onTimerTick);
    m_updateTimer->start(1000);
    
}

ProcessingTimeManager::~ProcessingTimeManager()
{
    m_updateTimer->stop();
}

void ProcessingTimeManager::setMessageModel(MessageModel* model)
{
    m_messageModel = model;
}

void ProcessingTimeManager::setSessionController(SessionController* controller)
{
    m_sessionController = controller;
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
    info.totalPausedMs = 0;
    info.lastPauseStartMs = 0;
    info.isOvertime = false;

    m_tasks.insert(messageId, info);
    
    // 同步到 TaskTimeEstimator，传入相同的开始时间以确保一致
    TaskTimeEstimator::instance()->startTracking(messageId, static_cast<double>(predictedTotalSec), info.processingStartTime);

    if (m_messageModel) {
        m_messageModel->updateProcessingStartTime(messageId, info.processingStartTime);
        m_messageModel->updatePredictedTotalSec(messageId, info.predictedTotalSec);
        m_messageModel->updateTimeInfo(messageId, info.elapsedSec, info.remainingSec, info.isOvertime);
    }
    
}

void ProcessingTimeManager::unregisterTask(const QString& messageId)
{
    auto it = m_tasks.find(messageId);
    if (it != m_tasks.end()) {
        TaskTimeInfo info = it.value();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        
        // 计算累积暂停时间（如果当前正在暂停，需要加上当前暂停的时间）
        qint64 totalPaused = info.totalPausedMs;
        if (m_pausedTasks.contains(messageId) && info.lastPauseStartMs > 0) {
            totalPaused += (nowMs - info.lastPauseStartMs);
        }
        
        // 基于 processingStartTime 直接计算真实总耗时（唯一真源）
        qint64 actualTotalSec = 0;
        if (info.processingStartTime > 0) {
            const qint64 totalElapsedMs = nowMs - info.processingStartTime;
            const qint64 effectiveElapsedMs = qMax<qint64>(0, totalElapsedMs - totalPaused);
            actualTotalSec = qMax<qint64>(1, qRound64(effectiveElapsedMs / 1000.0));
        }

        // 优先使用 SessionController 跨会话更新（支持会话切换场景）
        bool updated = false;
        if (m_sessionController && actualTotalSec > 0) {
            updated = m_sessionController->updateMessageActualTotalSec(messageId, actualTotalSec);
        }
        
        // 如果 SessionController 未能更新（可能消息在当前会话），回退到 MessageModel
        if (!updated && m_messageModel && actualTotalSec > 0) {
            m_messageModel->updateActualTotalSec(messageId, actualTotalSec);
        }
        
        // 同步停止 TaskTimeEstimator 跟踪
        TaskTimeEstimator::instance()->stopTracking(messageId);
        
        m_tasks.erase(it);
        m_pausedTasks.remove(messageId);
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
}

void ProcessingTimeManager::pauseTask(const QString& messageId)
{
    if (!m_tasks.contains(messageId)) {
        return;
    }
    
    if (m_pausedTasks.contains(messageId)) {
        return;  // 已经暂停
    }
    
    TaskTimeInfo& info = m_tasks[messageId];
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    // 记录暂停开始时间
    info.lastPauseStartMs = now;
    
    // 更新当前已用时间（用于显示）
    qint64 totalElapsedMs = now - info.processingStartTime;
    qint64 effectiveElapsedMs = qMax<qint64>(0, totalElapsedMs - info.totalPausedMs);
    info.elapsedSec = effectiveElapsedMs / 1000;
    
    m_pausedTasks.insert(messageId);
    
    // 同步到 TaskTimeEstimator
    TaskTimeEstimator::instance()->pauseTracking(messageId);
}

void ProcessingTimeManager::resumeTask(const QString& messageId)
{
    if (!m_tasks.contains(messageId)) {
        return;
    }
    
    if (!m_pausedTasks.contains(messageId)) {
        return;  // 没有暂停
    }
    
    TaskTimeInfo& info = m_tasks[messageId];
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    // 累积本次暂停时间
    if (info.lastPauseStartMs > 0) {
        info.totalPausedMs += (now - info.lastPauseStartMs);
        info.lastPauseStartMs = 0;
    }
    
    m_pausedTasks.remove(messageId);
    
    // 同步到 TaskTimeEstimator
    TaskTimeEstimator::instance()->resumeTracking(messageId);
    
    // 同步累积暂停时间到 MessageModel
    if (m_messageModel) {
        m_messageModel->updateTotalPausedMs(messageId, info.totalPausedMs);
        m_messageModel->updateTimeInfo(messageId, info.elapsedSec, qMax<qint64>(0, info.predictedTotalSec - info.elapsedSec), false);
    }
}

bool ProcessingTimeManager::isTaskPaused(const QString& messageId) const
{
    return m_pausedTasks.contains(messageId);
}

void ProcessingTimeManager::updateTaskProgress(const QString& messageId, double progress)
{
    if (!m_tasks.contains(messageId)) {
        return;
    }
    
    // 使用 TaskTimeEstimator 进行动态修正
    TaskTimeEstimator* estimator = TaskTimeEstimator::instance();
    double remainingSec = estimator->updateProgress(messageId, progress);
    
    if (remainingSec >= 0) {
        // 从 TaskTimeEstimator 获取修正后的时间信息
        QVariantMap timeInfo = estimator->getTaskTimeInfo(messageId);
        if (timeInfo.value("valid", false).toBool()) {
            TaskTimeInfo& info = m_tasks[messageId];
            info.predictedTotalSec = static_cast<qint64>(timeInfo.value("predictedSec", 0.0).toDouble());
            info.elapsedSec = static_cast<qint64>(timeInfo.value("elapsedSec", 0.0).toDouble());
            info.remainingSec = static_cast<qint64>(remainingSec);
            info.isOvertime = timeInfo.value("isOvertime", false).toBool();
            
            // 发出更新信号
            emit timeInfoUpdated(messageId, info.elapsedSec, info.remainingSec, info.isOvertime);
            
            // 更新 MessageModel
            if (m_messageModel) {
                m_messageModel->updateTimeInfo(messageId, info.elapsedSec, info.remainingSec, info.isOvertime);
                // 同时更新修正后的预测总时间
                m_messageModel->updatePredictedTotalSec(messageId, info.predictedTotalSec);
            }
        }
    }
}

void ProcessingTimeManager::onTimerTick()
{
    if (m_tasks.isEmpty()) {
        return;
    }

    TaskTimeEstimator* estimator = TaskTimeEstimator::instance();

    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        TaskTimeInfo& info = it.value();
        
        // 跳过暂停的任务，不更新其时间
        if (m_pausedTasks.contains(info.messageId)) {
            continue;
        }
        
        // 从 TaskTimeEstimator 获取最新的时间信息（含动态修正）
        QVariantMap timeInfo = estimator->getTaskTimeInfo(info.messageId);
        if (timeInfo.value("valid", false).toBool()) {
            info.predictedTotalSec = static_cast<qint64>(timeInfo.value("predictedSec", 0.0).toDouble());
            info.elapsedSec = static_cast<qint64>(timeInfo.value("elapsedSec", 0.0).toDouble());
            info.remainingSec = static_cast<qint64>(timeInfo.value("remainingSec", 0.0).toDouble());
            info.isOvertime = timeInfo.value("isOvertime", false).toBool();
        } else {
            // 回退到原有逻辑（TaskTimeEstimator 未跟踪时）
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            info.elapsedSec = (now - info.processingStartTime) / 1000;
            info.remainingSec = info.predictedTotalSec - info.elapsedSec;
            info.isOvertime = info.remainingSec < 0;
        }

        emit timeInfoUpdated(info.messageId, 
                             info.elapsedSec, 
                             info.remainingSec, 
                             info.isOvertime);

        if (m_messageModel) {
            m_messageModel->updateTimeInfo(info.messageId, 
                                           info.elapsedSec, 
                                           info.remainingSec, 
                                           info.isOvertime);
            m_messageModel->updatePredictedTotalSec(info.messageId, info.predictedTotalSec);
        }
    }
}

} // namespace EnhanceVision
