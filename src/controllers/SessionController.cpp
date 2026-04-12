/**
 * @file SessionController.cpp
 * @brief 会话控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/TaskRecoveryController.h"
#include "EnhanceVision/controllers/SessionPruneUtils.h"
#include "EnhanceVision/models/MessageModel.h"
#include "EnhanceVision/core/TaskCoordinator.h"
#include "EnhanceVision/core/TaskStateManager.h"
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

namespace {

struct PruneTarget {
    QString sessionId;
    QString messageId;
    QString fileId;
    MediaFile file;
    bool isActiveSession = false;
};

int countTasksForTargets(const QList<QueueTask>& tasks, const QList<PruneTarget>& targets)
{
    QSet<QString> targetKeys;
    for (const PruneTarget& target : targets) {
        targetKeys.insert(target.messageId + QLatin1Char(':') + target.fileId);
    }

    int cancelledTaskCount = 0;
    for (const QueueTask& task : tasks) {
        if (targetKeys.contains(task.messageId + QLatin1Char(':') + task.fileId)) {
            ++cancelledTaskCount;
        }
    }
    return cancelledTaskCount;
}

} // namespace

SessionController::SessionController(QObject* parent)
    : QObject(parent)
    , m_sessionModel(new SessionModel(this))
    , m_messageModel(nullptr)
    , m_processingTimeManager(new ProcessingTimeManager(this))
    , m_batchSelectionMode(false)
    , m_sessionCounter(0)
    , m_autoSaveEnabled(true)
    , m_lastAutoSaveMs(0)
    , m_saveTimer(new QTimer(this))
    , m_sessionNotifyTimer(new QTimer(this))
{
    connect(m_sessionModel, &SessionModel::errorOccurred, this, &SessionController::errorOccurred);
    connect(m_sessionModel, &SessionModel::sessionsReordered, this, &SessionController::rebuildSessionMessageIndex);

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
        disconnect(m_messageModel, &MessageModel::actualTotalSecUpdated, this, nullptr);
        disconnect(m_messageModel, &MessageModel::tipDismissStateUpdated, this, nullptr);
    }
    
    m_messageModel = model;
    
    if (m_messageModel) {
        connect(m_messageModel, &MessageModel::countChanged, this, &SessionController::onMessageCountChanged);
        // 当实际总耗时更新时，同步到会话并保存
        connect(m_messageModel, &MessageModel::actualTotalSecUpdated, this, [this](const QString &messageId, qint64 totalSec) {
            Q_UNUSED(messageId)
            Q_UNUSED(totalSec)
            syncCurrentMessagesToSession();
            saveSessions();
        });
        connect(m_messageModel, &MessageModel::tipDismissStateUpdated, this, [this](const QString &messageId, bool failedDismissed, bool errorDismissed) {
            Q_UNUSED(messageId)
            Q_UNUSED(failedDismissed)
            Q_UNUSED(errorDismissed)
            syncCurrentMessagesToSession();
            saveSessionsImmediately();
        });
        // 设置 ProcessingTimeManager 的 MessageModel 引用
        if (m_processingTimeManager) {
            m_processingTimeManager->setMessageModel(m_messageModel);
        }
    }
}

void SessionController::setProcessingController(ProcessingController* controller)
{
    m_processingController = controller;
}

void SessionController::setTaskRecoveryController(TaskRecoveryController* controller)
{
    m_taskRecoveryController = controller;
}

void SessionController::onMessageCountChanged()
{
    if (!m_messageModel) return;
    
    QString currentId = m_messageModel->currentSessionId();
    if (currentId.isEmpty()) return;
    
    // 【修复】校验 sessionId 一致性：只有当 MessageModel 的 sessionId 
    // 与当前活动会话一致时才同步，防止跨会话数据污染
    const QString activeId = m_sessionModel->activeSessionId();
    if (currentId != activeId) {
        qWarning() << "[SessionController] onMessageCountChanged: sessionId mismatch"
                   << "messageModel:" << currentId << "active:" << activeId
                   << "- skipping sync to prevent cross-session contamination";
        return;
    }
    
    Session* session = getSession(currentId);
    if (session) {
        m_messageModel->syncToSession(*session);
        rebuildMessageRowIndexForSession(currentId, session->messages);
        
        // 【修复】先收集当前消息ID集合，清理已删除消息的残留索引
        QSet<QString> currentMessageIds;
        for (const Message& message : session->messages) {
            m_messageToSessionId[message.id] = currentId;
            currentMessageIds.insert(message.id);
        }
        
        // 清理 m_messageToSessionId 中属于该会话但已不存在的消息
        auto it = m_messageToSessionId.begin();
        while (it != m_messageToSessionId.end()) {
            if (it.value() == currentId && !currentMessageIds.contains(it.key())) {
                it = m_messageToSessionId.erase(it);
            } else {
                ++it;
            }
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

    // 2. 切换会话（会触发 activeSessionChanged 信号）
    //    信号处理器会自动执行：保存旧会话消息 + 加载新会话消息
    //    【修复】不再在这里手动调用 loadSessionMessages()，避免双重加载导致消息混乱
    m_sessionModel->switchSession(sessionId);
    // m_activeSessionId 由信号处理器更新

    emit sessionSwitched(sessionId);
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
    
    Session* session = getSession(sessionId);
    if (session) {
        deleteSessionMediaFiles(*session);
    }
    
    // 【修复】deleteSession 内部的 switchSession 会触发 activeSessionChanged 信号，
    // 信号处理器会自动保存旧会话消息 + 加载新会话消息，
    // 不再需要手动调用 loadSessionMessages，避免双重加载导致消息混乱
    m_sessionModel->deleteSession(sessionId);
    rebuildSessionMessageIndex();
    
    if (isActiveSession) {
        QString newActiveId = m_sessionModel->activeSessionId();
        
        if (newActiveId.isEmpty()) {
            // 没有活动会话了，确保清空（信号处理器也会处理，这里做防御性清理）
            if (m_messageModel) {
                m_messageModel->clear();
                m_messageModel->setCurrentSessionId("");
            }
            m_activeSessionId.clear();
        }
        // 有新活动会话时，由 activeSessionChanged 信号处理器自动加载
    }
    
    emit sessionDeleted(sessionId);
    emit sessionCountChanged();
}

void SessionController::clearSession(const QString& sessionId)
{
    if (m_processingController) {
        m_processingController->cancelSessionTasks(sessionId);
    }
    
    Session* session = getSession(sessionId);
    if (session) {
        deleteSessionMediaFiles(*session);
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
    
    for (const QString& sessionId : selectedIds) {
        Session* session = getSession(sessionId);
        if (session) {
            deleteSessionMediaFiles(*session);
        }
    }
    
    // 【修复】deleteSelectedSessions 内部会调用 switchSession 触发 activeSessionChanged 信号，
    // 信号处理器会自动加载新会话消息，不再需要手动调用 loadSessionMessages
    int count = m_sessionModel->deleteSelectedSessions();
    if (count > 0) {
        rebuildSessionMessageIndex();
        emit sessionCountChanged();
        emit selectionChanged();
        
        if (activeSessionDeleted) {
            QString newActiveId = m_sessionModel->activeSessionId();
            if (newActiveId.isEmpty()) {
                // 没有活动会话了，确保清空
                if (m_messageModel) {
                    m_messageModel->clear();
                    m_messageModel->setCurrentSessionId("");
                }
                m_activeSessionId.clear();
            }
            // 有新活动会话时，由 activeSessionChanged 信号处理器自动加载
            // 不再手动调用 loadSessionMessages 和 emit activeSessionChanged
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
    
    for (const QString& sessionId : selectedIds) {
        Session* session = getSession(sessionId);
        if (session) {
            deleteSessionMediaFiles(*session);
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
    return tr("Unnamed Session %1").arg(m_sessionCounter);
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
            errorMessage = tr("Cannot save session data");
        } else {
            QJsonDocument doc(root);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();

            QFile oldFile(filePath);
            if (oldFile.exists()) {
                oldFile.remove();
            }

            if (!file.rename(filePath)) {
                errorMessage = tr("Cannot save session data");
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
    
    // 【修复】批量加载会话并按 sortIndex 排序，保留用户拖拽排序结果
    QList<Session> allSessions;
    allSessions.reserve(sessionsArray.size());
    for (const QJsonValue& value : sessionsArray) {
        Session session = jsonToSession(value.toObject());
        allSessions.append(session);
    }
    
    // 按 sortIndex 排序（置顶会话在前，普通会话在后，同组内按 sortIndex 升序）
    std::sort(allSessions.begin(), allSessions.end(),
              [](const Session& a, const Session& b) {
                  if (a.isPinned != b.isPinned) {
                      return a.isPinned > b.isPinned; // 置顶在前
                  }
                  return a.sortIndex < b.sortIndex;
              });
    
    // 批量加载到 SessionModel（不经过 addSession 避免重新计算插入位置）
    // 注意：已按 sortIndex 排序，不需要再调用 updateSortIndices
    if (!allSessions.isEmpty()) {
        m_sessionModel->updateSessions(allSessions);
    }

    rebuildSessionMessageIndex();
    
    QString lastActiveId = root["lastActiveSessionId"].toString();
    if (!lastActiveId.isEmpty()) {
        for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
            Session session = m_sessionModel->sessionAt(i);
            if (session.id == lastActiveId) {
                // 不要在这里设置 m_activeSessionId，让 activeSessionChanged 信号处理器来处理
                // 这样可以确保消息被正确加载
                m_sessionModel->switchSession(lastActiveId);
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
        m_messageModel->setMessages(session->messages);
        m_messageModel->setCurrentSessionId(session->id);
    }
}

void SessionController::reloadActiveSessionMessages()
{
    if (m_activeSessionId.isEmpty()) {
        if (m_messageModel) {
            m_messageModel->clear();
            m_messageModel->setCurrentSessionId(QString());
        }
        return;
    }

    loadSessionMessages(m_activeSessionId);
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
    
    // 保存实际总耗时
    json["actualTotalSec"] = message.actualTotalSec;
    // 保存处理开始时间（用于会话切换后恢复进度）
    json["processingStartTime"] = message.processingStartTime;
    
    // 保存时间预测系统字段（用于应用关闭后恢复）
    json["predictedTotalSec"] = message.predictedTotalSec;
    json["elapsedSec"] = message.elapsedSec;
    json["remainingSec"] = message.remainingSec;
    json["isOvertime"] = message.isOvertime;
    json["failedTipDismissed"] = message.failedTipDismissed;
    json["errorTipDismissed"] = message.errorTipDismissed;
    
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
        message.aiParams.useGpu = aiParamsJson["useGpu"].toBool(false);
        message.aiParams.tileSize = aiParamsJson["tileSize"].toInt(0);
        message.aiParams.modelParams = aiParamsJson["modelParams"].toObject().toVariantMap();
    }
    
    // 恢复实际总耗时
    message.actualTotalSec = json["actualTotalSec"].toVariant().toLongLong();
    // 恢复处理开始时间（用于会话切换后恢复进度）
    message.processingStartTime = json["processingStartTime"].toVariant().toLongLong();
    
    // 恢复时间预测系统字段
    message.predictedTotalSec = json["predictedTotalSec"].toVariant().toLongLong();
    message.elapsedSec = json["elapsedSec"].toVariant().toLongLong();
    message.remainingSec = json["remainingSec"].toVariant().toLongLong();
    message.isOvertime = json["isOvertime"].toBool(false);
    message.failedTipDismissed = json["failedTipDismissed"].toBool(false);
    message.errorTipDismissed = json["errorTipDismissed"].toBool(false);
    
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
        qWarning() << "[SessionController] ThumbnailProvider not available for thumbnail restoration";
        return;
    }
    
    int missingCount = 0;
    
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        Session session = m_sessionModel->sessionAt(i);
        bool sessionModified = false;
        
        for (Message& message : session.messages) {
            bool messageModified = false;
            
            for (MediaFile& file : message.mediaFiles) {
                if (file.status == ProcessingStatus::Completed && !file.resultPath.isEmpty()) {
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
                                // Recently modified files may still be locked; skip thumbnail restoration for now.
                            } else {
                                thumbnail = ImageUtils::generateVideoThumbnail(file.resultPath, QSize(512, 512));
                            }
                        } else {
                            thumbnail = ImageUtils::generateThumbnail(file.resultPath, QSize(512, 512));
                        }
                        
                        if (!thumbnail.isNull()) {
                            thumbnailProvider->setThumbnail(thumbId, thumbnail, file.resultPath);
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

bool SessionController::prepareForShutdown(int waitMs)
{
    m_autoSaveEnabled = false;
    if (m_saveTimer) {
        m_saveTimer->stop();
    }
    if (m_sessionNotifyTimer) {
        m_sessionNotifyTimer->stop();
    }
    m_hasPendingSave = false;

    if (m_saveInProgress) {
        QElapsedTimer timer;
        timer.start();
        while (m_saveInProgress && timer.elapsed() < waitMs) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            QThread::msleep(10);
        }

        if (m_saveInProgress) {
            qWarning() << "[SessionController] Timed out waiting for async save before shutdown";
            return false;
        }
    }

    saveSessionsImmediately();
    return true;
}

QString SessionController::findTaskIdForFile(const QString& messageId, const QString& fileId) const
{
    if (!m_processingController) {
        return QString();
    }
    
    QList<QueueTask> tasks = m_processingController->getAllTasks();
    for (const QueueTask& task : tasks) {
        if (task.messageId == messageId && task.fileId == fileId) {
            return task.taskId;
        }
    }
    
    return QString();
}

void SessionController::clearAllSessionMessages()
{
    clearAllSessionMessagesWithStats(QStringLiteral("clear-all"));
}

QVariantMap SessionController::clearAllSessionMessagesWithStats(const QString& reason)
{
    QVariantMap summary;
    int removedMessageCount = 0;
    int removedFileCount = 0;

    QList<QueueTask> tasksSnapshot;
    if (m_processingController) {
        tasksSnapshot = m_processingController->getAllTasks();
    }

    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        const Session session = m_sessionModel->sessionAt(i);
        removedMessageCount += session.messages.size();
        for (const Message& message : session.messages) {
            removedFileCount += message.mediaFiles.size();
        }
    }

    if (m_processingController) {
        for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
            const Session session = m_sessionModel->sessionAt(i);
            m_processingController->cancelSessionTasks(session.id);
        }
    }

    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        const Session session = m_sessionModel->sessionAt(i);
        m_sessionModel->clearSession(session.id);
    }

    if (m_messageModel) {
        m_messageModel->clear();
    }

    rebuildSessionMessageIndex();
    saveSessionsImmediately();

    if (m_taskRecoveryController) {
        m_taskRecoveryController->syncPendingRecoveryFromSessions();
        m_taskRecoveryController->persistSnapshotNow();
    }

    emit allSessionMessagesCleared();

    summary[QStringLiteral("reason")] = reason;
    summary[QStringLiteral("removedFiles")] = removedFileCount;
    summary[QStringLiteral("removedMessages")] = removedMessageCount;
    summary[QStringLiteral("cancelledTasks")] = tasksSnapshot.size();
    summary[QStringLiteral("sessionsAffected")] = m_sessionModel->rowCount();
    summary[QStringLiteral("success")] = true;

    return summary;
}

void SessionController::clearAllSessionMessagesByMode(int mode)
{
    pruneMediaFilesByModeAndType(mode, static_cast<int>(MediaType::Image), QStringLiteral("clear-mode"));
    pruneMediaFilesByModeAndType(mode, static_cast<int>(MediaType::Video), QStringLiteral("clear-mode"));
}

void SessionController::clearAllShaderVideoMessages()
{
    pruneMediaFilesByModeAndType(static_cast<int>(ProcessingMode::Shader),
                                 static_cast<int>(MediaType::Video),
                                 QStringLiteral("clear-shader-video"));
}

void SessionController::clearMediaFilesByModeAndType(int mode, int mediaType)
{
    pruneMediaFilesByModeAndType(mode, mediaType, QStringLiteral("legacy-clear"));
}

QVariantMap SessionController::pruneMediaFilesByModeAndType(int mode, int mediaType, const QString& reason)
{
    const ProcessingMode targetMode = static_cast<ProcessingMode>(mode);
    const MediaType targetMediaType = static_cast<MediaType>(mediaType);

    QList<PruneTarget> targets;
    QHash<QString, int> targetedCountByMessage;
    QHash<QString, int> totalFileCountByMessage;
    const QString activeId = m_sessionModel->activeSessionId();

    QList<Session>& sessions = m_sessionModel->sessionsRef();
    for (Session& session : sessions) {
        for (const Message& message : session.messages) {
            if (message.mode != targetMode) {
                continue;
            }

            const QString messageKey = session.id + QLatin1Char(':') + message.id;
            totalFileCountByMessage[messageKey] = message.mediaFiles.size();

            for (const MediaFile& file : message.mediaFiles) {
                if (file.type != targetMediaType) {
                    continue;
                }

                PruneTarget target;
                target.sessionId = session.id;
                target.messageId = message.id;
                target.fileId = file.id;
                target.file = file;
                target.isActiveSession = (session.id == activeId);
                targets.append(target);
                targetedCountByMessage[messageKey] += 1;
            }
        }
    }

    QVariantMap summary;
    summary[QStringLiteral("reason")] = reason;
    summary[QStringLiteral("removedFiles")] = targets.size();
    summary[QStringLiteral("removedMessages")] = 0;
    summary[QStringLiteral("cancelledTasks")] = 0;
    summary[QStringLiteral("sessionsAffected")] = 0;
    summary[QStringLiteral("success")] = true;

    if (targets.isEmpty()) {
        return summary;
    }

    int removedMessagesCount = 0;
    QSet<QString> affectedSessionIds;
    for (auto it = targetedCountByMessage.constBegin(); it != targetedCountByMessage.constEnd(); ++it) {
        if (it.value() >= totalFileCountByMessage.value(it.key())) {
            ++removedMessagesCount;
        }
        affectedSessionIds.insert(it.key().section(QLatin1Char(':'), 0, 0));
    }
    summary[QStringLiteral("removedMessages")] = removedMessagesCount;
    summary[QStringLiteral("sessionsAffected")] = affectedSessionIds.size();

    QList<QueueTask> taskSnapshot;
    if (m_processingController) {
        taskSnapshot = m_processingController->getAllTasks();
        summary[QStringLiteral("cancelledTasks")] = countTasksForTargets(taskSnapshot, targets);
    }

    QHash<QString, QSet<QString>> nonActiveFilesByMessageKey;

    for (const PruneTarget& target : targets) {
        if (target.isActiveSession && m_messageModel) {
            m_messageModel->removeMediaFileById(target.messageId, target.fileId);
            continue;
        }

        if (m_processingController) {
            m_processingController->cancelMessageFileTasks(target.messageId, target.fileId);
        }

        ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
        if (!target.file.resultPath.isEmpty()) {
            QFile resultFile(target.file.resultPath);
            if (resultFile.exists()) {
                resultFile.remove();
            }
            if (thumbnailProvider) {
                thumbnailProvider->removeThumbnail(QStringLiteral("processed_") + target.fileId);
            }
        }
        if (thumbnailProvider && !target.file.filePath.isEmpty()) {
            thumbnailProvider->removeThumbnail(target.file.filePath);
        }
        nonActiveFilesByMessageKey[target.sessionId + QLatin1Char(':') + target.messageId].insert(target.fileId);
    }

    for (Session& session : sessions) {
        if (session.id == activeId) {
            continue;
        }

        bool sessionModified = false;
        for (int messageIndex = session.messages.size() - 1; messageIndex >= 0; --messageIndex) {
            Message& message = session.messages[messageIndex];
            const QString messageKey = session.id + QLatin1Char(':') + message.id;
            if (!nonActiveFilesByMessageKey.contains(messageKey)) {
                continue;
            }

            const MessagePruneResult pruneResult = pruneMediaFilesFromMessage(
                message,
                nonActiveFilesByMessageKey.value(messageKey));
            if (pruneResult.removedFiles == 0) {
                continue;
            }

            sessionModified = true;
            if (pruneResult.removedMessage) {
                session.messages.removeAt(messageIndex);
                continue;
            }

            session.messages[messageIndex] = message;
        }

        if (sessionModified) {
            session.modifiedAt = QDateTime::currentDateTime();
        }
    }

    if (activeId == m_activeSessionId && m_messageModel) {
        syncCurrentMessagesToSession();
    }

    rebuildSessionMessageIndex();

    for (const QString& sessionId : affectedSessionIds) {
        m_sessionModel->notifySessionDataChanged(sessionId);
    }

    if (!activeId.isEmpty()) {
        reloadActiveSessionMessages();
    }

    if (m_taskRecoveryController) {
        m_taskRecoveryController->syncPendingRecoveryFromSessions();
    }

    saveSessionsImmediately();

    return summary;
}

void SessionController::deleteAllSessions()
{
    if (m_processingController) {
        for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
            Session session = m_sessionModel->sessionAt(i);
            m_processingController->cancelSessionTasks(session.id);
        }
    }
    
    QList<QString> sessionIds;
    for (int i = 0; i < m_sessionModel->rowCount(); ++i) {
        sessionIds.append(m_sessionModel->sessionAt(i).id);
    }
    
    for (const QString& sessionId : sessionIds) {
        Session* session = getSession(sessionId);
        if (session) {
            deleteSessionMediaFiles(*session);
        }
    }
    
    for (const QString& sessionId : sessionIds) {
        m_sessionModel->deleteSession(sessionId);
    }
    
    if (m_messageModel) {
        m_messageModel->clear();
        m_messageModel->setCurrentSessionId("");
    }
    
    m_activeSessionId.clear();
    rebuildSessionMessageIndex();
    saveSessions();
    
    emit allSessionsDeleted();
    emit sessionCountChanged();
    emit activeSessionChanged();
}

int SessionController::deleteSessionMediaFiles(const Session& session)
{
    int deletedCount = 0;
    
    // 获取 ThumbnailProvider 实例用于清理缩略图缓存
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    
    for (const Message& msg : session.messages) {
        for (const MediaFile& file : msg.mediaFiles) {
            // 清理结果文件
            if (!file.resultPath.isEmpty()) {
                QFile resultFile(file.resultPath);
                if (resultFile.exists()) {
                    if (resultFile.remove()) {
                        ++deletedCount;
                    } else {
                        qWarning() << "[SessionController] Failed to delete result file:" << file.resultPath;
                    }
                }
                
                // 清理处理后的缩略图缓存
                if (thumbnailProvider) {
                    QString processedThumbnailId = "processed_" + file.id;
                    thumbnailProvider->removeThumbnail(processedThumbnailId);
                }
            }
            
            // 清理原始文件的缩略图缓存
            if (thumbnailProvider && !file.filePath.isEmpty()) {
                thumbnailProvider->removeThumbnail(file.filePath);
            }
        }
    }

    return deletedCount;
}

} // namespace EnhanceVision
