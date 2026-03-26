/**
 * @file DeadlockDetector.h
 * @brief 死锁检测器 - 检测任务调度中的循环等待和资源死锁
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_DEADLOCKDETECTOR_H
#define ENHANCEVISION_DEADLOCKDETECTOR_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QDateTime>

namespace EnhanceVision {

class DeadlockDetector : public QObject
{
    Q_OBJECT

public:
    enum class RecoveryStrategy {
        CancelYoungest,
        CancelLowestPriority,
        CancelAll
    };

    struct WaitEdge {
        QString waiterId;
        QString holderId;
        QString resourceId;
        QDateTime createdAt;
    };

    explicit DeadlockDetector(QObject* parent = nullptr);
    ~DeadlockDetector() override;

    void addWaitEdge(const QString& waiterId, const QString& holderId, const QString& resourceId);
    void removeWaitEdge(const QString& waiterId, const QString& resourceId);
    void removeAllEdgesFor(const QString& taskId);

    bool detectCycle() const;
    QStringList getCycleParticipants() const;

    void setCheckInterval(int intervalMs);
    int checkInterval() const;

    void setRecoveryStrategy(RecoveryStrategy strategy);
    RecoveryStrategy recoveryStrategy() const;

    void start();
    void stop();
    bool isRunning() const;

    int edgeCount() const;

signals:
    void deadlockDetected(const QStringList& participants);
    void recoveryRequested(const QString& taskIdToCancel);

private slots:
    void performCheck();

private:
    bool dfs(const QString& node, QSet<QString>& visited, QSet<QString>& inStack,
             QStringList& cyclePath) const;
    QString selectVictim(const QStringList& participants) const;

    QHash<QString, QList<WaitEdge>> m_waitGraph;
    QTimer* m_checkTimer;
    int m_checkIntervalMs = 10000;
    RecoveryStrategy m_strategy = RecoveryStrategy::CancelYoungest;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_DEADLOCKDETECTOR_H
