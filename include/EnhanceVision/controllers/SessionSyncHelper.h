#ifndef ENHANCEVISION_SESSIONSYNCHELPER_H
#define ENHANCEVISION_SESSIONSYNCHELPER_H

#include <QObject>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <functional>

namespace EnhanceVision {

class SessionController;
class MessageModel;

class SessionSyncHelper : public QObject
{
    Q_OBJECT

public:
    explicit SessionSyncHelper(QObject* parent = nullptr);
    ~SessionSyncHelper() override;

    void setSessionController(SessionController* controller);
    void setMessageModel(MessageModel* model);

    void requestSessionSync();
    void requestSessionMemorySync(const QString& messageId = QString());
    void flushSessionMemorySync();
    
    void syncVisibleMessageIfNeeded(const QString& sessionId,
                                    const QString& messageId,
                                    const std::function<void()>& syncFn);

private:
    SessionController* m_sessionController = nullptr;
    MessageModel* m_messageModel = nullptr;
    
    QTimer* m_sessionSyncTimer = nullptr;
    QTimer* m_memorySyncTimer = nullptr;
    
    QSet<QString> m_pendingMemorySyncMessageIds;
    QHash<QString, qint64> m_lastSessionMemorySyncMs;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SESSIONSYNCHELPER_H
