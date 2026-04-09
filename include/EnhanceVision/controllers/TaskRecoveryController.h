/**
 * @file TaskRecoveryController.h
 * @brief 启动恢复控制器 - 管理未完成任务的恢复快照、恢复弹窗和模式切换拦截摘要
 */
#ifndef ENHANCEVISION_TASKRECOVERYCONTROLLER_H
#define ENHANCEVISION_TASKRECOVERYCONTROLLER_H

#include <QObject>
#include <QVariantList>
#include "EnhanceVision/models/DataTypes.h"

class QTimer;

namespace EnhanceVision {

class SessionController;
class ProcessingController;

class TaskRecoveryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasPendingRecovery READ hasPendingRecovery NOTIFY hasPendingRecoveryChanged)
    Q_PROPERTY(QVariantList recoverySummary READ recoverySummary NOTIFY recoverySummaryChanged)
    Q_PROPERTY(QString shutdownReason READ shutdownReason NOTIFY shutdownReasonChanged)
    Q_PROPERTY(bool restoreAvailable READ restoreAvailable NOTIFY restoreAvailableChanged)

public:
    explicit TaskRecoveryController(QObject* parent = nullptr);

    void setSessionController(SessionController* sessionController);
    void setProcessingController(ProcessingController* processingController);

    bool hasPendingRecovery() const;
    QVariantList recoverySummary() const;
    QString shutdownReason() const;
    bool restoreAvailable() const;

    void initializeStartupRecovery();
    void persistSnapshotNow();

    Q_INVOKABLE QVariantList getPauseModeSwitchBlockers() const;
    Q_INVOKABLE void resolveRestorePreviousState();
    Q_INVOKABLE void resolveFailAllRecoverableTasks();
    Q_INVOKABLE void syncPendingRecoveryFromSessions();
    Q_INVOKABLE void scheduleSnapshotSync();

signals:
    void hasPendingRecoveryChanged();
    void recoverySummaryChanged();
    void shutdownReasonChanged();
    void restoreAvailableChanged();

private:
    QString snapshotFilePath() const;
    void ensureSnapshotDirectory() const;
    bool loadSnapshotFromDisk(RecoverySnapshot& snapshot) const;
    void saveSnapshotToDisk(const RecoverySnapshot& snapshot);
    void clearSnapshotFile();
    RecoverySnapshot buildFallbackSnapshotFromCurrentSessions() const;
    QVariantList buildSummaryFromSnapshot(const RecoverySnapshot& snapshot) const;
    void applyRecoverableStateToSessions();
    void failRecoverableTasks(const QString& errorMessage);
    void setPendingRecoveryState(bool pending);

    SessionController* m_sessionController = nullptr;
    ProcessingController* m_processingController = nullptr;
    RecoverySnapshot m_snapshot;
    QVariantList m_recoverySummary;
    QString m_shutdownReason;
    bool m_hasPendingRecovery = false;
    bool m_restoreAvailable = true;
    QTimer* m_snapshotSyncTimer = nullptr;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_TASKRECOVERYCONTROLLER_H
