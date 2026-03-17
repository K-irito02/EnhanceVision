/**
 * @file SessionController.cpp
 * @brief 会话控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/models/MessageModel.h"
#include <QUuid>
#include <QDebug>

namespace EnhanceVision {

SessionController::SessionController(QObject* parent)
    : QObject(parent)
    , m_sessionModel(new SessionModel(this))
    , m_messageModel(nullptr)
    , m_batchSelectionMode(false)
    , m_sessionCounter(0)
    , m_autoSaveEnabled(true)
{
    connect(m_sessionModel, &SessionModel::errorOccurred, this, &SessionController::errorOccurred);
    
    // 连接 SessionModel 的 activeSessionChanged 信号，处理内部会话切换（如删除会话后自动切换）
    connect(m_sessionModel, &SessionModel::activeSessionChanged, this, [this]() {
        QString newActiveId = m_sessionModel->activeSessionId();
        if (newActiveId != m_activeSessionId) {
            // 保存旧会话的消息（如果旧会话还存在）
            if (!m_activeSessionId.isEmpty() && m_messageModel) {
                Session* oldSession = getSession(m_activeSessionId);
                if (oldSession) {
                    m_messageModel->syncToSession(*oldSession);
                }
            }
            
            // 更新当前活动会话ID
            m_activeSessionId = newActiveId;
            
            if (newActiveId.isEmpty()) {
                // 没有活动会话，清空消息模型
                if (m_messageModel) {
                    m_messageModel->clear();
                }
            } else {
                // 加载新会话的消息
                if (m_messageModel) {
                    loadSessionMessages(newActiveId);
                }
            }
            
            emit activeSessionChanged();
        }
    });
}

SessionController::~SessionController()
{
}

void SessionController::setMessageModel(MessageModel* model)
{
    if (m_messageModel) {
        disconnect(m_messageModel, &MessageModel::countChanged, this, nullptr);
    }
    
    m_messageModel = model;
    
    if (m_messageModel) {
        connect(m_messageModel, &MessageModel::countChanged, this, &SessionController::onMessageCountChanged);
    }
}

void SessionController::onMessageCountChanged()
{
    QString currentId = m_sessionModel->activeSessionId();
    if (currentId.isEmpty() || !m_messageModel) return;
    
    Session* session = getSession(currentId);
    if (session) {
        m_messageModel->syncToSession(*session);
        m_sessionModel->notifySessionDataChanged(currentId);
    }
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
    session.isActive = false;  // 不自动选中
    session.isSelected = false;
    session.isPinned = false;
    session.sortIndex = m_sessionModel->rowCount();

    m_sessionModel->addSession(session);

    emit sessionCreated(session.id);
    emit sessionCountChanged();

    return session.id;
}

QString SessionController::createAndSelectSession(const QString& name)
{
    QString sessionId = createSession(name);
    switchSession(sessionId);  // 锁定选中
    return sessionId;
}

void SessionController::switchSession(const QString& sessionId)
{
    QString currentActiveId = m_sessionModel->activeSessionId();
    if (currentActiveId == sessionId) return;

    // 1. 保存当前会话的消息
    saveCurrentSessionMessages();

    // 2. 切换会话
    m_sessionModel->switchSession(sessionId);
    m_activeSessionId = sessionId;

    // 3. 加载新会话的消息
    loadSessionMessages(sessionId);

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
    QString activeId = m_sessionModel->activeSessionId();
    bool isActiveSession = (sessionId == activeId);
    
    // 先删除会话（SessionModel 会自动切换到相邻会话）
    m_sessionModel->deleteSession(sessionId);
    
    // 如果删除的是当前活动会话，需要处理消息模型
    if (isActiveSession) {
        // 获取新的活动会话ID
        QString newActiveId = m_sessionModel->activeSessionId();
        
        if (newActiveId.isEmpty()) {
            // 没有剩余会话，清空消息模型
            if (m_messageModel) {
                m_messageModel->clear();
                m_messageModel->setCurrentSessionId("");
            }
        } else {
            // 切换到了新会话，加载新会话的消息
            m_activeSessionId = newActiveId;
            loadSessionMessages(newActiveId);
        }
    }
    
    emit sessionDeleted(sessionId);
    emit sessionCountChanged();
}

void SessionController::clearSession(const QString& sessionId)
{
    // 清空 Session 中的消息
    m_sessionModel->clearSession(sessionId);
    
    // 如果清空的是当前活动会话，同步清空消息模型
    if (sessionId == m_sessionModel->activeSessionId() && m_messageModel) {
        m_messageModel->clear();
    }
    
    emit sessionCleared(sessionId);
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
    QString activeId = m_sessionModel->activeSessionId();
    
    // 检查当前活动会话是否被选中
    bool activeSessionDeleted = false;
    Session* activeSession = getSession(activeId);
    if (activeSession && activeSession->isSelected) {
        activeSessionDeleted = true;
    }
    
    // 删除选中的会话
    int count = m_sessionModel->deleteSelectedSessions();
    if (count > 0) {
        emit sessionCountChanged();
        emit selectionChanged();
        
        // 如果当前活动会话被删除，SessionModel 会自动切换到相邻会话
        // activeSessionChanged 信号会触发消息模型的更新
        // 但如果没有剩余会话，需要手动清空消息模型
        if (activeSessionDeleted && m_sessionModel->activeSessionId().isEmpty() && m_messageModel) {
            m_messageModel->clear();
            m_activeSessionId.clear();
            emit activeSessionChanged();
        }
    }
    setBatchSelectionMode(false);
}

void SessionController::clearSelectedSessions()
{
    QString activeId = m_sessionModel->activeSessionId();
    
    // 检查当前活动会话是否被选中
    bool activeSessionCleared = false;
    Session* activeSession = getSession(activeId);
    if (activeSession && activeSession->isSelected) {
        activeSessionCleared = true;
    }
    
    // 清空选中的会话
    m_sessionModel->clearSelectedSessions();
    
    // 如果当前活动会话被清空，同步清空消息模型
    if (activeSessionCleared && m_messageModel) {
        m_messageModel->clear();
    }
    
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
    // 如果已有活动会话，直接返回（复用现有会话）
    if (!m_sessionModel->activeSessionId().isEmpty()) {
        return m_sessionModel->activeSessionId();
    }
    
    // 没有活动会话，创建新会话并锁定选中
    QString newSessionId = createSession();
    switchSession(newSessionId);
    return newSessionId;
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

bool SessionController::autoSaveEnabled() const
{
    return m_autoSaveEnabled;
}

void SessionController::setAutoSaveEnabled(bool enabled)
{
    if (m_autoSaveEnabled != enabled) {
        m_autoSaveEnabled = enabled;
        emit autoSaveEnabledChanged();
    }
}

void SessionController::saveSessions()
{
}

void SessionController::loadSessions()
{
}

void SessionController::saveCurrentSessionMessages()
{
    if (!m_messageModel) return;
    
    QString currentId = m_sessionModel->activeSessionId();
    if (currentId.isEmpty()) return;
    
    Session* session = getSession(currentId);
    if (session) {
        m_messageModel->syncToSession(*session);
        
        // 通知 SessionModel 数据变化，更新 messageCount
        m_sessionModel->notifySessionDataChanged(currentId);
    }
}

void SessionController::syncCurrentMessagesToSession()
{
    saveCurrentSessionMessages();
}

void SessionController::loadSessionMessages(const QString& sessionId)
{
    if (!m_messageModel) return;
    
    Session* session = getSession(sessionId);
    if (session) {
        m_messageModel->loadFromSession(*session);
    }
}

} // namespace EnhanceVision
