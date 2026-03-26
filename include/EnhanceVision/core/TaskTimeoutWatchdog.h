/**
 * @file TaskTimeoutWatchdog.h
 * @brief 任务超时看门狗 - 检测停滞和超时任务，触发自动恢复
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_TASKTIMEOUTWATCHDOG_H
#define ENHANCEVISION_TASKTIMEOUTWATCHDOG_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QTimer>

namespace EnhanceVision {

class TaskTimeoutWatchdog : public QObject
{
    Q_OBJECT

public:
    explicit TaskTimeoutWatchdog(QObject* parent = nullptr);
    ~TaskTimeoutWatchdog() override;

    void watchTask(const QString& taskId, int timeoutMs);
    void unwatchTask(const QString& taskId);
    void resetTimer(const QString& taskId);

    void setDefaultTimeout(int timeoutMs);
    int defaultTimeout() const;

    void setStallThreshold(int stallMs);
    int stallThreshold() const;

    void setCheckInterval(int intervalMs);
    int checkInterval() const;

    void start();
    void stop();
    bool isRunning() const;

    int watchedTaskCount() const;

signals:
    void taskTimedOut(const QString& taskId);
    void taskStalled(const QString& taskId, qint64 stallDurationMs);

private slots:
    void performCheck();

private:
    struct WatchEntry {
        qint64 startMs = 0;
        qint64 lastProgressMs = 0;
        int timeoutMs = 300000;
        bool stallWarned = false;
    };

    QHash<QString, WatchEntry> m_watches;
    QTimer* m_checkTimer;
    int m_defaultTimeoutMs = 300000;
    int m_stallThresholdMs = 30000;
    int m_checkIntervalMs = 5000;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_TASKTIMEOUTWATCHDOG_H
