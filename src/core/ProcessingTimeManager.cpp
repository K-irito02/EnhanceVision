/**
 * @file ProcessingTimeManager.cpp
 * @brief 处理任务时间管理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ProcessingTimeManager.h"
#include "EnhanceVision/core/TaskTimeEstimator.h"
#include "EnhanceVision/models/MessageModel.h"
#include <QDebug>
#include <QtMath>

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
    
    // 同步到 TaskTimeEstimator 进行动态跟踪
    TaskTimeEstimator::instance()->startTracking(messageId, static_cast<double>(predictedTotalSec));

    if (m_messageModel) {
        m_messageModel->updateProcessingStartTime(messageId, info.processingStartTime);
        m_messageModel->updatePredictedTotalSec(messageId, info.predictedTotalSec);
        m_messageModel->updateTimeInfo(messageId, info.elapsedSec, info.remainingSec, info.isOvertime);
    }
    
    qInfo() << "[ProcessingTimeManager] Registered task:" << messageId
            << "predictedTotalSec:" << predictedTotalSec
            << "startTime:" << info.processingStartTime;
}

void ProcessingTimeManager::unregisterTask(const QString& messageId)
{
    auto it = m_tasks.find(messageId);
    if (it != m_tasks.end()) {
        // 在注销前获取最终的实际耗时（以实时时钟为主，避免完成瞬间因采样滞后导致仅显示 1 秒）
        TaskTimeEstimator* estimator = TaskTimeEstimator::instance();
        QVariantMap timeInfo = estimator->getTaskTimeInfo(messageId);

        double estimatorElapsedSec = 0.0;
        if (timeInfo.value("valid", false).toBool()) {
            estimatorElapsedSec = timeInfo.value("elapsedSec", 0.0).toDouble();
        }

        const TaskTimeInfo info = it.value();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const double realtimeElapsedSec = info.processingStartTime > 0
            ? static_cast<double>(qMax<qint64>(0, nowMs - info.processingStartTime)) / 1000.0
            : 0.0;
        const double localElapsedSec = static_cast<double>(qMax<qint64>(0, info.elapsedSec));

        // 采用三者最大值，避免任何单一路径低估最终总耗时
        const double bestElapsedSec = qMax(estimatorElapsedSec, qMax(realtimeElapsedSec, localElapsedSec));
        qint64 actualTotalSec = bestElapsedSec > 0.0 ? qMax<qint64>(1, qRound64(bestElapsedSec)) : 0;

        // 若任务确实启动过但最终仍为 0，则兜底为 1 秒
        if (actualTotalSec <= 0 && info.processingStartTime > 0) {
            actualTotalSec = 1;
        }

        // 更新 MessageModel 中的实际总耗时
        if (m_messageModel) {
            const qint64 displayActualTotalSec = actualTotalSec > 0 ? actualTotalSec : (info.processingStartTime > 0 ? 1 : 0);
            if (displayActualTotalSec > 0) {
                m_messageModel->updateActualTotalSec(messageId, displayActualTotalSec);
                qInfo() << "[ProcessingTimeManager] Updated actualTotalSec for:" << messageId << "value:" << displayActualTotalSec;
            }
        }
        
        // 同步停止 TaskTimeEstimator 跟踪
        estimator->stopTracking(messageId);
        
        m_tasks.erase(it);
        m_pausedTasks.remove(messageId);
        m_pausedElapsedSec.remove(messageId);
        
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
    
    // 同步到 TaskTimeEstimator
    TaskTimeEstimator::instance()->pauseTracking(messageId);
    
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
    
    // 同步到 TaskTimeEstimator
    TaskTimeEstimator::instance()->resumeTracking(messageId);

    if (m_messageModel) {
        m_messageModel->updateProcessingStartTime(messageId, info.processingStartTime);
        m_messageModel->updateTimeInfo(messageId, pausedElapsed, qMax<qint64>(0, info.predictedTotalSec - pausedElapsed), false);
    }
    
    qInfo() << "[ProcessingTimeManager] Resumed task:" << messageId
            << "adjustedStartTime:" << info.processingStartTime;
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
