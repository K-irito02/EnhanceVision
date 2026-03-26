/**
 * @file TaskRetryPolicy.h
 * @brief 任务重试策略 - 指数退避、最大重试次数限制、条件重试
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_TASKRETRYPOLICY_H
#define ENHANCEVISION_TASKRETRYPOLICY_H

#include <QObject>
#include <QHash>
#include <QString>

namespace EnhanceVision {

class TaskRetryPolicy : public QObject
{
    Q_OBJECT

public:
    struct RetryConfig {
        int maxRetries = 3;
        int baseDelayMs = 1000;
        int maxDelayMs = 30000;
        double backoffMultiplier = 2.0;
        bool retryOnTimeout = true;
        bool retryOnResourceExhaustion = false;
    };

    struct RetryState {
        int attemptCount = 0;
        int nextDelayMs = 0;
        QString lastError;
    };

    explicit TaskRetryPolicy(QObject* parent = nullptr);
    ~TaskRetryPolicy() override;

    void setConfig(const RetryConfig& config);
    RetryConfig config() const;

    bool canRetry(const QString& taskId) const;
    int nextDelayMs(const QString& taskId) const;
    int attemptCount(const QString& taskId) const;

    void recordAttempt(const QString& taskId, const QString& error);
    void resetTask(const QString& taskId);
    void clearAll();

    RetryState stateForTask(const QString& taskId) const;

signals:
    void retryScheduled(const QString& taskId, int delayMs, int attempt);
    void retriesExhausted(const QString& taskId, int totalAttempts, const QString& lastError);

private:
    int computeDelay(int attempt) const;

    RetryConfig m_config;
    QHash<QString, RetryState> m_states;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_TASKRETRYPOLICY_H
