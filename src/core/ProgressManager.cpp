/**
 * @file ProgressManager.cpp
 * @brief 统一进度管理器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/ProgressManager.h"
#include "EnhanceVision/core/ProgressReporter.h"
#include <QMutexLocker>
#include <QDateTime>
#include <algorithm>

namespace EnhanceVision {

ProgressManager::ProgressManager(QObject* parent)
    : QObject(parent)
    , m_throttleTimer(new QTimer(this))
{
    m_throttleTimer->setInterval(m_updateIntervalMs);
    m_throttleTimer->setSingleShot(false);
    connect(m_throttleTimer, &QTimer::timeout, this, &ProgressManager::onThrottleTimer);
    m_throttleTimer->start();
}

ProgressManager::~ProgressManager()
{
    QMutexLocker locker(&m_mutex);
    m_providers.clear();
    m_progressCache.clear();
    m_groups.clear();
}

void ProgressManager::registerProvider(const QString& id, IProgressProvider* provider)
{
    if (!provider) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_providers.contains(id)) {
        return;
    }
    
    m_providers[id] = provider;
    m_providerObjects[id] = provider->asQObject();
    
    ProgressInfo info;
    info.progress = provider->progress();
    info.stage = provider->stage();
    info.estimatedRemainingSec = provider->estimatedRemainingSec();
    info.isValid = provider->isValid();
    info.lastUpdateMs = QDateTime::currentMSecsSinceEpoch();
    m_progressCache[id] = info;
    
    QObject* obj = provider->asQObject();
    if (obj) {
        connect(obj, &QObject::destroyed, this, &ProgressManager::onProviderDestroyed);
        
        ProgressReporter* reporter = qobject_cast<ProgressReporter*>(obj);
        if (reporter) {
            connect(reporter, &ProgressReporter::progressChanged,
                    this, &ProgressManager::onProviderProgressChanged);
            connect(reporter, &ProgressReporter::stageChanged,
                    this, &ProgressManager::onProviderStageChanged);
            connect(reporter, &ProgressReporter::estimatedRemainingSecChanged,
                    this, &ProgressManager::onProviderEstimatedTimeChanged);
        }
    }
}

void ProgressManager::unregisterProvider(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_providers.find(id);
    if (it != m_providers.end()) {
        IProgressProvider* provider = it.value();
        if (provider) {
            QObject* obj = provider->asQObject();
            if (obj) {
                disconnect(obj, nullptr, this, nullptr);
            }
        }
        m_providers.erase(it);
    }
    
    m_providerObjects.remove(id);
    m_progressCache.remove(id);
    
    for (auto groupIt = m_groups.begin(); groupIt != m_groups.end(); ++groupIt) {
        groupIt.value().removeAll(id);
    }
    
    m_pendingUpdates.remove(id);
}

bool ProgressManager::hasProvider(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    return m_providers.contains(id);
}

ProgressManager::ProgressInfo ProgressManager::getProgress(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    return m_progressCache.value(id);
}

ProgressManager::ProgressInfo ProgressManager::getAggregatedProgress(const QStringList& ids) const
{
    QMutexLocker locker(&m_mutex);
    
    ProgressInfo result;
    result.isValid = false;
    
    if (ids.isEmpty()) {
        return result;
    }
    
    double totalProgress = 0.0;
    int validCount = 0;
    int totalEstimatedSec = 0;
    QString currentStage;
    qint64 latestUpdate = 0;
    
    for (const QString& id : ids) {
        auto it = m_progressCache.find(id);
        if (it != m_progressCache.end() && it.value().isValid) {
            totalProgress += it.value().progress;
            if (it.value().estimatedRemainingSec > totalEstimatedSec) {
                totalEstimatedSec = it.value().estimatedRemainingSec;
            }
            if (it.value().lastUpdateMs > latestUpdate) {
                latestUpdate = it.value().lastUpdateMs;
                currentStage = it.value().stage;
            }
            validCount++;
        }
    }
    
    if (validCount > 0) {
        result.progress = totalProgress / validCount;
        result.stage = currentStage;
        result.estimatedRemainingSec = totalEstimatedSec;
        result.isValid = true;
        result.lastUpdateMs = latestUpdate;
    }
    
    return result;
}

void ProgressManager::addToGroup(const QString& groupId, const QString& providerId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_groups.contains(groupId)) {
        m_groups[groupId] = QStringList();
    }
    
    if (!m_groups[groupId].contains(providerId)) {
        m_groups[groupId].append(providerId);
    }
}

void ProgressManager::removeFromGroup(const QString& groupId, const QString& providerId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_groups.find(groupId);
    if (it != m_groups.end()) {
        it.value().removeAll(providerId);
    }
}

ProgressManager::ProgressInfo ProgressManager::getGroupProgress(const QString& groupId) const
{
    QMutexLocker locker(&m_mutex);
    
    QStringList ids = m_groups.value(groupId);
    locker.unlock();
    
    return getAggregatedProgress(ids);
}

void ProgressManager::setUpdateInterval(int ms)
{
    if (ms < 16) ms = 16;
    if (ms > 1000) ms = 1000;
    
    m_updateIntervalMs = ms;
    m_throttleTimer->setInterval(ms);
}

int ProgressManager::updateInterval() const
{
    return m_updateIntervalMs;
}

double ProgressManager::getProgressValue(const QString& id) const
{
    return getProgress(id).progress;
}

QString ProgressManager::getProgressStage(const QString& id) const
{
    return getProgress(id).stage;
}

int ProgressManager::getProgressEstimatedSec(const QString& id) const
{
    return getProgress(id).estimatedRemainingSec;
}

QVariantMap ProgressManager::getProgressInfo(const QString& id) const
{
    ProgressInfo info = getProgress(id);
    QVariantMap result;
    result[QStringLiteral("progress")] = info.progress;
    result[QStringLiteral("stage")] = info.stage;
    result[QStringLiteral("estimatedRemainingSec")] = info.estimatedRemainingSec;
    result[QStringLiteral("isValid")] = info.isValid;
    return result;
}

QVariantMap ProgressManager::getGroupProgressInfo(const QString& groupId) const
{
    ProgressInfo info = getGroupProgress(groupId);
    QVariantMap result;
    result[QStringLiteral("progress")] = info.progress;
    result[QStringLiteral("stage")] = info.stage;
    result[QStringLiteral("estimatedRemainingSec")] = info.estimatedRemainingSec;
    result[QStringLiteral("isValid")] = info.isValid;
    return result;
}

void ProgressManager::onProviderProgressChanged(double progress)
{
    Q_UNUSED(progress);
    QString id = senderId();
    if (id.isEmpty()) return;
    
    updateProgressInfo(id);
}

void ProgressManager::onProviderStageChanged(const QString& stage)
{
    Q_UNUSED(stage);
    QString id = senderId();
    if (id.isEmpty()) return;
    
    updateProgressInfo(id);
}

void ProgressManager::onProviderEstimatedTimeChanged(int seconds)
{
    Q_UNUSED(seconds);
    QString id = senderId();
    if (id.isEmpty()) return;
    
    updateProgressInfo(id);
}

void ProgressManager::onProviderDestroyed(QObject* obj)
{
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
        QObject* providerObj = m_providerObjects.value(it.key());
        if (providerObj == obj || it.value() == nullptr) {
            QString id = it.key();
            m_progressCache.remove(id);
            m_pendingUpdates.remove(id);
            m_providerObjects.remove(id);
            
            for (auto groupIt = m_groups.begin(); groupIt != m_groups.end(); ++groupIt) {
                groupIt.value().removeAll(id);
            }
            
            it = m_providers.erase(it);
            break;
        }
    }
}

void ProgressManager::onThrottleTimer()
{
    QSet<QString> updates;
    {
        QMutexLocker locker(&m_mutex);
        updates = m_pendingUpdates;
        m_pendingUpdates.clear();
    }
    
    for (const QString& id : updates) {
        ProgressInfo info = getProgress(id);
        if (info.isValid) {
            emit progressUpdated(id, info.progress, info.stage, info.estimatedRemainingSec);
            
            QMutexLocker locker(&m_mutex);
            for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
                if (it.value().contains(id)) {
                    QString groupId = it.key();
                    locker.unlock();
                    ProgressInfo groupInfo = getGroupProgress(groupId);
                    emit groupProgressUpdated(groupId, groupInfo.progress, groupInfo.stage, groupInfo.estimatedRemainingSec);
                    locker.relock();
                }
            }
        }
    }
}

void ProgressManager::updateProgressInfo(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_providers.find(id);
    if (it == m_providers.end() || !it.value()) {
        return;
    }
    
    IProgressProvider* provider = it.value();
    ProgressInfo info;
    info.progress = provider->progress();
    info.stage = provider->stage();
    info.estimatedRemainingSec = provider->estimatedRemainingSec();
    info.isValid = provider->isValid();
    info.lastUpdateMs = QDateTime::currentMSecsSinceEpoch();
    
    m_progressCache[id] = info;
    m_pendingUpdates.insert(id);
}

QString ProgressManager::senderId() const
{
    QObject* senderObj = sender();
    if (!senderObj) return QString();
    
    QMutexLocker locker(&m_mutex);
    for (auto it = m_providerObjects.begin(); it != m_providerObjects.end(); ++it) {
        if (it.value() == senderObj) {
            return it.key();
        }
    }
    return QString();
}

} // namespace EnhanceVision
