#include <QtTest>
#include "EnhanceVision/controllers/SessionPruneUtils.h"

using namespace EnhanceVision;

namespace {

MediaFile makeFile(const QString& id, MediaType type, ProcessingStatus status)
{
    MediaFile file;
    file.id = id;
    file.fileName = id;
    file.type = type;
    file.status = status;
    return file;
}

} // namespace

class SessionPruneUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void pruneMixedMessageKeepsRemainingFiles();
    void pruneAllFilesMarksMessageRemoved();
};

void SessionPruneUtilsTest::pruneMixedMessageKeepsRemainingFiles()
{
    Message message;
    message.status = ProcessingStatus::Processing;
    message.progress = 57;
    message.queuePosition = 3;
    message.processingStartTime = 12345;
    message.predictedTotalSec = 22;
    message.elapsedSec = 7;
    message.remainingSec = 15;
    message.isOvertime = true;
    message.errorMessage = QStringLiteral("stale");
    message.errorTipDismissed = true;
    message.mediaFiles = {
        makeFile(QStringLiteral("image-1"), MediaType::Image, ProcessingStatus::Pending),
        makeFile(QStringLiteral("video-1"), MediaType::Video, ProcessingStatus::Completed)
    };

    const MessagePruneResult result = pruneMediaFilesFromMessage(
        message,
        QSet<QString>{QStringLiteral("image-1")});

    QCOMPARE(result.removedFiles, 1);
    QVERIFY(!result.removedMessage);
    QCOMPARE(message.mediaFiles.size(), 1);
    QCOMPARE(message.mediaFiles.first().id, QStringLiteral("video-1"));
    QCOMPARE(message.status, ProcessingStatus::Completed);
    QCOMPARE(message.progress, 100);
    QCOMPARE(message.queuePosition, -1);
    QCOMPARE(message.processingStartTime, 0);
    QCOMPARE(message.predictedTotalSec, 0);
    QCOMPARE(message.elapsedSec, 0);
    QCOMPARE(message.remainingSec, 0);
    QVERIFY(!message.isOvertime);
    QVERIFY(message.errorMessage.isEmpty());
    QVERIFY(!message.errorTipDismissed);
}

void SessionPruneUtilsTest::pruneAllFilesMarksMessageRemoved()
{
    Message message;
    message.status = ProcessingStatus::Recoverable;
    message.mediaFiles = {
        makeFile(QStringLiteral("image-1"), MediaType::Image, ProcessingStatus::Recoverable),
        makeFile(QStringLiteral("video-1"), MediaType::Video, ProcessingStatus::Completed)
    };

    const MessagePruneResult result = pruneMediaFilesFromMessage(
        message,
        QSet<QString>{QStringLiteral("image-1"), QStringLiteral("video-1")});

    QCOMPARE(result.removedFiles, 2);
    QVERIFY(result.removedMessage);
    QVERIFY(message.mediaFiles.isEmpty());
}

QTEST_MAIN(SessionPruneUtilsTest)

#include "SessionPruneUtilsTest.moc"
