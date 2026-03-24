/**
 * @file SessionController.h
 * @brief 会话控制器 - 管理会话创建、切换、重命名、删除等
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_SESSIONCONTROLLER_H
#define ENHANCEVISION_SESSIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QFuture>
#include <QHash>
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/models/SessionModel.h"

class QTimer;

namespace EnhanceVision {

class MessageModel;
class ProcessingController;

class SessionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeSessionId READ activeSessionId NOTIFY activeSessionChanged)
    Q_PROPERTY(SessionModel* sessionModel READ sessionModel CONSTANT)
    Q_PROPERTY(int sessionCount READ sessionCount NOTIFY sessionCountChanged)
    Q_PROPERTY(bool batchSelectionMode READ batchSelectionMode WRITE setBatchSelectionMode NOTIFY batchSelectionModeChanged)
    Q_PROPERTY(bool autoSaveEnabled READ autoSaveEnabled WRITE setAutoSaveEnabled NOTIFY autoSaveEnabledChanged)

public:
    explicit SessionController(QObject* parent = nullptr);
    ~SessionController() override;

    QString activeSessionId() const;
    SessionModel* sessionModel() const;
    int sessionCount() const;
    bool batchSelectionMode() const;
    void setBatchSelectionMode(bool mode);

    /**
     * @brief 设置消息模型引用
     * @param model 消息模型指针
     */
    void setMessageModel(MessageModel* model);
    
    void setProcessingController(ProcessingController* controller);

    /**
     * @brief 创建新会话（不自动选中）
     * @param name 会话名称（可选）
     * @return 新会话ID
     */
    Q_INVOKABLE QString createSession(const QString& name = QString());

    /**
     * @brief 创建新会话并锁定选中
     * @param name 会话名称（可选）
     * @return 新会话ID
     */
    Q_INVOKABLE QString createAndSelectSession(const QString& name = QString());

    Q_INVOKABLE void switchSession(const QString& sessionId);
    Q_INVOKABLE void renameSession(const QString& sessionId, const QString& newName);
    Q_INVOKABLE void deleteSession(const QString& sessionId);
    Q_INVOKABLE void clearSession(const QString& sessionId);
    Q_INVOKABLE void selectSession(const QString& sessionId, bool selected);
    Q_INVOKABLE void selectAllSessions();
    Q_INVOKABLE void deselectAllSessions();
    Q_INVOKABLE void deleteSelectedSessions();
    Q_INVOKABLE void clearSelectedSessions();
    Q_INVOKABLE void pinSession(const QString& sessionId, bool pinned);
    Q_INVOKABLE void moveSession(int fromIndex, int toIndex);
    Q_INVOKABLE bool isSessionPinned(const QString& sessionId) const;
    Q_INVOKABLE int selectedCount() const;
    Q_INVOKABLE QString getSessionName(const QString& sessionId) const;
    Q_INVOKABLE Session* getSession(const QString& sessionId);
    const Session* getSessionConst(const QString& sessionId) const;
    Message messageInSession(const QString& sessionId, const QString& messageId) const;
    QString sessionIdForMessage(const QString& messageId) const;
    bool updateMessageInSession(const QString& sessionId, const Message& message);
    void rebuildSessionMessageIndex();
    
    /**
     * @brief 确保有活动会话（用于发送消息时）
     * 如果已有活动会话则复用，否则创建新会话并锁定选中
     * @return 活动会话ID
     */
    Q_INVOKABLE QString ensureActiveSession();
    
    /**
     * @brief 同步当前消息模型到活动会话
     * 用于在消息添加后同步到 Session
     */
    void syncCurrentMessagesToSession();
    
    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);
    
    Q_INVOKABLE void saveSessions();
    Q_INVOKABLE void loadSessions();
    
    /**
     * @brief 恢复已处理文件的缩略图
     * 在加载会话后调用，为已完成的文件从 resultPath 重新生成缩略图
     */
    void restoreThumbnails();

signals:
    void autoSaveEnabledChanged();
    void activeSessionChanged();
    void sessionCountChanged();
    void batchSelectionModeChanged();
    void selectionChanged();
    void sessionCreated(const QString& sessionId);
    void sessionSwitched(const QString& sessionId);
    void sessionRenamed(const QString& sessionId, const QString& newName);
    void sessionDeleted(const QString& sessionId);
    void sessionCleared(const QString& sessionId);
    void sessionPinned(const QString& sessionId, bool pinned);
    void sessionMoved(int fromIndex, int toIndex);
    void errorOccurred(const QString& message);

private:
    QString m_activeSessionId;
    SessionModel* m_sessionModel;
    MessageModel* m_messageModel;
    ProcessingController* m_processingController = nullptr;
    bool m_batchSelectionMode;
    int m_sessionCounter;
    bool m_autoSaveEnabled;
    qint64 m_lastAutoSaveMs;
    QSet<QString> m_autoRetriedMessageIds;
    QTimer* m_saveTimer = nullptr;
    QFuture<void> m_saveFuture;
    QTimer* m_sessionNotifyTimer = nullptr;
    bool m_hasPendingSave = false;
    bool m_saveInProgress = false;
    bool m_saveQueued = false;
    QSet<QString> m_pendingNotifySessionIds;
    QHash<QString, QString> m_messageToSessionId;
    QHash<QString, int> m_sessionRowById;
    QHash<QString, QHash<QString, int>> m_messageRowBySessionId;

    void rebuildMessageRowIndexForSession(const QString& sessionId, const QList<Message>& messages);

    QString generateSessionId();
    QString generateDefaultSessionName();
    
    void saveCurrentSessionMessages();
    
    void loadSessionMessages(const QString& sessionId);
    
    void onMessageCountChanged();
    
    QString sessionsFilePath() const;
    void ensureDataDirectory() const;
    
    QJsonObject sessionToJson(const Session& session) const;
    Session jsonToSession(const QJsonObject& json) const;
    QJsonObject messageToJson(const Message& message) const;
    Message jsonToMessage(const QJsonObject& json) const;
    QJsonObject mediaFileToJson(const MediaFile& file) const;
    MediaFile jsonToMediaFile(const QJsonObject& json) const;
    QJsonObject shaderParamsToJson(const ShaderParams& params) const;
    ShaderParams jsonToShaderParams(const QJsonObject& json) const;
    QJsonObject parametersToJson(const QVariantMap& params) const;
    QVariantMap jsonToParameters(const QJsonObject& json) const;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SESSIONCONTROLLER_H
