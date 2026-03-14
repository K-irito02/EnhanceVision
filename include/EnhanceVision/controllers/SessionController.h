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
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/models/SessionModel.h"

namespace EnhanceVision {

class SessionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString activeSessionId READ activeSessionId NOTIFY activeSessionChanged)
    Q_PROPERTY(SessionModel* sessionModel READ sessionModel CONSTANT)
    Q_PROPERTY(int sessionCount READ sessionCount NOTIFY sessionCountChanged)
    Q_PROPERTY(bool batchSelectionMode READ batchSelectionMode WRITE setBatchSelectionMode NOTIFY batchSelectionModeChanged)

public:
    explicit SessionController(QObject* parent = nullptr);
    ~SessionController() override;

    QString activeSessionId() const;
    SessionModel* sessionModel() const;
    int sessionCount() const;
    bool batchSelectionMode() const;
    void setBatchSelectionMode(bool mode);

    Q_INVOKABLE QString createSession(const QString& name = QString());
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
    
    Q_INVOKABLE QString ensureActiveSession();

signals:
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
    bool m_batchSelectionMode;
    int m_sessionCounter;

    QString generateSessionId();
    QString generateDefaultSessionName();
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SESSIONCONTROLLER_H
