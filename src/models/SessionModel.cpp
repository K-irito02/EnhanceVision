/**
 * @file SessionModel.cpp
 * @brief 会话列表模型实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/models/SessionModel.h"
#include <QUuid>
#include <QDateTime>

namespace EnhanceVision {

SessionModel::SessionModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_sessionCounter(0)
{
    // 创建默认会话
    createSession();
}

SessionModel::~SessionModel()
{
}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_sessions.size();
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sessions.size()) {
        return QVariant();
    }

    const Session &session = m_sessions.at(index.row());

    switch (static_cast<Roles>(role)) {
    case IdRole:
        return session.id;
    case NameRole:
        return session.name;
    case CreatedAtRole:
        return session.createdAt;
    case ModifiedAtRole:
        return session.modifiedAt;
    case IsActiveRole:
        return session.isActive;
    case IsSelectedRole:
        return session.isSelected;
    case MessageCountRole:
        return session.messages.size();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SessionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[CreatedAtRole] = "createdAt";
    roles[ModifiedAtRole] = "modifiedAt";
    roles[IsActiveRole] = "isActive";
    roles[IsSelectedRole] = "isSelected";
    roles[MessageCountRole] = "messageCount";
    return roles;
}

QString SessionModel::createSession(const QString &name)
{
    m_sessionCounter++;

    Session session;
    session.id = generateId();
    session.name = name.isEmpty() ? generateDefaultName() : name;
    session.createdAt = QDateTime::currentDateTime();
    session.modifiedAt = QDateTime::currentDateTime();
    session.isActive = m_sessions.isEmpty();  // 第一个会话设为活动
    session.isSelected = false;

    beginInsertRows(QModelIndex(), m_sessions.size(), m_sessions.size());
    m_sessions.append(session);
    endInsertRows();

    // 如果是第一个会话，设置为活动会话
    if (m_sessions.size() == 1) {
        m_activeSessionId = session.id;
        emit activeSessionChanged();
    }

    emit countChanged();
    emit sessionCreated(session.id);

    return session.id;
}

void SessionModel::switchSession(const QString &sessionId)
{
    if (m_activeSessionId == sessionId) {
        return;
    }

    int oldIndex = findSessionIndex(m_activeSessionId);
    int newIndex = findSessionIndex(sessionId);

    if (newIndex < 0) {
        emit errorOccurred(tr("会话不存在: %1").arg(sessionId));
        return;
    }

    // 更新旧会话状态
    if (oldIndex >= 0) {
        m_sessions[oldIndex].isActive = false;
        QModelIndex oldModelIndex = index(oldIndex);
        emit dataChanged(oldModelIndex, oldModelIndex, {IsActiveRole});
    }

    // 更新新会话状态
    m_sessions[newIndex].isActive = true;
    m_sessions[newIndex].modifiedAt = QDateTime::currentDateTime();
    m_activeSessionId = sessionId;

    QModelIndex newModelIndex = index(newIndex);
    emit dataChanged(newModelIndex, newModelIndex, {IsActiveRole, ModifiedAtRole});

    emit activeSessionChanged();
}

bool SessionModel::renameSession(const QString &sessionId, const QString &newName)
{
    int index = findSessionIndex(sessionId);
    if (index < 0) {
        emit errorOccurred(tr("会话不存在: %1").arg(sessionId));
        return false;
    }

    if (newName.trimmed().isEmpty()) {
        emit errorOccurred(tr("会话名称不能为空"));
        return false;
    }

    if (newName.length() > 50) {
        emit errorOccurred(tr("会话名称不能超过50个字符"));
        return false;
    }

    m_sessions[index].name = newName.trimmed();
    m_sessions[index].modifiedAt = QDateTime::currentDateTime();

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {NameRole, ModifiedAtRole});

    emit sessionRenamed(sessionId, newName);
    return true;
}

bool SessionModel::deleteSession(const QString &sessionId)
{
    int index = findSessionIndex(sessionId);
    if (index < 0) {
        emit errorOccurred(tr("会话不存在: %1").arg(sessionId));
        return false;
    }

    if (m_sessions.size() <= 1) {
        emit errorOccurred(tr("至少需要保留一个会话"));
        return false;
    }

    bool wasActive = m_sessions[index].isActive;
    QString deletedId = sessionId;

    beginRemoveRows(QModelIndex(), index, index);
    m_sessions.removeAt(index);
    endRemoveRows();

    // 如果删除的是活动会话，切换到相邻会话
    if (wasActive && !m_sessions.isEmpty()) {
        int newActiveIndex = qMin(index, m_sessions.size() - 1);
        switchSession(m_sessions[newActiveIndex].id);
    }

    emit countChanged();
    emit sessionDeleted(deletedId);
    return true;
}

void SessionModel::clearSession(const QString &sessionId)
{
    int index = findSessionIndex(sessionId);
    if (index < 0) {
        emit errorOccurred(tr("会话不存在: %1").arg(sessionId));
        return;
    }

    m_sessions[index].messages.clear();
    m_sessions[index].modifiedAt = QDateTime::currentDateTime();

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {MessageCountRole, ModifiedAtRole});
}

void SessionModel::selectSession(const QString &sessionId, bool selected)
{
    int index = findSessionIndex(sessionId);
    if (index < 0) {
        return;
    }

    m_sessions[index].isSelected = selected;
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsSelectedRole});
}

void SessionModel::clearSelection()
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].isSelected) {
            m_sessions[i].isSelected = false;
            QModelIndex modelIndex = createIndex(i, 0);
            emit dataChanged(modelIndex, modelIndex, {IsSelectedRole});
        }
    }
}

int SessionModel::deleteSelectedSessions()
{
    int deletedCount = 0;
    QList<QString> toDelete;

    // 收集要删除的会话ID
    for (const Session &session : m_sessions) {
        if (session.isSelected) {
            toDelete.append(session.id);
        }
    }

    // 检查是否会删除所有会话
    if (toDelete.size() >= m_sessions.size()) {
        emit errorOccurred(tr("至少需要保留一个会话"));
        return 0;
    }

    // 删除会话（从后往前删，避免索引问题）
    for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it) {
        if (deleteSession(*it)) {
            deletedCount++;
        }
    }

    return deletedCount;
}

Session SessionModel::sessionById(const QString &sessionId) const
{
    int index = findSessionIndex(sessionId);
    if (index >= 0) {
        return m_sessions.at(index);
    }
    return Session();
}

void SessionModel::addSession(const Session &session)
{
    beginInsertRows(QModelIndex(), m_sessions.size(), m_sessions.size());
    m_sessions.append(session);
    endInsertRows();
    emit countChanged();
}

void SessionModel::updateSessions(const QList<Session> &sessions)
{
    beginResetModel();
    m_sessions = sessions;
    endResetModel();
    emit countChanged();
}

void SessionModel::updateSession(const Session &session)
{
    int index = findSessionIndex(session.id);
    if (index >= 0) {
        m_sessions[index] = session;
        QModelIndex modelIndex = createIndex(index, 0);
        emit dataChanged(modelIndex, modelIndex, {NameRole, ModifiedAtRole, IsActiveRole, IsSelectedRole, MessageCountRole});
    }
}

void SessionModel::removeSession(const QString &sessionId)
{
    int index = findSessionIndex(sessionId);
    if (index >= 0) {
        beginRemoveRows(QModelIndex(), index, index);
        m_sessions.removeAt(index);
        endRemoveRows();
        emit countChanged();
    }
}

QString SessionModel::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString SessionModel::generateDefaultName() const
{
    return tr("未命名会话 %1").arg(m_sessionCounter);
}

int SessionModel::findSessionIndex(const QString &sessionId) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].id == sessionId) {
            return i;
        }
    }
    return -1;
}

} // namespace EnhanceVision
