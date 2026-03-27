/**
 * @file SessionController.cpp
 * @brief 会话控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/models/MessageModel.h"
#include "EnhanceVision/core/TaskCoordinator.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUuid>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QtConcurrent>

namespace EnhanceVision {

SessionController::SessionController(QObject* parent)
    : QObject(parent)
    , m_sessionModel(new SessionModel(this))
    , m_messageModel(nullptr)
    , m_batchSelectionMode(false)
    , m_sessionCounter(0)
    , m_autoSaveEnabled(true)
    , m_lastAutoSaveMs(0)
    , m_saveTimer(new QTimer(this))
    , m_sessionNotifyTimer(new QTimer(this))
{
    connect(m_sessionModel, &SessionModel::errorOccurred, this, &SessionController::errorOccurred);

    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1800);
    connect(m_saveTimer, &QTimer::timeout, this, [this]() {
        if (!m_autoSaveEnabled) {
            return;
        }

        if (m_hasPendingSave) {
            m_hasPendingSave = false;
            saveSessions();
        }
    });

    m_sessionNotifyTimer->setSingleShot(true);
    m_sessionNotifyTimer->setInterval(120);
    connect(m_sessionNotifyTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingNotifySessionIds.isEmpty()) {
            return;
        }

        const QSet<QString> sessionIds = m_pendingNotifySessionIds;
        m_pendingNotifySessionIds.clear();

        for (const QString& sessionId : sessionIds) {
            m_sessionModel->notifySessionDataChanged(sessionId);
        }
    });
    
    // 连接 SessionModel 的 activeSessionChanged 信号，处理内部会话切换（如删除会话后自动切换）
    connect(m_sessionModel, &SessionModel::activeSessionChanged, this, [this]() {
        QString newActiveId = m_sessionModel->activeSessionId();
        if (newActiveId != m_activeSessionId) {
            const QString oldActiveId = m_activeSessionId;

            if (m_processingController && !oldActiveId.isEmpty() && oldActiveId != newActiveId) {
                m_processingController->onSessionChanging(oldActiveId, newActiveId);
            }

            // 保存旧会话的消息（如果旧会话还存在）
            if (!oldActiveId.isEmpty() && m_messageModel) {
                Session* oldSession = getSession(oldActiveId);
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

    rebuildSessionMessageIndex();
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

void SessionController::setProcessingController(ProcessingController* controller)
{
    m_processingController = controller;
}

void SessionController::onMessageCountChanged()
{
    QString currentId = m_messageModel ? m_messageModel->currentSessionId() : QString();
    if (currentId.isEmpty() || !m_messageModel) return;
    
    Session* session = getSession(currentId);
    if (session) {
        m_messageModel->syncToSession(*session);
        rebuildMessageRowIndexForSession(currentId, session->messages);
        for (const Message& message : session->messages) {
            m_messageToSessionId[message.id] = currentId;
        }
        m_pendingNotifySessionIds.insert(currentId);
        if (m_sessionNotifyTimer) {
            m_sessionNotifyTimer->start();
        }
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
    rebuildSessionMessageIndex();

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
    QElapsedTimer perfTimer;
    perfTimer.start();

    const QString currentActiveId = m_sessionModel->activeSessionId();
    if (currentActiveId == sessionId) return;

    // 1. 保存当前会话的消息
    saveCurrentSessionMessages();

    // 2. 切换会话
    m_sessionModel->switchSession(sessionId);
    m_activeSessionId = sessionId;

    // 3. 加载新会话的消息
    loadSessionMessages(sessionId);

    qInfo() << "[Perf][SessionController] switchSession cost:" << perfTimer.elapsed() << "ms";

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
    
    if (m_processingController) {
        m_processingController->cancelSessionTasks(sessionId);
    }
    
    m_sessionModel->deleteSession(sessionId);
    rebuildSessionMessageIndex();
    
    if (isActiveSession) {
        QString newActiveId = m_sessionModel->activeSessionId();
        
        if (newActiveId.isEmpty()) {
            if (m_messageModel) {
                m_messageModel->clear();
                m_messageModel->setCurrentSessionId("");
            }
        } else {
            m_activeSessionId = newActiveId;
            loadSessionMessages(newActiveId);
        }
    }
    
    emit sessionDeleted(sessionId);
    emit sessionCountChanged();
}

void SessionController::clearSession(const QString& sessionId)
{
    if (m_processingController) {
        m_processingController->cancelSessionTasks(sessionId);
    }
    
    m_sessionModel->clearSession(sessionId);
    rebuildSessionMessageIndex();
    
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
    
    bool activeSessionDeleted = false;
    Session* activeSession = getSession(activeId);
    if (activeSession && activeSession->isSelected) {
        activeSessionDeleted = true;
    }
    
    QList<QString> selectedIds;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        Session session = m_sessionModel->sessionAt(i);
        if (session.isSelected) {
            selectedIds.append(session.id);
        }
    }
    
    if (m_processingController) {
        for (const QString& sessionId : selectedIds) {
            m_processingController->cancelSessionTasks(sessionId);
        }
    }
    
    int count = m_sessionModel->deleteSelectedSessions();
    if (count > 0) {
        rebuildSessionMessageIndex();
        emit sessionCountChanged();
        emit selectionChanged();
        
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
    
    bool activeSessionCleared = false;
    Session* activeSession = getSession(activeId);
    if (activeSession && activeSession->isSelected) {
        activeSessionCleared = true;
    }
    
    QList<QString> selectedIds;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        Session session = m_sessionModel->sessionAt(i);
        if (session.isSelected) {
            selectedIds.append(session.id);
        }
    }
    
    if (m_processingController) {
        for (const QString& sessionId : selectedIds) {
            m_processingController->cancelSessionTasks(sessionId);
        }
    }
    
    m_sessionModel->clearSelectedSessions();
    rebuildSessionMessageIndex();
    
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
    if (sessionId.isEmpty()) {
        return nullptr;
    }

    const auto indexIt = m_sessionRowById.constFind(sessionId);
    if (indexIt == m_sessionRowById.constEnd()) {
        return nullptr;
    }

    QList<Session>& sessions = m_sessionModel->sessionsRef();
    const int index = indexIt.value();
    if (index < 0 || index >= sessions.size() || sessions[index].id != sessionId) {
        return nullptr;
    }

    return &sessions[index];
}

const Session* SessionController::getSessionConst(const QString& sessionId) const
{
    if (sessionId.isEmpty()) {
        return nullptr;
    }

    const auto indexIt = m_sessionRowById.constFind(sessionId);
    if (indexIt == m_sessionRowById.constEnd()) {
        return nullptr;
    }

    const QList<Session>& sessions = m_sessionModel->sessionsRef();
    const int index = indexIt.value();
    if (index < 0 || index >= sessions.size() || sessions[index].id != sessionId) {
        return nullptr;
    }

    return &sessions[index];
}

Message SessionController::messageInSession(const QString& sessionId, const QString& messageId) const
{
    const Session* session = getSessionConst(sessionId);
    if (!session) {
        return Message();
    }

    const auto sessionMessageIndexIt = m_messageRowBySessionId.constFind(sessionId);
    if (sessionMessageIndexIt != m_messageRowBySessionId.constEnd()) {
        const auto messageIndexIt = sessionMessageIndexIt.value().constFind(messageId);
        if (messageIndexIt != sessionMessageIndexIt.value().constEnd()) {
            const int messageIndex = messageIndexIt.value();
            if (messageIndex >= 0 && messageIndex < session->messages.size()) {
                return session->messages.at(messageIndex);
            }
        }
    }

    for (const auto& message : session->messages) {
        if (message.id == messageId) {
            return message;
        }
    }

    return Message();
}

QString SessionController::sessionIdForMessage(const QString& messageId) const
{
    if (messageId.isEmpty()) {
        return QString();
    }

    const auto it = m_messageToSessionId.constFind(messageId);
    if (it != m_messageToSessionId.constEnd()) {
        return it.value();
    }

    return QString();
}

bool SessionController::updateMessageInSession(const QString& sessionId, const Message& message)
{
    Session* session = getSession(sessionId);
    if (!session) {
        return false;
    }

    const auto sessionMessageIndexIt = m_messageRowBySessionId.constFind(sessionId);
    if (sessionMessageIndexIt != m_messageRowBySessionId.constEnd()) {
        const auto messageIndexIt = sessionMessageIndexIt.value().constFind(message.id);
        if (messageIndexIt != sessionMessageIndexIt.value().constEnd()) {
            const int messageIndex = messageIndexIt.value();
            if (messageIndex >= 0 && messageIndex < session->messages.size()) {
                session->messages[messageIndex] = message;
                session->modifiedAt = QDateTime::currentDateTime();

                m_messageToSessionId[message.id] = sessionId;
                m_pendingNotifySessionIds.insert(sessionId);
                if (m_sessionNotifyTimer) {
                    m_sessionNotifyTimer->start();
                }
                return true;
            }
        }
    }

    for (int i = 0; i < session->messages.size(); ++i) {
        if (session->messages[i].id == message.id) {
            session->messages[i] = message;
            session->modifiedAt = QDateTime::currentDateTime();

            m_messageToSessionId[message.id] = sessionId;
            m_messageRowBySessionId[sessionId][message.id] = i;
            m_pendingNotifySessionIds.insert(sessionId);
            if (m_sessionNotifyTimer) {
                m_sessionNotifyTimer->start();
            }
            return true;
        }
    }

    return false;
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

        if (!m_autoSaveEnabled && m_saveTimer) {
            m_saveTimer->stop();
            m_hasPendingSave = false;
            m_saveQueued = false;
        }

        emit autoSaveEnabledChanged();
    }
}

void SessionController::saveSessions()
{
    if (m_saveInProgress) {
        m_saveQueued = true;
        return;
    }

    m_saveInProgress = true;

    ensureDataDirectory();

    QString filePath = sessionsFilePath();

    QJsonObject root;
    root["version"] = 1;
    root["lastActiveSessionId"] = m_sessionModel->activeSessionId();
    root["sessionCounter"] = m_sessionCounter;

    QJsonArray sessionsArray;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        Session session = m_sessionModel->sessionAt(i);
        sessionsArray.append(sessionToJson(session));
    }
    root["sessions"] = sessionsArray;

    const QString tempPath = filePath + ".tmp";

    m_saveFuture = QtConcurrent::run([this, root, tempPath, filePath]() {
        QString errorMessage;

        QFile file(tempPath);
        if (!file.open(QIODevice::WriteOnly)) {
            errorMessage = tr("无法保存会话数据");
        } else {
            QJsonDocument doc(root);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();

            QFile oldFile(filePath);
            if (oldFile.exists()) {
                oldFile.remove();
            }

            if (!file.rename(filePath)) {
                errorMessage = tr("无法保存会话数据");
            }
        }

        QMetaObject::invokeMethod(this, [this, errorMessage]() {
            m_saveInProgress = false;

            if (!errorMessage.isEmpty()) {
                emit errorOccurred(errorMessage);
            } else {
                m_lastAutoSaveMs = QDateTime::currentMSecsSinceEpoch();
            }

            if (m_saveQueued) {
                m_saveQueued = false;
                saveSessions();
            }
        }, Qt::QueuedConnection);
    });
}

void SessionController::loadSessions()
{
    QString filePath = sessionsFilePath();
    QFile file(filePath);
    
    if (!file.exists()) {
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open sessions file:" << filePath;
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse sessions file:" << parseError.errorString();
        return;
    }
    
    QJsonObject root = doc.object();
    
    m_sessionCounter = root["sessionCounter"].toInt(0);
    
    QJsonArray sessionsArray = root["sessions"].toArray();
    for (const QJsonValue& value : sessionsArray) {
        Session session = jsonToSession(value.toObject());
        m_sessionModel->addSession(session);
    }

    rebuildSessionMessageIndex();
    
    QString lastActiveId = root["lastActiveSessionId"].toString();
    if (!lastActiveId.isEmpty()) {
        for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
            Session session = m_sessionModel->sessionAt(i);
            if (session.id == lastActiveId) {
                m_sessionModel->switchSession(lastActiveId);
                m_activeSessionId = lastActiveId;
                break;
            }
        }
    }
    
    emit sessionCountChanged();
}

void SessionController::saveCurrentSessionMessages()
{
    if (!m_messageModel) return;

    QString currentId = m_sessionModel->activeSessionId();
    if (currentId.isEmpty()) return;

    Session* session = getSession(currentId);
    if (session) {
        m_messageModel->syncToSession(*session);

        for (const Message& message : session->messages) {
            m_messageToSessionId[message.id] = currentId;
        }

        m_pendingNotifySessionIds.insert(currentId);
        if (m_sessionNotifyTimer) {
            m_sessionNotifyTimer->start();
        }
    }
}

void SessionController::syncCurrentMessagesToSession()
{
    saveCurrentSessionMessages();

    if (!m_autoSaveEnabled) {
        return;
    }

    m_hasPendingSave = true;

    if (m_saveInProgress) {
        m_saveQueued = true;
    }

    if (m_saveTimer) {
        m_saveTimer->start();
    }
}

void SessionController::loadSessionMessages(const QString& sessionId)
{
    if (!m_messageModel) return;

    Session* session = getSession(sessionId);
    if (session) {
        if (m_processingController) {
            auto* settings = SettingsController::instance();
            QStringList interruptedMessageIds;
            interruptedMessageIds.reserve(session->messages.size());
            bool hasStatusChanges = false;

            for (const Message& msg : session->messages) {
                if (m_autoRetriedMessageIds.contains(msg.id)) {
                    continue;
                }

                bool hasInterruptedFiles = false;
                for (const MediaFile& file : msg.mediaFiles) {
                    if (file.status == ProcessingStatus::Pending || file.status == ProcessingStatus::Processing) {
                        hasInterruptedFiles = true;
                        break;
                    }
                }

                if (hasInterruptedFiles) {
                    bool shouldAutoReprocess = false;
                    
                    if (msg.mode == ProcessingMode::Shader) {
                        shouldAutoReprocess = settings->autoReprocessShaderEnabled();
                    } else if (msg.mode == ProcessingMode::AIInference) {
                        shouldAutoReprocess = settings->autoReprocessAIEnabled();
                    }
                    
                    if (shouldAutoReprocess) {
                        m_autoRetriedMessageIds.insert(msg.id);
                        interruptedMessageIds.append(msg.id);
                    } else {
                        for (Message& m : session->messages) {
                            if (m.id == msg.id) {
                                for (MediaFile& file : m.mediaFiles) {
                                    if (file.status == ProcessingStatus::Pending || 
                                        file.status == ProcessingStatus::Processing) {
                                        file.status = ProcessingStatus::Failed;
                                        hasStatusChanges = true;
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }

            if (hasStatusChanges) {
                saveSessions();
            }

            m_messageModel->setMessages(session->messages);
            m_messageModel->setCurrentSessionId(session->id);

            if (!interruptedMessageIds.isEmpty()) {
                QTimer::singleShot(0, this, [this, sessionId, interruptedMessageIds]() {
                    if (!m_processingController) {
                        return;
                    }

                    for (const QString& messageId : interruptedMessageIds) {
                        m_processingController->autoRetryInterruptedFiles(messageId, sessionId);
                    }
                });
            }
        } else {
            m_messageModel->setMessages(session->messages);
            m_messageModel->setCurrentSessionId(session->id);
        }
    }
}

QString SessionController::sessionsFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dataPath).filePath("EnhanceVision/sessions.json");
}

void SessionController::ensureDataDirectory() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dirPath = QDir(dataPath).filePath("EnhanceVision");
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QJsonObject SessionController::sessionToJson(const Session& session) const
{
    QJsonObject json;
    json["id"] = session.id;
    json["name"] = session.name;
    json["createdAt"] = session.createdAt.toString(Qt::ISODate);
    json["modifiedAt"] = session.modifiedAt.toString(Qt::ISODate);
    json["isActive"] = session.isActive;
    json["isSelected"] = session.isSelected;
    json["isPinned"] = session.isPinned;
    json["sortIndex"] = session.sortIndex;
    
    QJsonArray messagesArray;
    for (const Message& msg : session.messages) {
        messagesArray.append(messageToJson(msg));
    }
    json["messages"] = messagesArray;
    
    QJsonArray pendingFilesArray;
    for (const MediaFile& file : session.pendingFiles) {
        pendingFilesArray.append(mediaFileToJson(file));
    }
    json["pendingFiles"] = pendingFilesArray;
    
    return json;
}

Session SessionController::jsonToSession(const QJsonObject& json) const
{
    Session session;
    session.id = json["id"].toString();
    session.name = json["name"].toString();
    session.createdAt = QDateTime::fromString(json["createdAt"].toString(), Qt::ISODate);
    session.modifiedAt = QDateTime::fromString(json["modifiedAt"].toString(), Qt::ISODate);
    session.isActive = json["isActive"].toBool(false);
    session.isSelected = json["isSelected"].toBool(false);
    session.isPinned = json["isPinned"].toBool(false);
    session.sortIndex = json["sortIndex"].toInt(0);
    
    QJsonArray messagesArray = json["messages"].toArray();
    for (const QJsonValue& value : messagesArray) {
        session.messages.append(jsonToMessage(value.toObject()));
    }
    
    QJsonArray pendingFilesArray = json["pendingFiles"].toArray();
    for (const QJsonValue& value : pendingFilesArray) {
        session.pendingFiles.append(jsonToMediaFile(value.toObject()));
    }
    
    return session;
}

QJsonObject SessionController::messageToJson(const Message& message) const
{
    QJsonObject json;
    json["id"] = message.id;
    json["timestamp"] = message.timestamp.toString(Qt::ISODate);
    json["mode"] = static_cast<int>(message.mode);
    json["status"] = static_cast<int>(message.status);
    json["errorMessage"] = message.errorMessage;
    json["isSelected"] = message.isSelected;
    json["progress"] = message.progress;
    json["queuePosition"] = message.queuePosition;
    
    QJsonArray filesArray;
    for (const MediaFile& file : message.mediaFiles) {
        filesArray.append(mediaFileToJson(file));
    }
    json["mediaFiles"] = filesArray;
    
    json["parameters"] = parametersToJson(message.parameters);
    json["shaderParams"] = shaderParamsToJson(message.shaderParams);
    
    // 保存 AI 参数
    QJsonObject aiParamsJson;
    aiParamsJson["modelId"] = message.aiParams.modelId;
    aiParamsJson["category"] = static_cast<int>(message.aiParams.category);
    aiParamsJson["useGpu"] = message.aiParams.useGpu;
    aiParamsJson["tileSize"] = message.aiParams.tileSize;
    aiParamsJson["modelParams"] = QJsonObject::fromVariantMap(message.aiParams.modelParams);
    json["aiParams"] = aiParamsJson;
    
    return json;
}

Message SessionController::jsonToMessage(const QJsonObject& json) const
{
    Message message;
    message.id = json["id"].toString();
    message.timestamp = QDateTime::fromString(json["timestamp"].toString(), Qt::ISODate);
    message.mode = static_cast<ProcessingMode>(json["mode"].toInt(0));
    message.status = static_cast<ProcessingStatus>(json["status"].toInt(0));
    message.errorMessage = json["errorMessage"].toString();
    message.isSelected = json["isSelected"].toBool(false);
    message.progress = json["progress"].toInt(0);
    message.queuePosition = json["queuePosition"].toInt(-1);
    
    QJsonArray filesArray = json["mediaFiles"].toArray();
    for (const QJsonValue& value : filesArray) {
        message.mediaFiles.append(jsonToMediaFile(value.toObject()));
    }
    
    message.parameters = jsonToParameters(json["parameters"].toObject());
    message.shaderParams = jsonToShaderParams(json["shaderParams"].toObject());
    
    // 恢复 AI 参数
    if (json.contains("aiParams")) {
        QJsonObject aiParamsJson = json["aiParams"].toObject();
        message.aiParams.modelId = aiParamsJson["modelId"].toString();
        message.aiParams.category = static_cast<ModelCategory>(aiParamsJson["category"].toInt(0));
        message.aiParams.useGpu = aiParamsJson["useGpu"].toBool(true);
        message.aiParams.tileSize = aiParamsJson["tileSize"].toInt(0);
        message.aiParams.modelParams = aiParamsJson["modelParams"].toObject().toVariantMap();
    }
    
    if (message.status == ProcessingStatus::Processing) {
        message.status = ProcessingStatus::Pending;
        message.progress = 0;
        message.queuePosition = -1;
    }
    
    return message;
}

QJsonObject SessionController::mediaFileToJson(const MediaFile& file) const
{
    QJsonObject json;
    json["id"] = file.id;
    json["filePath"] = file.filePath;
    json["fileName"] = file.fileName;
    json["fileSize"] = file.fileSize;
    json["type"] = static_cast<int>(file.type);
    json["duration"] = file.duration;
    json["width"] = file.resolution.width();
    json["height"] = file.resolution.height();
    json["status"] = static_cast<int>(file.status);
    json["resultPath"] = file.resultPath;
    return json;
}

MediaFile SessionController::jsonToMediaFile(const QJsonObject& json) const
{
    MediaFile file;
    file.id = json["id"].toString();
    file.filePath = json["filePath"].toString();
    file.fileName = json["fileName"].toString();
    file.fileSize = json["fileSize"].toVariant().toLongLong();
    file.type = static_cast<MediaType>(json["type"].toInt(0));
    file.duration = json["duration"].toVariant().toLongLong();
    file.resolution = QSize(json["width"].toInt(0), json["height"].toInt(0));
    file.status = static_cast<ProcessingStatus>(json["status"].toInt(0));
    file.resultPath = json["resultPath"].toString();
    
    if (file.status == ProcessingStatus::Processing) {
        file.status = ProcessingStatus::Pending;
    }
    
    return file;
}

QJsonObject SessionController::shaderParamsToJson(const ShaderParams& params) const
{
    QJsonObject json;
    json["brightness"] = params.brightness;
    json["contrast"] = params.contrast;
    json["saturation"] = params.saturation;
    json["sharpness"] = params.sharpness;
    json["blur"] = params.blur;
    json["denoise"] = params.denoise;
    json["hue"] = params.hue;
    json["exposure"] = params.exposure;
    json["gamma"] = params.gamma;
    json["temperature"] = params.temperature;
    json["tint"] = params.tint;
    json["vignette"] = params.vignette;
    json["highlights"] = params.highlights;
    json["shadows"] = params.shadows;
    return json;
}

ShaderParams SessionController::jsonToShaderParams(const QJsonObject& json) const
{
    ShaderParams params;
    params.brightness = json["brightness"].toDouble(0.0);
    params.contrast = json["contrast"].toDouble(1.0);
    params.saturation = json["saturation"].toDouble(1.0);
    params.sharpness = json["sharpness"].toDouble(0.0);
    params.blur = json["blur"].toDouble(0.0);
    params.denoise = json["denoise"].toDouble(0.0);
    params.hue = json["hue"].toDouble(0.0);
    params.exposure = json["exposure"].toDouble(0.0);
    params.gamma = json["gamma"].toDouble(1.0);
    params.temperature = json["temperature"].toDouble(0.0);
    params.tint = json["tint"].toDouble(0.0);
    params.vignette = json["vignette"].toDouble(0.0);
    params.highlights = json["highlights"].toDouble(0.0);
    params.shadows = json["shadows"].toDouble(0.0);
    return params;
}

QJsonObject SessionController::parametersToJson(const QVariantMap& params) const
{
    QJsonObject json;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        json[it.key()] = QJsonValue::fromVariant(it.value());
    }
    return json;
}

QVariantMap SessionController::jsonToParameters(const QJsonObject& json) const
{
    QVariantMap params;
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        params[it.key()] = it.value().toVariant();
    }
    return params;
}

void SessionController::restoreThumbnails()
{
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (!thumbnailProvider) {
        qWarning() << "ThumbnailProvider not available for thumbnail restoration";
        return;
    }
    
    int restoredCount = 0;
    int missingCount = 0;
    int totalCompletedFiles = 0;
    
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        Session session = m_sessionModel->sessionAt(i);
        bool sessionModified = false;
        
        for (Message& message : session.messages) {
            bool messageModified = false;
            
            for (MediaFile& file : message.mediaFiles) {
                if (file.status == ProcessingStatus::Completed && !file.resultPath.isEmpty()) {
                    totalCompletedFiles++;
                    QFileInfo resultFileInfo(file.resultPath);
                    
                    if (resultFileInfo.exists()) {
                        // 检查文件完整性
                        if (resultFileInfo.size() == 0) {
                            qWarning() << "[SessionController] Processed file is empty:" << file.resultPath;
                            file.status = ProcessingStatus::Failed;
                            missingCount++;
                            messageModified = true;
                            continue;
                        }
                        
                        QString thumbId = "processed_" + file.id;
                        QImage thumbnail;
                        
                        if (file.type == MediaType::Video) {
                            // 对于视频文件，检查文件是否完整（修改时间检查）
                            QDateTime lastModified = resultFileInfo.lastModified();
                            qint64 msSinceModified = lastModified.msecsTo(QDateTime::currentDateTime());
                            if (msSinceModified < 1000) {
                                qInfo() << "[SessionController] Video file recently modified, skipping thumbnail:" << file.resultPath;
                            } else {
                                thumbnail = ImageUtils::generateVideoThumbnail(file.resultPath, QSize(512, 512));
                            }
                        } else {
                            thumbnail = ImageUtils::generateThumbnail(file.resultPath, QSize(512, 512));
                        }
                        
                        if (!thumbnail.isNull()) {
                            thumbnailProvider->setThumbnail(thumbId, thumbnail);
                            restoredCount++;
                        }
                    } else {
                        file.status = ProcessingStatus::Failed;
                        missingCount++;
                        messageModified = true;
                        qWarning() << "[SessionController] Processed file not found:" << file.resultPath;
                    }
                }
                
                if (!file.thumbnail.isNull()) {
                    thumbnailProvider->setThumbnail(file.filePath, file.thumbnail);
                }
            }
            
            if (messageModified) {
                int completedCount = 0;
                int failedCount = 0;
                for (const MediaFile& f : message.mediaFiles) {
                    if (f.status == ProcessingStatus::Completed) completedCount++;
                    else if (f.status == ProcessingStatus::Failed || 
                             f.status == ProcessingStatus::Cancelled) failedCount++;
                }
                
                if (failedCount > 0 && completedCount == 0) {
                    message.status = ProcessingStatus::Failed;
                } else if (completedCount == message.mediaFiles.size()) {
                    message.status = ProcessingStatus::Completed;
                } else {
                    message.status = ProcessingStatus::Pending;
                }
                sessionModified = true;
            }
        }
        
        if (sessionModified) {
            m_sessionModel->updateSession(session);
            
            if (session.isActive && m_messageModel) {
                m_messageModel->setMessages(session.messages);
            }
        }
    }

    rebuildSessionMessageIndex();
    
    if (missingCount > 0) {
        qWarning() << "[SessionController] Found" << missingCount << "missing processed files";
    }
}

void SessionController::rebuildMessageRowIndexForSession(const QString& sessionId, const QList<Message>& messages)
{
    QHash<QString, int> rowIndex;
    rowIndex.reserve(messages.size());

    for (int i = 0; i < messages.size(); ++i) {
        rowIndex.insert(messages[i].id, i);
    }

    m_messageRowBySessionId.insert(sessionId, rowIndex);
}

void SessionController::rebuildSessionMessageIndex()
{
    m_messageToSessionId.clear();
    m_sessionRowById.clear();
    m_messageRowBySessionId.clear();

    const QList<Session>& sessions = m_sessionModel->sessionsRef();
    for (int i = 0; i < sessions.size(); ++i) {
        const Session& session = sessions[i];
        m_sessionRowById[session.id] = i;
        rebuildMessageRowIndexForSession(session.id, session.messages);
        for (const Message& message : session.messages) {
            m_messageToSessionId[message.id] = session.id;
        }
    }
}

void SessionController::checkAndAutoRetryAllInterruptedTasks()
{
    if (!m_processingController) {
        qWarning() << "[SessionController] checkAndAutoRetryAllInterruptedTasks: ProcessingController not set";
        return;
    }

    auto* settings = SettingsController::instance();
    if (!settings) {
        qWarning() << "[SessionController] checkAndAutoRetryAllInterruptedTasks: SettingsController not available";
        return;
    }

    QStringList interruptedMessageIds;
    QHash<QString, QString> messageToSession;
    bool hasStatusChanges = false;

    QList<Session>& sessions = m_sessionModel->sessionsRef();
    for (int sessionIdx = 0; sessionIdx < sessions.size(); ++sessionIdx) {
        Session& session = sessions[sessionIdx];
        
        for (Message& msg : session.messages) {
            if (m_autoRetriedMessageIds.contains(msg.id)) {
                continue;
            }

            bool hasInterruptedFiles = false;
            for (const MediaFile& file : msg.mediaFiles) {
                if (file.status == ProcessingStatus::Pending || file.status == ProcessingStatus::Processing) {
                    hasInterruptedFiles = true;
                    break;
                }
            }

            if (hasInterruptedFiles) {
                bool shouldAutoReprocess = false;
                
                if (msg.mode == ProcessingMode::Shader) {
                    shouldAutoReprocess = settings->autoReprocessShaderEnabled();
                } else if (msg.mode == ProcessingMode::AIInference) {
                    shouldAutoReprocess = settings->autoReprocessAIEnabled();
                }
                
                if (shouldAutoReprocess) {
                    m_autoRetriedMessageIds.insert(msg.id);
                    interruptedMessageIds.append(msg.id);
                    messageToSession[msg.id] = session.id;
                    qInfo() << "[SessionController] Found interrupted task for auto-retry:"
                            << "session:" << session.id
                            << "message:" << msg.id
                            << "mode:" << static_cast<int>(msg.mode);
                } else {
                    for (MediaFile& file : msg.mediaFiles) {
                        if (file.status == ProcessingStatus::Pending || 
                            file.status == ProcessingStatus::Processing) {
                            file.status = ProcessingStatus::Failed;
                            hasStatusChanges = true;
                        }
                    }
                    
                    int failedCount = 0;
                    for (const MediaFile& f : msg.mediaFiles) {
                        if (f.status == ProcessingStatus::Failed) failedCount++;
                    }
                    if (failedCount > 0) {
                        msg.status = ProcessingStatus::Failed;
                    }
                }
            }
        }
    }

    if (hasStatusChanges) {
        saveSessionsImmediately();
    }

    if (!interruptedMessageIds.isEmpty()) {
        qInfo() << "[SessionController] Auto-retrying" << interruptedMessageIds.size() << "interrupted tasks";
        
        QTimer::singleShot(500, this, [this, interruptedMessageIds, messageToSession]() {
            if (!m_processingController) {
                return;
            }

            for (const QString& messageId : interruptedMessageIds) {
                const QString sessionId = messageToSession.value(messageId);
                m_processingController->autoRetryInterruptedFiles(messageId, sessionId);
            }
        });
    }
}

void SessionController::saveSessionsImmediately()
{
    if (m_saveInProgress) {
        m_saveQueued = true;
        return;
    }

    if (m_saveTimer) {
        m_saveTimer->stop();
    }
    m_hasPendingSave = false;

    m_saveInProgress = true;

    ensureDataDirectory();

    QString filePath = sessionsFilePath();

    QJsonObject root;
    root["version"] = 1;
    root["lastActiveSessionId"] = m_sessionModel->activeSessionId();
    root["sessionCounter"] = m_sessionCounter;

    QJsonArray sessionsArray;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        Session session = m_sessionModel->sessionAt(i);
        sessionsArray.append(sessionToJson(session));
    }
    root["sessions"] = sessionsArray;

    const QString tempPath = filePath + ".tmp";

    QFile file(tempPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        QFile oldFile(filePath);
        if (oldFile.exists()) {
            oldFile.remove();
        }

        if (!file.rename(filePath)) {
            qWarning() << "[SessionController] saveSessionsImmediately: Failed to rename temp file";
        }
    } else {
        qWarning() << "[SessionController] saveSessionsImmediately: Failed to open temp file";
    }

    m_saveInProgress = false;

    if (m_saveQueued) {
        m_saveQueued = false;
        QTimer::singleShot(100, this, &SessionController::saveSessions);
    }
}

} // namespace EnhanceVision
