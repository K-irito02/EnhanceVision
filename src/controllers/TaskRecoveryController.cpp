/**
 * @file TaskRecoveryController.cpp
 * @brief 启动恢复控制器实现
 */
#include "EnhanceVision/controllers/TaskRecoveryController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/models/SessionModel.h"
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>

namespace EnhanceVision {

namespace {

constexpr int kRecoverySnapshotVersion = 1;

QString runIntentToString(RecoveryRunIntent intent)
{
    switch (intent) {
    case RecoveryRunIntent::Active:
        return QStringLiteral("active");
    case RecoveryRunIntent::Queued:
        return QStringLiteral("queued");
    case RecoveryRunIntent::Paused:
        return QStringLiteral("paused");
    }
    return QStringLiteral("queued");
}

RecoveryRunIntent runIntentFromString(const QString& value)
{
    if (value == QStringLiteral("active")) {
        return RecoveryRunIntent::Active;
    }
    if (value == QStringLiteral("paused")) {
        return RecoveryRunIntent::Paused;
    }
    return RecoveryRunIntent::Queued;
}

QJsonObject toJson(const RecoverableFileState& fileState)
{
    QJsonObject json;
    json[QStringLiteral("fileId")] = fileState.fileId;
    json[QStringLiteral("preShutdownStatus")] = static_cast<int>(fileState.preShutdownStatus);
    json[QStringLiteral("orderIndex")] = fileState.orderIndex;
    return json;
}

RecoverableFileState fileStateFromJson(const QJsonObject& json)
{
    RecoverableFileState fileState;
    fileState.fileId = json[QStringLiteral("fileId")].toString();
    fileState.preShutdownStatus = static_cast<ProcessingStatus>(json[QStringLiteral("preShutdownStatus")].toInt(static_cast<int>(ProcessingStatus::Pending)));
    fileState.orderIndex = json[QStringLiteral("orderIndex")].toInt(-1);
    return fileState;
}

QJsonObject toJson(const RecoverableMessageState& messageState)
{
    QJsonObject json;
    json[QStringLiteral("sessionId")] = messageState.sessionId;
    json[QStringLiteral("sessionName")] = messageState.sessionName;
    json[QStringLiteral("messageId")] = messageState.messageId;
    json[QStringLiteral("preShutdownStatus")] = static_cast<int>(messageState.preShutdownStatus);
    json[QStringLiteral("runIntent")] = runIntentToString(messageState.runIntent);
    json[QStringLiteral("orderIndex")] = messageState.orderIndex;

    QJsonArray filesArray;
    for (const RecoverableFileState& fileState : messageState.fileStates) {
        filesArray.append(toJson(fileState));
    }
    json[QStringLiteral("fileStates")] = filesArray;
    return json;
}

RecoverableMessageState messageStateFromJson(const QJsonObject& json)
{
    RecoverableMessageState messageState;
    messageState.sessionId = json[QStringLiteral("sessionId")].toString();
    messageState.sessionName = json[QStringLiteral("sessionName")].toString();
    messageState.messageId = json[QStringLiteral("messageId")].toString();
    messageState.preShutdownStatus = static_cast<ProcessingStatus>(json[QStringLiteral("preShutdownStatus")].toInt(static_cast<int>(ProcessingStatus::Pending)));
    messageState.runIntent = runIntentFromString(json[QStringLiteral("runIntent")].toString());
    messageState.orderIndex = json[QStringLiteral("orderIndex")].toInt(-1);

    const QJsonArray filesArray = json[QStringLiteral("fileStates")].toArray();
    for (const QJsonValue& value : filesArray) {
        messageState.fileStates.append(fileStateFromJson(value.toObject()));
    }
    return messageState;
}

QJsonObject toJson(const RecoverySnapshot& snapshot)
{
    QJsonObject json;
    json[QStringLiteral("snapshotVersion")] = snapshot.snapshotVersion;
    json[QStringLiteral("shutdownReason")] = snapshot.shutdownReason;
    json[QStringLiteral("pauseModeAtShutdown")] = snapshot.pauseModeAtShutdown;
    json[QStringLiteral("currentProcessingMessageId")] = snapshot.currentProcessingMessageId;
    json[QStringLiteral("queueBlockerMessageId")] = snapshot.queueBlockerMessageId;
    json[QStringLiteral("capturedAtMs")] = QString::number(snapshot.capturedAtMs);

    QJsonArray messagesArray;
    for (const RecoverableMessageState& messageState : snapshot.messages) {
        messagesArray.append(toJson(messageState));
    }
    json[QStringLiteral("messages")] = messagesArray;
    return json;
}

RecoverySnapshot snapshotFromJson(const QJsonObject& json)
{
    RecoverySnapshot snapshot;
    snapshot.snapshotVersion = json[QStringLiteral("snapshotVersion")].toInt(0);
    snapshot.shutdownReason = json[QStringLiteral("shutdownReason")].toString();
    snapshot.pauseModeAtShutdown = json[QStringLiteral("pauseModeAtShutdown")].toInt(1);
    snapshot.currentProcessingMessageId = json[QStringLiteral("currentProcessingMessageId")].toString();
    snapshot.queueBlockerMessageId = json[QStringLiteral("queueBlockerMessageId")].toString();
    snapshot.capturedAtMs = json[QStringLiteral("capturedAtMs")].toVariant().toLongLong();

    const QJsonArray messagesArray = json[QStringLiteral("messages")].toArray();
    for (const QJsonValue& value : messagesArray) {
        snapshot.messages.append(messageStateFromJson(value.toObject()));
    }
    return snapshot;
}

bool isUnfinishedStatus(ProcessingStatus status)
{
    return status == ProcessingStatus::Pending ||
           status == ProcessingStatus::Processing ||
           status == ProcessingStatus::Paused ||
           status == ProcessingStatus::Recoverable;
}

void recomputeMessageStatus(Message& message)
{
    bool hasRecoverable = false;
    bool hasProcessing = false;
    bool hasPending = false;
    bool hasPaused = false;
    bool hasFailed = false;
    bool hasCancelled = false;
    bool hasCompleted = false;

    for (const MediaFile& file : message.mediaFiles) {
        switch (file.status) {
        case ProcessingStatus::Recoverable:
            hasRecoverable = true;
            break;
        case ProcessingStatus::Processing:
            hasProcessing = true;
            break;
        case ProcessingStatus::Pending:
            hasPending = true;
            break;
        case ProcessingStatus::Paused:
            hasPaused = true;
            break;
        case ProcessingStatus::Failed:
            hasFailed = true;
            break;
        case ProcessingStatus::Cancelled:
            hasCancelled = true;
            break;
        case ProcessingStatus::Completed:
            hasCompleted = true;
            break;
        }
    }

    if (hasRecoverable) {
        message.status = ProcessingStatus::Recoverable;
    } else if (hasPaused) {
        message.status = ProcessingStatus::Paused;
    } else if (hasProcessing) {
        message.status = ProcessingStatus::Processing;
    } else if (hasPending) {
        message.status = ProcessingStatus::Pending;
    } else if (hasFailed) {
        message.status = ProcessingStatus::Failed;
    } else if (hasCancelled && !hasCompleted) {
        message.status = ProcessingStatus::Cancelled;
    } else if (hasCompleted) {
        message.status = ProcessingStatus::Completed;
    }
}

} // namespace

TaskRecoveryController::TaskRecoveryController(QObject* parent)
    : QObject(parent)
    , m_snapshotSyncTimer(new QTimer(this))
{
    m_snapshotSyncTimer->setSingleShot(true);
    m_snapshotSyncTimer->setInterval(240);
    connect(m_snapshotSyncTimer, &QTimer::timeout, this, &TaskRecoveryController::persistSnapshotNow);
}

void TaskRecoveryController::setSessionController(SessionController* sessionController)
{
    m_sessionController = sessionController;
}

void TaskRecoveryController::setProcessingController(ProcessingController* processingController)
{
    m_processingController = processingController;
    if (!m_processingController) {
        return;
    }

    connect(m_processingController, &ProcessingController::taskAdded,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::taskStarted,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::taskCompleted,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::taskFailed,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::taskCancelled,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::messageTasksPaused,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::messageTasksResumed,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::messageTasksCancelled,
            this, &TaskRecoveryController::scheduleSnapshotSync);
    connect(m_processingController, &ProcessingController::queueStatusChanged,
            this, &TaskRecoveryController::scheduleSnapshotSync);
}

bool TaskRecoveryController::hasPendingRecovery() const
{
    return m_hasPendingRecovery;
}

QVariantList TaskRecoveryController::recoverySummary() const
{
    return m_recoverySummary;
}

QString TaskRecoveryController::shutdownReason() const
{
    return m_shutdownReason;
}

bool TaskRecoveryController::restoreAvailable() const
{
    return m_restoreAvailable;
}

QString TaskRecoveryController::snapshotFilePath() const
{
    const QString dataRoot = SettingsController::instance()->effectiveDataPath();
    return QDir(dataRoot).filePath(QStringLiteral("system/recovery_snapshot.json"));
}

void TaskRecoveryController::ensureSnapshotDirectory() const
{
    const QFileInfo snapshotInfo(snapshotFilePath());
    QDir dir(snapshotInfo.dir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

bool TaskRecoveryController::loadSnapshotFromDisk(RecoverySnapshot& snapshot) const
{
    QFile file(snapshotFilePath());
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[TaskRecoveryController] Failed to open snapshot:" << file.fileName();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "[TaskRecoveryController] Invalid snapshot json:" << parseError.errorString();
        return false;
    }

    snapshot = snapshotFromJson(document.object());
    if (snapshot.snapshotVersion != kRecoverySnapshotVersion || !snapshot.isValid()) {
        qWarning() << "[TaskRecoveryController] Unsupported or empty snapshot version:" << snapshot.snapshotVersion;
        return false;
    }

    return true;
}

void TaskRecoveryController::saveSnapshotToDisk(const RecoverySnapshot& snapshot)
{
    ensureSnapshotDirectory();
    QFile file(snapshotFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[TaskRecoveryController] Failed to write snapshot:" << file.fileName();
        return;
    }

    const QJsonDocument document(toJson(snapshot));
    file.write(document.toJson(QJsonDocument::Indented));
    file.close();
}

void TaskRecoveryController::clearSnapshotFile()
{
    QFile::remove(snapshotFilePath());
}

RecoverySnapshot TaskRecoveryController::buildFallbackSnapshotFromCurrentSessions() const
{
    RecoverySnapshot snapshot;
    snapshot.snapshotVersion = kRecoverySnapshotVersion;
    snapshot.pauseModeAtShutdown = SettingsController::instance()->pauseMode();
    snapshot.shutdownReason = SettingsController::instance()->lastExitReason();
    snapshot.capturedAtMs = QDateTime::currentMSecsSinceEpoch();

    if (!m_sessionController || !m_sessionController->sessionModel()) {
        return snapshot;
    }

    SessionModel* sessionModel = m_sessionController->sessionModel();
    for (int sessionIndex = 0; sessionIndex < sessionModel->rowCount(); ++sessionIndex) {
        const Session session = sessionModel->sessionAt(sessionIndex);
        for (int messageIndex = 0; messageIndex < session.messages.size(); ++messageIndex) {
            const Message& message = session.messages.at(messageIndex);

            RecoverableMessageState messageState;
            messageState.sessionId = session.id;
            messageState.sessionName = session.name;
            messageState.messageId = message.id;
            messageState.preShutdownStatus = message.status;
            messageState.orderIndex = messageIndex;

            if (message.status == ProcessingStatus::Paused) {
                messageState.runIntent = RecoveryRunIntent::Paused;
            } else if (message.status == ProcessingStatus::Processing) {
                messageState.runIntent = RecoveryRunIntent::Active;
            } else {
                messageState.runIntent = RecoveryRunIntent::Queued;
            }

            for (int fileIndex = 0; fileIndex < message.mediaFiles.size(); ++fileIndex) {
                const MediaFile& file = message.mediaFiles.at(fileIndex);
                if (!isUnfinishedStatus(file.status)) {
                    continue;
                }

                RecoverableFileState fileState;
                fileState.fileId = file.id;
                fileState.preShutdownStatus = file.status;
                fileState.orderIndex = fileIndex;
                messageState.fileStates.append(fileState);
            }

            if (!messageState.fileStates.isEmpty()) {
                snapshot.messages.append(messageState);
            }
        }
    }

    return snapshot;
}

QVariantList TaskRecoveryController::buildSummaryFromSnapshot(const RecoverySnapshot& snapshot) const
{
    struct SummaryItem {
        QString sessionId;
        QString sessionName;
        int processingCount = 0;
        int pendingCount = 0;
        int pausedCount = 0;
        int recoverableCount = 0;
    };

    QHash<QString, SummaryItem> summaries;
    QStringList order;

    for (const RecoverableMessageState& messageState : snapshot.messages) {
        if (messageState.messageId.isEmpty()) {
            continue;
        }

        if (!summaries.contains(messageState.sessionId)) {
            SummaryItem item;
            item.sessionId = messageState.sessionId;
            item.sessionName = messageState.sessionName.trimmed().isEmpty()
                ? tr("未命名会话")
                : messageState.sessionName;
            summaries.insert(messageState.sessionId, item);
            order.append(messageState.sessionId);
        }

        SummaryItem& item = summaries[messageState.sessionId];
        item.recoverableCount += 1;
        switch (messageState.preShutdownStatus) {
        case ProcessingStatus::Processing:
            item.processingCount += 1;
            break;
        case ProcessingStatus::Paused:
            item.pausedCount += 1;
            break;
        case ProcessingStatus::Pending:
        case ProcessingStatus::Recoverable:
        default:
            item.pendingCount += 1;
            break;
        }
    }

    QVariantList summary;
    for (const QString& sessionId : order) {
        const SummaryItem& item = summaries[sessionId];
        QVariantMap map;
        map[QStringLiteral("sessionId")] = item.sessionId;
        map[QStringLiteral("sessionName")] = item.sessionName;
        map[QStringLiteral("processingCount")] = item.processingCount;
        map[QStringLiteral("pendingCount")] = item.pendingCount;
        map[QStringLiteral("pausedCount")] = item.pausedCount;
        map[QStringLiteral("recoverableCount")] = item.recoverableCount;
        map[QStringLiteral("totalCount")] = item.recoverableCount;
        summary.append(map);
    }
    return summary;
}

void TaskRecoveryController::setPendingRecoveryState(bool pending)
{
    if (m_hasPendingRecovery == pending) {
        return;
    }
    m_hasPendingRecovery = pending;
    emit hasPendingRecoveryChanged();
}

void TaskRecoveryController::initializeStartupRecovery()
{
    const bool snapshotFileExists = QFile::exists(snapshotFilePath());
    RecoverySnapshot snapshot;
    bool loadedFromDisk = loadSnapshotFromDisk(snapshot);
    bool restoreAvailable = true;

    if (!loadedFromDisk) {
        snapshot = buildFallbackSnapshotFromCurrentSessions();
        restoreAvailable = !snapshotFileExists;
    }

    if (!snapshot.isValid()) {
        if (snapshotFileExists) {
            clearSnapshotFile();
        }
        if (!m_recoverySummary.isEmpty()) {
            m_recoverySummary.clear();
            emit recoverySummaryChanged();
        }
        if (!m_shutdownReason.isEmpty()) {
            m_shutdownReason.clear();
            emit shutdownReasonChanged();
        }
        if (!m_restoreAvailable) {
            m_restoreAvailable = true;
            emit restoreAvailableChanged();
        }
        setPendingRecoveryState(false);
        return;
    }

    m_snapshot = snapshot;
    if (m_restoreAvailable != restoreAvailable) {
        m_restoreAvailable = restoreAvailable;
        emit restoreAvailableChanged();
    }

    const QString nextShutdownReason = m_snapshot.shutdownReason.trimmed().isEmpty()
        ? QStringLiteral("normal")
        : m_snapshot.shutdownReason;
    if (m_shutdownReason != nextShutdownReason) {
        m_shutdownReason = nextShutdownReason;
        emit shutdownReasonChanged();
    }

    applyRecoverableStateToSessions();

    const QVariantList summary = buildSummaryFromSnapshot(m_snapshot);
    if (m_recoverySummary != summary) {
        m_recoverySummary = summary;
        emit recoverySummaryChanged();
    }

    setPendingRecoveryState(!m_recoverySummary.isEmpty());
    persistSnapshotNow();
}

void TaskRecoveryController::applyRecoverableStateToSessions()
{
    if (!m_sessionController) {
        return;
    }

    for (const RecoverableMessageState& messageState : m_snapshot.messages) {
        Session* session = m_sessionController->getSession(messageState.sessionId);
        if (!session) {
            continue;
        }

        Message message;
        bool foundMessage = false;
        for (const Message& candidate : session->messages) {
            if (candidate.id == messageState.messageId) {
                message = candidate;
                foundMessage = true;
                break;
            }
        }
        if (!foundMessage) {
            continue;
        }

        for (const RecoverableFileState& fileState : messageState.fileStates) {
            for (MediaFile& file : message.mediaFiles) {
                if (file.id == fileState.fileId && isUnfinishedStatus(file.status)) {
                    file.status = ProcessingStatus::Recoverable;
                    break;
                }
            }
        }

        message.status = ProcessingStatus::Recoverable;
        message.progress = 0;
        message.queuePosition = -1;
        message.errorMessage.clear();
        m_sessionController->updateMessageInSession(messageState.sessionId, message);
    }

    m_sessionController->reloadActiveSessionMessages();
    m_sessionController->saveSessionsImmediately();
}

void TaskRecoveryController::failRecoverableTasks(const QString& errorMessage)
{
    if (!m_sessionController) {
        return;
    }

    for (const RecoverableMessageState& messageState : m_snapshot.messages) {
        Session* session = m_sessionController->getSession(messageState.sessionId);
        if (!session) {
            continue;
        }

        Message message;
        bool foundMessage = false;
        for (const Message& candidate : session->messages) {
            if (candidate.id == messageState.messageId) {
                message = candidate;
                foundMessage = true;
                break;
            }
        }
        if (!foundMessage) {
            continue;
        }

        for (const RecoverableFileState& fileState : messageState.fileStates) {
            for (MediaFile& file : message.mediaFiles) {
                if (file.id == fileState.fileId && isUnfinishedStatus(file.status)) {
                    file.status = ProcessingStatus::Failed;
                    break;
                }
            }
        }

        message.errorMessage = errorMessage;
        message.progress = 0;
        message.queuePosition = -1;
        recomputeMessageStatus(message);
        m_sessionController->updateMessageInSession(messageState.sessionId, message);
    }

    m_sessionController->reloadActiveSessionMessages();
    m_sessionController->saveSessionsImmediately();
    clearSnapshotFile();

    m_snapshot = RecoverySnapshot();
    if (!m_recoverySummary.isEmpty()) {
        m_recoverySummary.clear();
        emit recoverySummaryChanged();
    }
    if (!m_shutdownReason.isEmpty()) {
        m_shutdownReason.clear();
        emit shutdownReasonChanged();
    }
    if (!m_restoreAvailable) {
        m_restoreAvailable = true;
        emit restoreAvailableChanged();
    }
    setPendingRecoveryState(false);
}

void TaskRecoveryController::persistSnapshotNow()
{
    if (m_hasPendingRecovery) {
        if (m_snapshot.isValid()) {
            saveSnapshotToDisk(m_snapshot);
        }
        return;
    }

    if (!m_processingController) {
        clearSnapshotFile();
        return;
    }

    const RecoverySnapshot snapshot = m_processingController->buildRecoverySnapshot();
    if (!snapshot.isValid()) {
        clearSnapshotFile();
        return;
    }

    saveSnapshotToDisk(snapshot);
}

QVariantList TaskRecoveryController::getPauseModeSwitchBlockers() const
{
    if (m_hasPendingRecovery) {
        return m_recoverySummary;
    }

    if (!m_processingController) {
        return {};
    }
    return m_processingController->getPauseModeSwitchBlockers();
}

void TaskRecoveryController::resolveRestorePreviousState()
{
    if (!m_hasPendingRecovery) {
        return;
    }

    if (!m_restoreAvailable || !m_processingController || !m_processingController->restoreFromRecoverySnapshot(m_snapshot)) {
        failRecoverableTasks(tr("恢复快照不可用，未完成任务已标记为失败"));
        return;
    }

    m_snapshot = RecoverySnapshot();
    if (!m_recoverySummary.isEmpty()) {
        m_recoverySummary.clear();
        emit recoverySummaryChanged();
    }
    if (!m_shutdownReason.isEmpty()) {
        m_shutdownReason.clear();
        emit shutdownReasonChanged();
    }
    if (!m_restoreAvailable) {
        m_restoreAvailable = true;
        emit restoreAvailableChanged();
    }
    setPendingRecoveryState(false);
    persistSnapshotNow();
}

void TaskRecoveryController::resolveFailAllRecoverableTasks()
{
    if (!m_hasPendingRecovery) {
        return;
    }
    failRecoverableTasks(tr("应用关闭导致任务中断，未执行恢复"));
}

void TaskRecoveryController::scheduleSnapshotSync()
{
    if (m_hasPendingRecovery) {
        return;
    }
    if (m_snapshotSyncTimer) {
        m_snapshotSyncTimer->start();
    }
}

} // namespace EnhanceVision
