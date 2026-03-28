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

bool ProgressReporter::isValid() const
{
    return m_progress.load() >= 0.0;
}

QObject* ProgressReporter::asQObject()
{
    return this;
}

int ProgressReporter::totalSteps() const
{
    return m_totalSteps.load();
}

int ProgressReporter::currentStep() const
{
    return m_currentStep.load();
}

QString ProgressReporter::subStage() const
{
    QMutexLocker locker(&m_mutex);
    return m_subStage;
}

int ProgressReporter::batchTotal() const
{
    return m_batchTotal.load();
}

int ProgressReporter::batchCompleted() const
{
    return m_batchCompleted.load();
}

bool ProgressReporter::isBatchMode() const
{
    return m_batchMode.load();
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
            emitSignals(m_progress.load(), stage);
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
            while (m_progressHistory.size() > static_cast<size_t>(kMaxHistorySize)) {
                m_progressHistory.pop_front();
                m_timeHistory.pop_front();
            }
        }
        
        updateEstimatedRemainingTime();
        
        QString currentStage;
        {
            QMutexLocker locker(&m_mutex);
            currentStage = m_stage;
        }
        emitSignals(smoothedValue, currentStage);
    }
}

void ProgressReporter::setProgress(double value, int step, int totalSteps, const QString& stage)
{
    if (totalSteps > 0) {
        m_totalSteps.store(totalSteps);
        m_currentStep.store(step);
    }
    
    setProgress(value, stage);
}

void ProgressReporter::setSubProgress(double subProgress, const QString& subStage)
{
    m_subProgress.store(std::clamp(subProgress, 0.0, 1.0));
    
    if (!subStage.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        if (m_subStage != subStage) {
            m_subStage = subStage;
            locker.unlock();
            
            if (QThread::currentThread() == thread()) {
                emit subStageChanged(subStage);
            } else {
                QMetaObject::invokeMethod(this, [this, subStage]() {
                    emit subStageChanged(subStage);
                }, Qt::QueuedConnection);
            }
        }
    }
}

void ProgressReporter::beginBatch(int totalItems, const QString& stage)
{
    m_batchTotal.store(totalItems);
    m_batchCompleted.store(0);
    m_batchMode.store(true);
    
    if (!stage.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        m_stage = stage;
    }
    
    setProgress(0.0, stage);
}

void ProgressReporter::updateBatch(int completedItems, const QString& subStage)
{
    if (!m_batchMode.load()) {
        return;
    }
    
    const int total = m_batchTotal.load();
    m_batchCompleted.store(completedItems);
    
    if (total > 0) {
        double progress = static_cast<double>(completedItems) / total;
        setProgress(progress, subStage);
    }
    
    if (QThread::currentThread() == thread()) {
        emit batchProgressChanged(completedItems, total);
    } else {
        QMetaObject::invokeMethod(this, [this, completedItems, total]() {
            emit batchProgressChanged(completedItems, total);
        }, Qt::QueuedConnection);
    }
}

void ProgressReporter::endBatch()
{
    m_batchMode.store(false);
    m_batchCompleted.store(m_batchTotal.load());
    setProgress(1.0);
}

void ProgressReporter::reset()
{
    m_progress.store(0.0);
    m_rawProgress.store(0.0);
    m_estimatedRemainingSec.store(0);
    m_startTimeMs.store(QDateTime::currentMSecsSinceEpoch());
    m_lastEmitMs.store(0);
    m_totalSteps.store(1);
    m_currentStep.store(0);
    m_subProgress.store(0.0);
    m_batchTotal.store(0);
    m_batchCompleted.store(0);
    m_batchMode.store(false);
    
    {
        QMutexLocker locker(&m_mutex);
        m_stage.clear();
        m_subStage.clear();
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
    
    emitSignals(clampedValue, stage);
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
    double delta = newValue - current;
    
    if (delta < 0) {
        return true;
    }
    
    return delta > kAnomalyThreshold;
}

void ProgressReporter::emitSignals(double progress, const QString& stage)
{
    if (QThread::currentThread() == thread()) {
        emit progressChanged(progress);
        if (!stage.isEmpty()) {
            emit stageChanged(stage);
        }
    } else {
        QMetaObject::invokeMethod(this, [this, progress, stage]() {
            emit progressChanged(progress);
            if (!stage.isEmpty()) {
                emit stageChanged(stage);
            }
        }, Qt::QueuedConnection);
    }
}

} // namespace EnhanceVision
