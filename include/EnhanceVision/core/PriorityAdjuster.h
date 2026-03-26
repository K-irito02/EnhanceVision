/**
 * @file PriorityAdjuster.h
 * @brief 动态优先级调整器 - 饥饿防护、会话切换提权、老化提升
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PRIORITYADJUSTER_H
#define ENHANCEVISION_PRIORITYADJUSTER_H

#include <QObject>
#include <QTimer>
#include "EnhanceVision/core/PriorityTaskQueue.h"

namespace EnhanceVision {

class PriorityAdjuster : public QObject
{
    Q_OBJECT

public:
    struct Config {
        int starvationThresholdMs = 30000;
        int checkIntervalMs = 5000;
        TaskPriority starvationPromoteTo = TaskPriority::UserInitiated;
    };

    explicit PriorityAdjuster(PriorityTaskQueue* queue, QObject* parent = nullptr);
    ~PriorityAdjuster() override;

    void setConfig(const Config& config);
    Config config() const;

    void start();
    void stop();
    bool isRunning() const;

    void onSessionChanged(const QString& activeSessionId);

signals:
    void taskPromoted(const QString& taskId, TaskPriority oldPriority, TaskPriority newPriority);
    void starvationDetected(const QString& taskId, qint64 waitMs);

private slots:
    void performCheck();

private:
    PriorityTaskQueue* m_queue;
    Config m_config;
    QTimer* m_checkTimer;
    QString m_activeSessionId;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PRIORITYADJUSTER_H
