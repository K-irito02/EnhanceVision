#include <QtTest>

#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/services/InstallMaintenanceService.h"
#include "EnhanceVision/utils/StoragePaths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

using namespace EnhanceVision;

namespace {

QString testConfigRootPath()
{
    return StoragePaths::configRootPath();
}

QString testRunRootPath()
{
    const QString testFunction = QString::fromLatin1(
        QTest::currentTestFunction() ? QTest::currentTestFunction() : "InstallMaintenanceServiceTest");
    const QString processId = QString::number(QCoreApplication::applicationPid());
    const QString timeStamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    return QDir::cleanPath(QDir::current().filePath(
        QStringLiteral("test_runtime/install_maintenance_service/%1/%2/%3").arg(processId, testFunction, timeStamp)));
}

bool writeTextFile(const QString& path, const QByteArray& data)
{
    const QFileInfo fileInfo(path);
    QDir parentDir = fileInfo.dir();
    if (!parentDir.exists() && !QDir().mkpath(parentDir.absolutePath())) {
        qWarning() << "Failed to create parent directory" << parentDir.absolutePath() << "for" << path;
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open file for writing" << path << file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        qWarning() << "Failed to fully write file" << path << file.errorString();
        return false;
    }

    file.close();
    return true;
}

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QJsonObject loadJsonObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject makeSessionJson(const QString& oldDataPath)
{
    const QString normalizedOldPath = QDir::fromNativeSeparators(oldDataPath);
    const QString resultPath = QDir(normalizedOldPath).filePath(QStringLiteral("ai/images/result.png"));

    QJsonObject mediaFile;
    mediaFile[QStringLiteral("id")] = QStringLiteral("file-1");
    mediaFile[QStringLiteral("filePath")] = QDir(normalizedOldPath).filePath(QStringLiteral("inputs/input.png"));
    mediaFile[QStringLiteral("fileName")] = QStringLiteral("input.png");
    mediaFile[QStringLiteral("fileSize")] = 1;
    mediaFile[QStringLiteral("type")] = 0;
    mediaFile[QStringLiteral("duration")] = 0;
    mediaFile[QStringLiteral("width")] = 1;
    mediaFile[QStringLiteral("height")] = 1;
    mediaFile[QStringLiteral("status")] = static_cast<int>(ProcessingStatus::Failed);
    mediaFile[QStringLiteral("resultPath")] = resultPath;

    QJsonObject message;
    message[QStringLiteral("id")] = QStringLiteral("message-1");
    message[QStringLiteral("timestamp")] = QStringLiteral("2026-04-14T00:00:00");
    message[QStringLiteral("mode")] = 1;
    message[QStringLiteral("status")] = static_cast<int>(ProcessingStatus::Failed);
    message[QStringLiteral("errorMessage")] = QStringLiteral("old error");
    message[QStringLiteral("isSelected")] = false;
    message[QStringLiteral("progress")] = 0;
    message[QStringLiteral("queuePosition")] = -1;
    message[QStringLiteral("mediaFiles")] = QJsonArray{mediaFile};
    message[QStringLiteral("parameters")] = QJsonObject{};
    message[QStringLiteral("shaderParams")] = QJsonObject{};
    message[QStringLiteral("aiParams")] = QJsonObject{};

    QJsonObject session;
    session[QStringLiteral("id")] = QStringLiteral("session-1");
    session[QStringLiteral("name")] = QStringLiteral("Session");
    session[QStringLiteral("createdAt")] = QStringLiteral("2026-04-14T00:00:00");
    session[QStringLiteral("modifiedAt")] = QStringLiteral("2026-04-14T00:00:00");
    session[QStringLiteral("isActive")] = true;
    session[QStringLiteral("isSelected")] = false;
    session[QStringLiteral("isPinned")] = false;
    session[QStringLiteral("sortIndex")] = 0;
    session[QStringLiteral("messages")] = QJsonArray{message};
    session[QStringLiteral("pendingFiles")] = QJsonArray{};

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("lastActiveSessionId")] = QStringLiteral("session-1");
    root[QStringLiteral("sessionCounter")] = 1;
    root[QStringLiteral("sessions")] = QJsonArray{session};
    return root;
}

} // namespace

class InstallMaintenanceServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void migrateIntentMovesRuntimeAndMetadataAndRewritesSessionPaths();
    void keepIntentPreservesOldDataPathAndMovesMetadata();
    void keepIntentFallsBackToDetectedLegacyDataPathWhenOldPathIsInvalid();
    void deleteIntentClearsMetadataAndOldData();

private:
    QString m_testRootPath;
};

