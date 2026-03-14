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
    createSession();
}

SessionController::~SessionController()
{
}

QString SessionController::activeSessionId() const
{
    return m_activeSessionId;
}

SessionModel* SessionController::sessionModel() const
{
    return m_sessionModel;
}

int SessionController::sessionCount() const
{
    return m_sessions.size();
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
    session.isActive = m_sessions.isEmpty();
    session.isSelected = false;

    m_sessions.append(session);

    if (session.isActive) {
        m_activeSessionId = session.id;
    }

    m_sessionModel->addSession(session);

    emit sessionCreated(session.id);
    emit sessionCountChanged();

    if (session.isActive) {
        emit activeSessionChanged();
    }

    return session.id;
}

void SessionController::switchSession(const QString& sessionId)
{
    if (m_activeSessionId == sessionId) return;

    for (auto& session : m_sessions) {
        if (session.id == m_activeSessionId) {
            session.isActive = false;
        }
        if (session.id == sessionId) {
            session.isActive = true;
            m_activeSessionId = sessionId;
        }
    }

    m_sessionModel->updateSessions(m_sessions);

    emit sessionSwitched(sessionId);
    emit activeSessionChanged();
}

void SessionController::renameSession(const QString& sessionId, const QString& newName)
{
    if (newName.isEmpty()) return;

    for (auto& session : m_sessions) {
        if (session.id == sessionId) {
            session.name = newName;
            session.modifiedAt = QDateTime::currentDateTime();
            m_sessionModel->updateSession(session);
            emit sessionRenamed(sessionId, newName);
            break;
        }
    }
}

void SessionController::deleteSession(const QString& sessionId)
{
    int index = -1;
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].id == sessionId) {
            index = i;
            break;
        }
    }

    if (index == -1) return;

    bool wasActive = m_sessions[index].isActive;
    m_sessions.removeAt(index);
    m_sessionModel->removeSession(sessionId);

    if (m_sessions.isEmpty()) {
        m_activeSessionId.clear();
        emit activeSessionChanged();
    } else if (wasActive) {
        int newIndex = qMin(index, m_sessions.size() - 1);
        switchSession(m_sessions[newIndex].id);
    }

    emit sessionDeleted(sessionId);
    emit sessionCountChanged();
}

void SessionController::clearSession(const QString& sessionId)
{
    for (auto& session : m_sessions) {
        if (session.id == sessionId) {
            session.messages.clear();
            session.modifiedAt = QDateTime::currentDateTime();
            m_sessionModel->updateSession(session);
            emit sessionCleared(sessionId);
            break;
        }
    }
}

void SessionController::selectSession(const QString& sessionId, bool selected)
{
    for (auto& session : m_sessions) {
        if (session.id == sessionId) {
            session.isSelected = selected;
            m_sessionModel->updateSession(session);
            emit selectionChanged();
            break;
        }
    }
}

void SessionController::selectAllSessions()
{
    for (auto& session : m_sessions) {
        session.isSelected = true;
    }
    m_sessionModel->updateSessions(m_sessions);
    emit selectionChanged();
}

void SessionController::deselectAllSessions()
{
    for (auto& session : m_sessions) {
        session.isSelected = false;
    }
    m_sessionModel->updateSessions(m_sessions);
    emit selectionChanged();
}

void SessionController::deleteSelectedSessions()
{
    QStringList toDelete;
    for (const auto& session : m_sessions) {
        if (session.isSelected) {
            toDelete.append(session.id);
        }
    }

    for (const auto& id : toDelete) {
        deleteSession(id);
    }

    setBatchSelectionMode(false);
    deselectAllSessions();
}

void SessionController::clearSelectedSessions()
{
    for (auto& session : m_sessions) {
        if (session.isSelected) {
            session.messages.clear();
            session.modifiedAt = QDateTime::currentDateTime();
            m_sessionModel->updateSession(session);
            emit sessionCleared(session.id);
        }
    }
    
    setBatchSelectionMode(false);
    deselectAllSessions();
}

void SessionController::pinSession(const QString& sessionId, bool pinned)
{
    for (auto& session : m_sessions) {
        if (session.id == sessionId) {
            session.isPinned = pinned;
            session.modifiedAt = QDateTime::currentDateTime();
            break;
        }
    }
    
    m_sessionModel->pinSession(sessionId, pinned);
    emit sessionPinned(sessionId, pinned);
}

void SessionController::moveSession(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_sessions.size() ||
        toIndex < 0 || toIndex >= m_sessions.size() ||
        fromIndex == toIndex) {
        return;
    }
    
    m_sessions.move(fromIndex, toIndex);
    m_sessionModel->moveSession(fromIndex, toIndex);
    emit sessionMoved(fromIndex, toIndex);
}

bool SessionController::isSessionPinned(const QString& sessionId) const
{
    for (const auto& session : m_sessions) {
        if (session.id == sessionId) {
            return session.isPinned;
        }
    }
    return false;
}

int SessionController::selectedCount() const
{
    int count = 0;
    for (const auto& session : m_sessions) {
        if (session.isSelected) {
            count++;
        }
    }
    return count;
}

QString SessionController::getSessionName(const QString& sessionId) const
{
    for (const auto& session : m_sessions) {
        if (session.id == sessionId) {
            return session.name;
        }
    }
    return QString();
}

Session* SessionController::getSession(const QString& sessionId)
{
    for (auto& session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

QString SessionController::ensureActiveSession()
{
    if (m_sessions.isEmpty() || m_activeSessionId.isEmpty()) {
        QString newId = createSession();
        return newId;
    }
    
    bool found = false;
    for (const auto& session : m_sessions) {
        if (session.id == m_activeSessionId) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        if (!m_sessions.isEmpty()) {
            switchSession(m_sessions.first().id);
        } else {
            QString newId = createSession();
            return newId;
        }
    }
    
    return m_activeSessionId;
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
