/**
 * @file ProgressReporter.cpp
 * @brief 进度报告器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/ProgressReporter.h"
#include <QThread>
#include <algorithm>
#include <cmath>

namespace EnhanceVision {

ProgressReporter::ProgressReporter(QObject* parent)
    : QObject(parent)
{
}

double ProgressReporter::progress() const
{
    return m_progress.load();
}

QString ProgressReporter::stage() const
{
    QMutexLocker locker(&m_mutex);
    return m_stage;
}

int ProgressReporter::estimatedRemainingSec() const
{
    return m_estimatedRemainingSec.load();
}

void ProgressReporter::setProgress(double value, const QString& stage)
{
    const double clampedValue = std::clamp(value, 0.0, 1.0);
    
    double previous = m_rawProgress.load();
    const bool isReset = (clampedValue < 0.01);
    const bool isComplete = (clampedValue >= 0.99);
    const bool isAnomaly = isAnomalyJump(clampedValue);
    
    if (!isReset && !isComplete && isAnomaly) {
        return;
    }
    
    m_rawProgress.store(clampedValue);
    
    if (!stage.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        if (m_stage != stage) {
            m_stage = stage;
            locker.unlock();
            if (QThread::currentThread() == thread()) {
                emit stageChanged(stage);
            } else {
                QMetaObject::invokeMethod(this, [this, stage]() {
                    emit stageChanged(stage);
                }, Qt::QueuedConnection);
            }
        }
    }
    
    const double smoothedValue = smoothProgress(clampedValue);
    
    previous = m_progress.exchange(smoothedValue);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastEmit = m_lastEmitMs.load();
    const bool firstEmit = (lastEmit == 0);
    const bool reachedTerminal = (smoothedValue >= 1.0);
    const bool progressedEnough = (std::abs(smoothedValue - previous) >= 0.01);
    const bool timeoutReached = (nowMs - lastEmit) >= kMinEmitIntervalMs;
    
    if (firstEmit || reachedTerminal || progressedEnough || timeoutReached) {
        m_lastEmitMs.store(nowMs);
        {
            QMutexLocker locker(&m_mutex);
            m_progressHistory.push_back(smoothedValue);
            m_timeHistory.push_back(nowMs);
            while (m_progressHistory.size() > kMaxHistorySize) {
                m_progressHistory.pop_front();
                m_timeHistory.pop_front();
            }
        }
        
        updateEstimatedRemainingTime();
        
        if (QThread::currentThread() == thread()) {
            emit progressChanged(smoothedValue);
        } else {
            QMetaObject::invokeMethod(this, [this, smoothedValue]() {
                emit progressChanged(smoothedValue);
            }, Qt::QueuedConnection);
        }
    }
}

void ProgressReporter::reset()
{
    m_progress.store(0.0);
    m_rawProgress.store(0.0);
    m_estimatedRemainingSec.store(0);
    m_startTimeMs.store(QDateTime::currentMSecsSinceEpoch());
    m_lastEmitMs.store(0);
    
    {
        QMutexLocker locker(&m_mutex);
        m_stage.clear();
        m_progressHistory.clear();
        m_timeHistory.clear();
    }
    
    if (QThread::currentThread() == thread()) {
        emit progressChanged(0.0);
        emit stageChanged(QString());
        emit estimatedRemainingSecChanged(0);
    } else {
        QMetaObject::invokeMethod(this, [this]() {
            emit progressChanged(0.0);
            emit stageChanged(QString());
            emit estimatedRemainingSecChanged(0);
        }, Qt::QueuedConnection);
    }
}

void ProgressReporter::forceUpdate(double value, const QString& stage)
{
    const double clampedValue = std::clamp(value, 0.0, 1.0);
    m_progress.store(clampedValue);
    m_rawProgress.store(clampedValue);
    
    if (!stage.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        m_stage = stage;
    }
    
    if (QThread::currentThread() == thread()) {
        emit progressChanged(clampedValue);
        if (!stage.isEmpty()) {
            emit stageChanged(stage);
        }
    } else {
        QMetaObject::invokeMethod(this, [this, clampedValue, stage]() {
            emit progressChanged(clampedValue);
            if (!stage.isEmpty()) {
                emit stageChanged(stage);
            }
        }, Qt::QueuedConnection);
    }
}

double ProgressReporter::smoothProgress(double rawValue)
{
    double current = m_progress.load();
    double delta = rawValue - current;
    
    if (delta <= 0) {
        return rawValue;
    }
    
    if (delta > kMaxJumpDelta) {
        return current + kMaxJumpDelta;
    }
    
    return rawValue;
}

void ProgressReporter::updateEstimatedRemainingTime()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_progressHistory.size() < 3) {
        return;
    }
    
    double progressStart = m_progressHistory.front();
    double progressEnd = m_progressHistory.back();
    qint64 timeStart = m_timeHistory.front();
    qint64 timeEnd = m_timeHistory.back();
    
    double progressDelta = progressEnd - progressStart;
    qint64 timeDeltaMs = timeEnd - timeStart;
    
    if (progressDelta <= 0.001 || timeDeltaMs <= 0) {
        return;
    }
    
    double rate = timeDeltaMs / progressDelta / 1000.0;
    double remainingProgress = 1.0 - progressEnd;
    int estimatedSec = static_cast<int>(std::round(rate * remainingProgress));
    
    estimatedSec = std::max(0, std::min(estimatedSec, 3600 * 24));
    
    locker.unlock();
    
    int previous = m_estimatedRemainingSec.exchange(estimatedSec);
    if (previous != estimatedSec) {
        if (QThread::currentThread() == thread()) {
            emit estimatedRemainingSecChanged(estimatedSec);
        } else {
            QMetaObject::invokeMethod(this, [this, estimatedSec]() {
                emit estimatedRemainingSecChanged(estimatedSec);
            }, Qt::QueuedConnection);
        }
    }
}

bool ProgressReporter::isAnomalyJump(double newValue) const
{
    double current = m_rawProgress.load();
    
    if (newValue < current) {
        return (current - newValue) > kAnomalyThreshold;
    }
    
    return false;
}

} // namespace EnhanceVision
