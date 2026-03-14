/**
 * @file SessionController.cpp
 * @brief 会话控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SessionController.h"
#include <QUuid>
#include <QDebug>

namespace EnhanceVision {

SessionController::SessionController(QObject* parent)
    : QObject(parent)
    , m_sessionModel(new SessionModel(this))
    , m_batchSelectionMode(false)
    , m_sessionCounter(0)
{
    connect(m_sessionModel, &SessionModel::errorOccurred, this, &SessionController::errorOccurred);
    createSession();
}

SessionController::~SessionController()
{
}

QString SessionController::activeSessionId() const
{
    return m_sessionModel->activeSessionId();
}

SessionModel* SessionController::sessionModel() const
{
    return m_sessionModel;
}

int SessionController::sessionCount() const
{
    return m_sessionModel->rowCount();
}

bool SessionController::batchSelectionMode() const
{
    return m_batchSelectionMode;
}

void SessionController::setBatchSelectionMode(bool mode)
{
    if (m_batchSelectionMode != mode) {
        m_batchSelectionMode = mode;
        emit batchSelectionModeChanged();
    }
}

QString SessionController::createSession(const QString& name)
{
    Session session;
    session.id = generateSessionId();
    session.name = name.isEmpty() ? generateDefaultSessionName() : name;
    session.createdAt = QDateTime::currentDateTime();
    session.modifiedAt = QDateTime::currentDateTime();
    session.isActive = m_sessionModel->rowCount() == 0;
    session.isSelected = false;

    m_sessionModel->addSession(session);

    if (session.isActive) {
        m_activeSessionId = session.id;
    }

    emit sessionCreated(session.id);
    emit sessionCountChanged();

    if (session.isActive) {
        emit activeSessionChanged();
    }

    return session.id;
}

void SessionController::switchSession(const QString& sessionId)
{
    QString currentActiveId = m_sessionModel->activeSessionId();
    if (currentActiveId == sessionId) return;

    m_sessionModel->switchSession(sessionId);
    m_activeSessionId = sessionId;

    emit sessionSwitched(sessionId);
    emit activeSessionChanged();
}

void SessionController::renameSession(const QString& sessionId, const QString& newName)
{
    if (newName.isEmpty()) return;

    m_sessionModel->renameSession(sessionId, newName);
    emit sessionRenamed(sessionId, newName);
}

void SessionController::deleteSession(const QString& sessionId)
{
    m_sessionModel->deleteSession(sessionId);
    emit sessionDeleted(sessionId);
    emit sessionCountChanged();
}

void SessionController::clearSession(const QString& sessionId)
{
    m_sessionModel->clearSession(sessionId);
}

void SessionController::selectSession(const QString& sessionId, bool selected)
{
    m_sessionModel->selectSession(sessionId, selected);
    emit selectionChanged();
}

void SessionController::selectAllSessions()
{
    m_sessionModel->selectAll();
    emit selectionChanged();
}

void SessionController::deselectAllSessions()
{
    m_sessionModel->clearSelection();
    emit selectionChanged();
}

void SessionController::deleteSelectedSessions()
{
    int count = m_sessionModel->deleteSelectedSessions();
    if (count > 0) {
        emit sessionCountChanged();
        emit selectionChanged();
    }
    setBatchSelectionMode(false);
}

void SessionController::clearSelectedSessions()
{
    m_sessionModel->clearSelectedSessions();
    m_sessionModel->clearSelection();
    emit selectionChanged();
    setBatchSelectionMode(false);
}

void SessionController::pinSession(const QString& sessionId, bool pinned)
{
    m_sessionModel->pinSession(sessionId, pinned);
    emit sessionPinned(sessionId, pinned);
}

void SessionController::moveSession(int fromIndex, int toIndex)
{
    m_sessionModel->moveSession(fromIndex, toIndex);
    emit sessionMoved(fromIndex, toIndex);
}

bool SessionController::isSessionPinned(const QString& sessionId) const
{
    Session session = m_sessionModel->sessionById(sessionId);
    return session.isPinned;
}

int SessionController::selectedCount() const
{
    return m_sessionModel->selectedCount();
}

QString SessionController::getSessionName(const QString& sessionId) const
{
    Session session = m_sessionModel->sessionById(sessionId);
    return session.name;
}

Session* SessionController::getSession(const QString& sessionId)
{
    QList<Session>& sessions = m_sessionModel->sessionsRef();
    for (auto& session : sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

QString SessionController::ensureActiveSession()
{
    if (m_sessionModel->rowCount() == 0 || m_sessionModel->activeSessionId().isEmpty()) {
        QString newId = createSession();
        return newId;
    }
    
    return m_sessionModel->activeSessionId();
}

QString SessionController::generateSessionId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString SessionController::generateDefaultSessionName()
{
    m_sessionCounter++;
    return tr("未命名会话 %1").arg(m_sessionCounter);
}

} // namespace EnhanceVision