void InstallMaintenanceServiceTest::init()
{
    m_testRootPath = testRunRootPath();
    QVERIFY(QDir().mkpath(m_testRootPath));

    const QByteArray localAppData = QDir(m_testRootPath).filePath(QStringLiteral("local")).toUtf8();
    const QByteArray appData = QDir(m_testRootPath).filePath(QStringLiteral("roaming")).toUtf8();
    const QByteArray configRootOverride = QDir(m_testRootPath).filePath(QStringLiteral("config-root")).toUtf8();
    const QByteArray configRootOverrideEnvVar = StoragePaths::configRootOverrideEnvVarName().toUtf8();
    QVERIFY(qputenv("LOCALAPPDATA", localAppData));
    QVERIFY(qputenv("APPDATA", appData));
    QVERIFY(qputenv(configRootOverrideEnvVar.constData(), configRootOverride));

    QDir().mkpath(QString::fromUtf8(localAppData));
    QDir().mkpath(QString::fromUtf8(appData));

    const QString configRootPath = testConfigRootPath();
    QVERIFY2(!configRootPath.isEmpty(), "Config root path should not be empty");

    QDir configRootDir(configRootPath);
    if (!configRootDir.exists()) {
        QVERIFY2(QDir().mkpath(configRootPath), qPrintable(configRootPath));
        configRootDir = QDir(configRootPath);
    }

    const QFileInfoList staleEntries = configRootDir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    for (const QFileInfo& entry : staleEntries) {
        if (entry.isDir()) {
            QVERIFY(QDir(entry.absoluteFilePath()).removeRecursively());
        } else {
            QVERIFY(QFile::remove(entry.absoluteFilePath()));
        }
    }

    QFile::remove(StoragePaths::settingsFilePath());
}

void InstallMaintenanceServiceTest::migrateIntentMovesRuntimeAndMetadataAndRewritesSessionPaths()
{
    const QString oldDataPath = QDir(m_testRootPath).filePath(QStringLiteral("old-data"));
    const QString targetDataPath = QDir(m_testRootPath).filePath(QStringLiteral("new-data"));
    const QString legacySessionsPath = QDir(m_testRootPath).filePath(QStringLiteral("legacy/sessions.json"));
    const QString legacyUiStatePath = QDir(m_testRootPath).filePath(QStringLiteral("legacy/ui_state.json"));

    QVERIFY(writeTextFile(QDir(oldDataPath).filePath(QStringLiteral("ai/images/result.png")), "result"));
    QVERIFY(writeTextFile(QDir(oldDataPath).filePath(QStringLiteral("system/recovery_snapshot.json")), "{}"));
    QVERIFY(writeTextFile(legacySessionsPath, QJsonDocument(makeSessionJson(oldDataPath)).toJson(QJsonDocument::Indented)));
    QVERIFY(writeTextFile(legacyUiStatePath, "{}"));

    InstallMaintenanceIntent intent;
    intent.action = InstallMaintenanceIntent::Action::Migrate;
    intent.oldDataPath = oldDataPath;
    intent.targetDataPath = targetDataPath;
    intent.legacySessionsPath = legacySessionsPath;
    intent.legacyUiStatePath = legacyUiStatePath;

    QString error;
    QVERIFY2(InstallMaintenanceService::executeIntent(intent, &error), qPrintable(error));

    QVERIFY(QFileInfo::exists(QDir(targetDataPath).filePath(QStringLiteral("ai/images/result.png"))));
    QVERIFY(QFileInfo::exists(QDir(targetDataPath).filePath(QStringLiteral("system/recovery_snapshot.json"))));
    QVERIFY(QFileInfo::exists(StoragePaths::sessionsFilePath()));
    QVERIFY(QFileInfo::exists(StoragePaths::uiStateFilePath()));
    const QString settingsText = readTextFile(StoragePaths::settingsFilePath());
    QVERIFY(settingsText.contains(QStringLiteral("customDataPath=%1")
                                      .arg(QDir::cleanPath(QDir::fromNativeSeparators(targetDataPath)))));

    const QJsonObject root = loadJsonObject(StoragePaths::sessionsFilePath());
    const QJsonObject session = root.value(QStringLiteral("sessions")).toArray().at(0).toObject();
    const QJsonObject message = session.value(QStringLiteral("messages")).toArray().at(0).toObject();
    const QJsonObject mediaFile = message.value(QStringLiteral("mediaFiles")).toArray().at(0).toObject();

    QCOMPARE(mediaFile.value(QStringLiteral("status")).toInt(), static_cast<int>(ProcessingStatus::Completed));
    QCOMPARE(message.value(QStringLiteral("status")).toInt(), static_cast<int>(ProcessingStatus::Completed));
    QVERIFY(message.value(QStringLiteral("errorMessage")).toString().isEmpty());
    QVERIFY(mediaFile.value(QStringLiteral("resultPath")).toString().startsWith(
        QDir::cleanPath(QDir::fromNativeSeparators(targetDataPath))));
}

