/**
 * @file MessageModel.cpp
 * @brief 消息列表模型实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/models/MessageModel.h"
#include <QUuid>
#include <QDateTime>

namespace EnhanceVision {

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

MessageModel::~MessageModel()
{
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_messages.size();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return QVariant();
    }

    const Message &message = m_messages.at(index.row());

    switch (static_cast<Roles>(role)) {
    case IdRole:
        return message.id;
    case TimestampRole:
        return message.timestamp;
    case ModeRole:
        return static_cast<int>(message.mode);
    case MediaFilesRole:
        // 转换为QVariantList
        return QVariant(); // 暂时返回空，后续实现媒体文件列表的QVariant转换
    case ParametersRole:
        return message.parameters;
    case ShaderParamsRole:
        return QVariant(); // 暂时返回空
    case StatusRole:
        return static_cast<int>(message.status);
    case ErrorMessageRole:
        return message.errorMessage;
    case IsSelectedRole:
        return message.isSelected;
    case ProgressRole:
        return message.progress;
    case QueuePositionRole:
        return message.queuePosition;
    case ModeTextRole:
        return getModeText(message.mode);
    case StatusTextRole:
        return getStatusText(message.status);
    case StatusColorRole:
        return getStatusColor(message.status);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TimestampRole] = "timestamp";
    roles[ModeRole] = "mode";
    roles[MediaFilesRole] = "mediaFiles";
    roles[ParametersRole] = "parameters";
    roles[ShaderParamsRole] = "shaderParams";
    roles[StatusRole] = "status";
    roles[ErrorMessageRole] = "errorMessage";
    roles[IsSelectedRole] = "isSelected";
    roles[ProgressRole] = "progress";
    roles[QueuePositionRole] = "queuePosition";
    roles[ModeTextRole] = "modeText";
    roles[StatusTextRole] = "statusText";
    roles[StatusColorRole] = "statusColor";
    return roles;
}

QString MessageModel::addMessage(const Message &message)
{
    Message newMessage = message;
    if (newMessage.id.isEmpty()) {
        newMessage.id = generateId();
    }
    if (newMessage.timestamp.isNull()) {
        newMessage.timestamp = QDateTime::currentDateTime();
    }

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(newMessage);
    endInsertRows();

    emit countChanged();
    emit messageAdded(newMessage.id);

    return newMessage.id;
}

QString MessageModel::addMessage(int mode, const QVariantList &mediaFiles, const QVariantMap &parameters)
{
    Message message;
    message.mode = static_cast<ProcessingMode>(mode);
    message.parameters = parameters;
    // 暂时不处理mediaFiles，后续实现
    return addMessage(message);
}

void MessageModel::updateStatus(const QString &messageId, int status)
{
    int index = findMessageIndex(messageId);
    if (index < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return;
    }

    m_messages[index].status = static_cast<ProcessingStatus>(status);
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {StatusRole, StatusTextRole, StatusColorRole});
    emit statusUpdated(messageId, status);
}

void MessageModel::updateProgress(const QString &messageId, int progress)
{
    int index = findMessageIndex(messageId);
    if (index < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return;
    }

    m_messages[index].progress = qBound(0, progress, 100);
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {ProgressRole});
}

void MessageModel::updateErrorMessage(const QString &messageId, const QString &errorMessage)
{
    int index = findMessageIndex(messageId);
    if (index < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return;
    }

    m_messages[index].errorMessage = errorMessage;
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {ErrorMessageRole});
}

bool MessageModel::removeMessage(const QString &messageId)
{
    int index = findMessageIndex(messageId);
    if (index < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return false;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_messages.removeAt(index);
    endRemoveRows();

    emit countChanged();
    emit messageRemoved(messageId);
    return true;
}

void MessageModel::selectMessage(const QString &messageId, bool selected)
{
    int index = findMessageIndex(messageId);
    if (index < 0) {
        return;
    }

    m_messages[index].isSelected = selected;
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsSelectedRole});
}

void MessageModel::clearSelection()
{
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].isSelected) {
            m_messages[i].isSelected = false;
            QModelIndex modelIndex = createIndex(i, 0);
            emit dataChanged(modelIndex, modelIndex, {IsSelectedRole});
        }
    }
}

int MessageModel::removeSelectedMessages()
{
    int deletedCount = 0;
    QList<QString> toDelete;

    // 收集要删除的消息ID
    for (const Message &message : m_messages) {
        if (message.isSelected) {
            toDelete.append(message.id);
        }
    }

    // 删除消息（从后往前删，避免索引问题）
    for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it) {
        if (removeMessage(*it)) {
            deletedCount++;
        }
    }

    return deletedCount;
}

void MessageModel::clear()
{
    if (m_messages.isEmpty()) {
        return;
    }

    beginResetModel();
    m_messages.clear();
    endResetModel();
    emit countChanged();
}

Message MessageModel::messageById(const QString &messageId) const
{
    int index = findMessageIndex(messageId);
    if (index >= 0) {
        return m_messages.at(index);
    }
    return Message();
}

QString MessageModel::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int MessageModel::findMessageIndex(const QString &messageId) const
{
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].id == messageId) {
            return i;
        }
    }
    return -1;
}

QString MessageModel::getStatusText(ProcessingStatus status) const
{
    switch (status) {
    case ProcessingStatus::Pending:
        return tr("待处理");
    case ProcessingStatus::Processing:
        return tr("处理中");
    case ProcessingStatus::Completed:
        return tr("已完成");
    case ProcessingStatus::Failed:
        return tr("失败");
    case ProcessingStatus::Cancelled:
        return tr("已取消");
    default:
        return tr("未知");
    }
}

QString MessageModel::getStatusColor(ProcessingStatus status) const
{
    switch (status) {
    case ProcessingStatus::Pending:
        return "#f59e0b"; // amber-500
    case ProcessingStatus::Processing:
        return "#3b82f6"; // blue-500
    case ProcessingStatus::Completed:
        return "#10b981"; // emerald-500
    case ProcessingStatus::Failed:
        return "#ef4444"; // red-500
    case ProcessingStatus::Cancelled:
        return "#6b7280"; // gray-500
    default:
        return "#6b7280"; // gray-500
    }
}

QString MessageModel::getModeText(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Shader:
        return tr("Shader 滤镜");
    case ProcessingMode::AIInference:
        return tr("AI 推理");
    case ProcessingMode::Browse:
        return tr("浏览");
    default:
        return tr("未知");
    }
}

} // namespace EnhanceVision
