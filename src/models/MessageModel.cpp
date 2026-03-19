/**
 * @file MessageModel.cpp
 * @brief 消息列表模型实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/models/MessageModel.h"
#include <QUuid>
#include <QDateTime>
#include <QtMath>

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
    case MediaFilesRole: {
        QVariantList filesList;
        
        bool hasShaderModifications = 
            qAbs(message.shaderParams.brightness) > 0.001f ||
            qAbs(message.shaderParams.contrast - 1.0f) > 0.001f ||
            qAbs(message.shaderParams.saturation - 1.0f) > 0.001f ||
            qAbs(message.shaderParams.hue) > 0.001f ||
            qAbs(message.shaderParams.sharpness) > 0.001f ||
            qAbs(message.shaderParams.blur) > 0.001f ||
            qAbs(message.shaderParams.denoise) > 0.001f ||
            qAbs(message.shaderParams.exposure) > 0.001f ||
            qAbs(message.shaderParams.gamma - 1.0f) > 0.001f ||
            qAbs(message.shaderParams.temperature) > 0.001f ||
            qAbs(message.shaderParams.tint) > 0.001f ||
            qAbs(message.shaderParams.vignette) > 0.001f ||
            qAbs(message.shaderParams.highlights) > 0.001f ||
            qAbs(message.shaderParams.shadows) > 0.001f;
        
                
        for (const MediaFile &file : message.mediaFiles) {
            QVariantMap fileMap;
            fileMap["id"] = file.id;
            fileMap["filePath"] = file.filePath;
            fileMap["fileName"] = file.fileName;
            fileMap["fileSize"] = file.fileSize;
            fileMap["mediaType"] = static_cast<int>(file.type);
            fileMap["thumbnail"] = "";
            fileMap["status"] = static_cast<int>(file.status);
            fileMap["resultPath"] = file.resultPath;
            fileMap["duration"] = file.duration;
            
            if (message.mode == ProcessingMode::Shader && hasShaderModifications && file.status == ProcessingStatus::Completed) {
                fileMap["processedThumbnailId"] = "processed_" + file.id;
            }
            
            filesList.append(fileMap);
        }
        return filesList;
    }
    case ParametersRole:
        return message.parameters;
    case ShaderParamsRole: {
        QVariantMap shaderMap;
        shaderMap["brightness"] = message.shaderParams.brightness;
        shaderMap["contrast"] = message.shaderParams.contrast;
        shaderMap["saturation"] = message.shaderParams.saturation;
        shaderMap["hue"] = message.shaderParams.hue;
        shaderMap["sharpness"] = message.shaderParams.sharpness;
        shaderMap["blur"] = message.shaderParams.blur;
        shaderMap["denoise"] = message.shaderParams.denoise;
        shaderMap["exposure"] = message.shaderParams.exposure;
        shaderMap["gamma"] = message.shaderParams.gamma;
        shaderMap["temperature"] = message.shaderParams.temperature;
        shaderMap["tint"] = message.shaderParams.tint;
        shaderMap["vignette"] = message.shaderParams.vignette;
        shaderMap["highlights"] = message.shaderParams.highlights;
        shaderMap["shadows"] = message.shaderParams.shadows;
        return shaderMap;
    }
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

    // 将 QVariantList 转换为 QList<MediaFile>
    for (const QVariant &item : mediaFiles) {
        QVariantMap fileMap = item.toMap();
        MediaFile file;
        file.id = fileMap.value("id").toString();
        file.filePath = fileMap.value("filePath").toString();
        file.fileName = fileMap.value("fileName").toString();
        file.fileSize = fileMap.value("fileSize").toLongLong();
        file.type = static_cast<MediaType>(fileMap.value("mediaType", 0).toInt());
        file.status = static_cast<ProcessingStatus>(fileMap.value("status", 0).toInt());
        file.resultPath = fileMap.value("resultPath").toString();
        file.duration = fileMap.value("duration").toLongLong();
        message.mediaFiles.append(file);
    }

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

void MessageModel::setCurrentSessionId(const QString& sessionId)
{
    if (m_currentSessionId != sessionId) {
        m_currentSessionId = sessionId;
    }
}

void MessageModel::syncToSession(Session& session)
{
    session.messages = m_messages;
}

void MessageModel::loadFromSession(const Session& session)
{
    beginResetModel();
    m_messages = session.messages;
    m_currentSessionId = session.id;
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

void MessageModel::updateFileStatus(const QString &messageId, const QString &fileId,
                                     int status, const QString &resultPath)
{
    int idx = findMessageIndex(messageId);
    if (idx < 0) return;

    Message &message = m_messages[idx];
    for (int i = 0; i < message.mediaFiles.size(); ++i) {
        if (message.mediaFiles[i].id == fileId) {
            message.mediaFiles[i].status = static_cast<ProcessingStatus>(status);
            if (!resultPath.isEmpty()) {
                message.mediaFiles[i].resultPath = resultPath;
            }
            QModelIndex modelIndex = createIndex(idx, 0);
            emit dataChanged(modelIndex, modelIndex, {MediaFilesRole});
            return;
        }
    }
}

void MessageModel::updateQueuePosition(const QString &messageId, int position)
{
    int idx = findMessageIndex(messageId);
    if (idx < 0) return;

    if (m_messages[idx].queuePosition != position) {
        m_messages[idx].queuePosition = position;
        QModelIndex modelIndex = createIndex(idx, 0);
        emit dataChanged(modelIndex, modelIndex, {QueuePositionRole});
    }
}

int MessageModel::getCompletedFileCount(const QString &messageId) const
{
    int idx = findMessageIndex(messageId);
    if (idx < 0) return 0;

    int count = 0;
    for (const MediaFile &file : m_messages.at(idx).mediaFiles) {
        if (file.status == ProcessingStatus::Completed) {
            count++;
        }
    }
    return count;
}

int MessageModel::getTotalFileCount(const QString &messageId) const
{
    int idx = findMessageIndex(messageId);
    if (idx < 0) return 0;
    return m_messages.at(idx).mediaFiles.size();
}

QVariantList MessageModel::getCompletedFiles(const QString &messageId) const
{
    QVariantList result;
    int idx = findMessageIndex(messageId);
    if (idx < 0) return result;

    for (const MediaFile &file : m_messages.at(idx).mediaFiles) {
        if (file.status == ProcessingStatus::Completed) {
            QVariantMap fileMap;
            fileMap["id"] = file.id;
            fileMap["filePath"] = file.filePath;
            fileMap["fileName"] = file.fileName;
            fileMap["fileSize"] = file.fileSize;
            fileMap["mediaType"] = static_cast<int>(file.type);
            fileMap["status"] = static_cast<int>(file.status);
            fileMap["resultPath"] = file.resultPath;
            fileMap["duration"] = file.duration;
            result.append(fileMap);
        }
    }
    return result;
}

QVariantList MessageModel::getMediaFiles(const QString &messageId) const
{
    QVariantList result;
    int idx = findMessageIndex(messageId);
    if (idx < 0) return result;

    const Message &msg = m_messages.at(idx);
    const ShaderParams &p = msg.shaderParams;
    bool hasShaderModifications = 
        qAbs(p.brightness) > 0.001f ||
        qAbs(p.contrast - 1.0f) > 0.001f ||
        qAbs(p.saturation - 1.0f) > 0.001f ||
        qAbs(p.hue) > 0.001f ||
        qAbs(p.sharpness) > 0.001f ||
        qAbs(p.blur) > 0.001f ||
        qAbs(p.denoise) > 0.001f ||
        qAbs(p.exposure) > 0.001f ||
        qAbs(p.gamma - 1.0f) > 0.001f ||
        qAbs(p.temperature) > 0.001f ||
        qAbs(p.tint) > 0.001f ||
        qAbs(p.vignette) > 0.001f ||
        qAbs(p.highlights) > 0.001f ||
        qAbs(p.shadows) > 0.001f;

    for (const MediaFile &file : msg.mediaFiles) {
        QVariantMap fileMap;
        fileMap["id"] = file.id;
        fileMap["filePath"] = file.filePath;
        fileMap["fileName"] = file.fileName;
        fileMap["fileSize"] = file.fileSize;
        fileMap["mediaType"] = static_cast<int>(file.type);
        fileMap["status"] = static_cast<int>(file.status);
        fileMap["resultPath"] = file.resultPath;
        fileMap["duration"] = file.duration;
        
        if (msg.mode == ProcessingMode::Shader && hasShaderModifications && file.status == ProcessingStatus::Completed) {
            fileMap["processedThumbnailId"] = "processed_" + file.id;
        }
        
        result.append(fileMap);
    }
    return result;
}

QVariantMap MessageModel::getShaderParams(const QString &messageId) const
{
    QVariantMap result;
    int idx = findMessageIndex(messageId);
    if (idx < 0) return result;

    const Message &msg = m_messages.at(idx);
    const ShaderParams &params = msg.shaderParams;
    result["brightness"] = params.brightness;
    result["contrast"] = params.contrast;
    result["saturation"] = params.saturation;
    result["hue"] = params.hue;
    result["sharpness"] = params.sharpness;
    result["blur"] = params.blur;
    result["denoise"] = params.denoise;
    result["exposure"] = params.exposure;
    result["gamma"] = params.gamma;
    result["temperature"] = params.temperature;
    result["tint"] = params.tint;
    result["vignette"] = params.vignette;
    result["highlights"] = params.highlights;
    result["shadows"] = params.shadows;
    
    bool hasShaderModifications = 
        qAbs(params.brightness) > 0.001f ||
        qAbs(params.contrast - 1.0f) > 0.001f ||
        qAbs(params.saturation - 1.0f) > 0.001f ||
        qAbs(params.hue) > 0.001f ||
        qAbs(params.sharpness) > 0.001f ||
        qAbs(params.blur) > 0.001f ||
        qAbs(params.denoise) > 0.001f ||
        qAbs(params.exposure) > 0.001f ||
        qAbs(params.gamma - 1.0f) > 0.001f ||
        qAbs(params.temperature) > 0.001f ||
        qAbs(params.tint) > 0.001f ||
        qAbs(params.vignette) > 0.001f ||
        qAbs(params.highlights) > 0.001f ||
        qAbs(params.shadows) > 0.001f;
    result["hasShaderModifications"] = hasShaderModifications;
    
    // 对于 Shader 模式，如果文件已处理完成，则 Shader 效果已经应用到导出的图像上
    // 不需要再次应用 Shader 效果
    bool isShaderMode = (msg.mode == ProcessingMode::Shader);
    bool hasCompletedFiles = false;
    for (const MediaFile &file : msg.mediaFiles) {
        if (file.status == ProcessingStatus::Completed) {
            hasCompletedFiles = true;
            break;
        }
    }
    result["shaderAlreadyApplied"] = isShaderMode && hasShaderModifications && hasCompletedFiles;
    
    return result;
}

bool MessageModel::removeMediaFile(const QString &messageId, int fileIndex)
{
    int msgIdx = findMessageIndex(messageId);
    if (msgIdx < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return false;
    }

    Message &message = m_messages[msgIdx];
    if (fileIndex < 0 || fileIndex >= message.mediaFiles.size()) {
        emit errorOccurred(tr("文件索引无效: %1").arg(fileIndex));
        return false;
    }

    message.mediaFiles.removeAt(fileIndex);
    
    if (message.mediaFiles.isEmpty()) {
        beginRemoveRows(QModelIndex(), msgIdx, msgIdx);
        m_messages.removeAt(msgIdx);
        endRemoveRows();
        emit countChanged();
        emit messageRemoved(messageId);
    } else {
        QModelIndex modelIndex = createIndex(msgIdx, 0);
        emit dataChanged(modelIndex, modelIndex, {MediaFilesRole});
        emit mediaFileRemoved(messageId, fileIndex);
    }
    
    return true;
}

bool MessageModel::removeMediaFileById(const QString &messageId, const QString &fileId)
{
    int msgIdx = findMessageIndex(messageId);
    if (msgIdx < 0) {
        return false;
    }

    Message &message = m_messages[msgIdx];
    for (int i = 0; i < message.mediaFiles.size(); ++i) {
        if (message.mediaFiles[i].id == fileId) {
            return removeMediaFile(messageId, i);
        }
    }
    return false;
}

} // namespace EnhanceVision