void InstallMaintenanceServiceTest::keepIntentPreservesOldDataPathAndMovesMetadata()
{
    const QString oldDataPath = QDir(m_testRootPath).filePath(QStringLiteral("keep-old-data"));
    const QString targetDataPath = QDir(m_testRootPath).filePath(QStringLiteral("keep-target-data"));
    const QString legacySessionsPath = QDir(m_testRootPath).filePath(QStringLiteral("keep/sessions.json"));
    const QString legacyUiStatePath = QDir(m_testRootPath).filePath(QStringLiteral("keep/ui_state.json"));

    QVERIFY(writeTextFile(QDir(oldDataPath).filePath(QStringLiteral("ai/images/keep.txt")), "keep"));
    QVERIFY(writeTextFile(legacySessionsPath, "{}"));
    QVERIFY(writeTextFile(legacyUiStatePath, "{}"));

    QVERIFY(writeTextFile(StoragePaths::settingsFilePath(),
                          QStringLiteral("[behavior]\ncustomDataPath=%1\n")
                              .arg(QDir::cleanPath(QDir::fromNativeSeparators(targetDataPath)))
                              .toUtf8()));

    InstallMaintenanceIntent intent;
    intent.action = InstallMaintenanceIntent::Action::Keep;
    intent.oldDataPath = oldDataPath;
    intent.targetDataPath = targetDataPath;
    intent.legacySessionsPath = legacySessionsPath;
    intent.legacyUiStatePath = legacyUiStatePath;

    QString error;
    QVERIFY2(InstallMaintenanceService::executeIntent(intent, &error), qPrintable(error));

    QVERIFY(QFileInfo::exists(QDir(oldDataPath).filePath(QStringLiteral("ai/images/keep.txt"))));
    QVERIFY(QFileInfo::exists(StoragePaths::sessionsFilePath()));
    QVERIFY(QFileInfo::exists(StoragePaths::uiStateFilePath()));

    const QString settingsText = readTextFile(StoragePaths::settingsFilePath());
    QVERIFY(settingsText.contains(QStringLiteral("customDataPath=%1")
                                      .arg(QDir::cleanPath(QDir::fromNativeSeparators(oldDataPath)))));
}

void InstallMaintenanceServiceTest::keepIntentFallsBackToDetectedLegacyDataPathWhenOldPathIsInvalid()
{
    const QString invalidOldDataPath = QDir(m_testRootPath).filePath(QStringLiteral("missing-old-data"));
    const QString targetDataPath = QDir(m_testRootPath).filePath(QStringLiteral("keep-target-data"));
    const QString detectedLegacyDataPath = StoragePaths::defaultConfiguredDataPath();

    QVERIFY(writeTextFile(QDir(detectedLegacyDataPath).filePath(QStringLiteral("ai/images/fallback.txt")), "ok"));

    InstallMaintenanceIntent intent;
    intent.action = InstallMaintenanceIntent::Action::Keep;
    intent.oldDataPath = invalidOldDataPath;
    intent.targetDataPath = targetDataPath;

    QString error;
    QVERIFY2(InstallMaintenanceService::executeIntent(intent, &error), qPrintable(error));

    const QString settingsText = readTextFile(StoragePaths::settingsFilePath());
    QVERIFY(settingsText.contains(QStringLiteral("customDataPath=%1")
                                      .arg(QDir::cleanPath(QDir::fromNativeSeparators(detectedLegacyDataPath)))));
}

void InstallMaintenanceServiceTest::deleteIntentClearsMetadataAndOldData()
{
    const QString oldDataPath = QDir(m_testRootPath).filePath(QStringLiteral("delete-old-data"));
    const QString targetDataPath = QDir(m_testRootPath).filePath(QStringLiteral("delete-target-data"));
    const QString legacySessionsPath = QDir(m_testRootPath).filePath(QStringLiteral("delete/sessions.json"));
    const QString legacyUiStatePath = QDir(m_testRootPath).filePath(QStringLiteral("delete/ui_state.json"));

    QVERIFY(writeTextFile(QDir(oldDataPath).filePath(QStringLiteral("ai/images/delete.txt")), "delete"));
    QVERIFY(writeTextFile(StoragePaths::sessionsFilePath(), "{}"));
    QVERIFY(writeTextFile(StoragePaths::uiStateFilePath(), "{}"));
    QVERIFY(writeTextFile(legacySessionsPath, "{}"));
    QVERIFY(writeTextFile(legacyUiStatePath, "{}"));

    InstallMaintenanceIntent intent;
    intent.action = InstallMaintenanceIntent::Action::Delete;
    intent.oldDataPath = oldDataPath;
    intent.targetDataPath = targetDataPath;
    intent.legacySessionsPath = legacySessionsPath;
    intent.legacyUiStatePath = legacyUiStatePath;

    QString error;
    QVERIFY2(InstallMaintenanceService::executeIntent(intent, &error), qPrintable(error));

    QVERIFY(QFileInfo::exists(targetDataPath));

    const QString settingsText = readTextFile(StoragePaths::settingsFilePath());
    QVERIFY(settingsText.contains(QStringLiteral("customDataPath=%1")
                                      .arg(QDir::cleanPath(QDir::fromNativeSeparators(targetDataPath)))));
}

QTEST_MAIN(InstallMaintenanceServiceTest)

#include "InstallMaintenanceServiceTest.moc"
