/**
 * @file TaskRetryPolicy.cpp
 * @brief 任务重试策略实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/TaskRetryPolicy.h"
#include <QDebug>
#include <QtMath>

namespace EnhanceVision {

TaskRetryPolicy::TaskRetryPolicy(QObject* parent)
    : QObject(parent)
{
}

TaskRetryPolicy::~TaskRetryPolicy() = default;

void TaskRetryPolicy::setConfig(const RetryConfig& config)
{
    m_config = config;
}

TaskRetryPolicy::RetryConfig TaskRetryPolicy::config() const
{
    return m_config;
}

bool TaskRetryPolicy::canRetry(const QString& taskId) const
{
    if (!m_states.contains(taskId)) {
        return true;
    }
    return m_states[taskId].attemptCount < m_config.maxRetries;
}

int TaskRetryPolicy::nextDelayMs(const QString& taskId) const
{
    if (!m_states.contains(taskId)) {
        return m_config.baseDelayMs;
    }
    return m_states[taskId].nextDelayMs;
}

int TaskRetryPolicy::attemptCount(const QString& taskId) const
{
    if (!m_states.contains(taskId)) {
        return 0;
    }
    return m_states[taskId].attemptCount;
}

void TaskRetryPolicy::recordAttempt(const QString& taskId, const QString& error)
{
    RetryState& state = m_states[taskId];
    state.attemptCount++;
    state.lastError = error;
    state.nextDelayMs = computeDelay(state.attemptCount);

    if (state.attemptCount >= m_config.maxRetries) {
        qWarning() << "[RetryPolicy] retries exhausted"
                    << "task:" << taskId
                    << "attempts:" << state.attemptCount
                    << "lastError:" << error;
        emit retriesExhausted(taskId, state.attemptCount, error);
    } else {
        qInfo() << "[RetryPolicy] retry scheduled"
                << "task:" << taskId
                << "attempt:" << state.attemptCount
                << "nextDelay:" << state.nextDelayMs << "ms"
                << "error:" << error;
        emit retryScheduled(taskId, state.nextDelayMs, state.attemptCount);
    }
}

void TaskRetryPolicy::resetTask(const QString& taskId)
{
    m_states.remove(taskId);
}

void TaskRetryPolicy::clearAll()
{
    m_states.clear();
}

TaskRetryPolicy::RetryState TaskRetryPolicy::stateForTask(const QString& taskId) const
{
    return m_states.value(taskId);
}

int TaskRetryPolicy::computeDelay(int attempt) const
{
    const double delay = m_config.baseDelayMs * qPow(m_config.backoffMultiplier, attempt - 1);
    return qMin(static_cast<int>(delay), m_config.maxDelayMs);
}

} // namespace EnhanceVision
