#ifndef ENHANCEVISION_PROGRESSMANAGER_H
#define ENHANCEVISION_PROGRESSMANAGER_H

#include "EnhanceVision/core/IProgressProvider.h"
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QMutex>
#include <QSet>
#include <QDateTime>

namespace EnhanceVision {

class ProgressManager : public QObject
{
    Q_OBJECT

public:
    struct ProgressInfo
    {
        double progress = 0.0;
        QString stage;
        int estimatedRemainingSec = -1;
        bool isValid = false;
        qint64 lastUpdateMs = 0;
    };

    explicit ProgressManager(QObject* parent = nullptr);
    ~ProgressManager() override;

    void registerProvider(const QString& id, IProgressProvider* provider);
    void unregisterProvider(const QString& id);
    bool hasProvider(const QString& id) const;

    ProgressInfo getProgress(const QString& id) const;
    ProgressInfo getAggregatedProgress(const QStringList& ids) const;

    void addToGroup(const QString& groupId, const QString& providerId);
    void removeFromGroup(const QString& groupId, const QString& providerId);
    ProgressInfo getGroupProgress(const QString& groupId) const;

    void setUpdateInterval(int ms);
    int updateInterval() const;

    Q_INVOKABLE double getProgressValue(const QString& id) const;
    Q_INVOKABLE QString getProgressStage(const QString& id) const;
    Q_INVOKABLE int getProgressEstimatedSec(const QString& id) const;
    Q_INVOKABLE QVariantMap getProgressInfo(const QString& id) const;
    Q_INVOKABLE QVariantMap getGroupProgressInfo(const QString& groupId) const;

signals:
    void progressUpdated(const QString& id, double progress, const QString& stage, int estimatedSec);
    void groupProgressUpdated(const QString& groupId, double progress, const QString& stage, int estimatedSec);

private slots:
    void onProviderProgressChanged(double progress);
    void onProviderStageChanged(const QString& stage);
    void onProviderEstimatedTimeChanged(int seconds);
    void onProviderDestroyed(QObject* obj);
    void onThrottleTimer();

private:
    void updateProgressInfo(const QString& id);
    QString senderId() const;

private:
    mutable QMutex m_mutex;
    QHash<QString, QObject*> m_providerObjects;
    QHash<QString, IProgressProvider*> m_providers;
    QHash<QString, ProgressInfo> m_progressCache;
    QHash<QString, QStringList> m_groups;

    QTimer* m_throttleTimer;
    QSet<QString> m_pendingUpdates;
    int m_updateIntervalMs = 100;

    QHash<QString, qint64> m_lastEmitTime;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROGRESSMANAGER_H
