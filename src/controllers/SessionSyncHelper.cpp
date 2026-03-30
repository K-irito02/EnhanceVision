#include "EnhanceVision/controllers/SessionSyncHelper.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/models/MessageModel.h"
#include <QDateTime>

namespace EnhanceVision {

SessionSyncHelper::SessionSyncHelper(QObject* parent)
    : QObject(parent)
{
    m_sessionSyncTimer = new QTimer(this);
    m_sessionSyncTimer->setSingleShot(true);
    m_sessionSyncTimer->setInterval(2500);
    connect(m_sessionSyncTimer, &QTimer::timeout, this, [this]() {
        if (m_sessionController) {
            m_sessionController->syncCurrentMessagesToSession();
        }
    });

    m_memorySyncTimer = new QTimer(this);
    m_memorySyncTimer->setSingleShot(true);
    m_memorySyncTimer->setInterval(100);
    connect(m_memorySyncTimer, &QTimer::timeout, this, &SessionSyncHelper::flushSessionMemorySync);
}

SessionSyncHelper::~SessionSyncHelper() = default;

void SessionSyncHelper::setSessionController(SessionController* controller)
{
    m_sessionController = controller;
}

void SessionSyncHelper::setMessageModel(MessageModel* model)
{
    m_messageModel = model;
}

void SessionSyncHelper::requestSessionSync()
{
    if (!m_sessionSyncTimer) {
        return;
    }
    m_sessionSyncTimer->start();
}

void SessionSyncHelper::requestSessionMemorySync(const QString& messageId)
{
    if (!m_sessionController) {
        return;
    }

    if (!messageId.isEmpty()) {
        m_pendingMemorySyncMessageIds.insert(messageId);
    }

    if (m_memorySyncTimer) {
        m_memorySyncTimer->start();
    }
}

void SessionSyncHelper::flushSessionMemorySync()
{
    if (!m_sessionController) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (m_pendingMemorySyncMessageIds.isEmpty()) {
        m_sessionController->syncCurrentMessagesToSession();
        return;
    }

    const QSet<QString> pendingIds = m_pendingMemorySyncMessageIds;
    for (const QString& messageId : pendingIds) {
        const qint64 lastMs = m_lastSessionMemorySyncMs.value(messageId, 0);
        if (lastMs > 0 && (nowMs - lastMs) < 100) {
            continue;
        }
        m_lastSessionMemorySyncMs[messageId] = nowMs;
        m_pendingMemorySyncMessageIds.remove(messageId);
    }

    if (!pendingIds.isEmpty()) {
        m_sessionController->syncCurrentMessagesToSession();
    }
}

void SessionSyncHelper::syncVisibleMessageIfNeeded(const QString& sessionId,
                                                    const QString& messageId,
                                                    const std::function<void()>& syncFn)
{
    if (!m_messageModel || !m_sessionController || sessionId.isEmpty()) {
        return;
    }

    if (m_sessionController->activeSessionId() != sessionId) {
        return;
    }

    syncFn();
    requestSessionMemorySync(messageId);
}

} // namespace EnhanceVision
