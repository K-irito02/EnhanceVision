#include <QtTest>

#include "EnhanceVision/controllers/MessageStatusResolver.h"

using namespace EnhanceVision;

namespace {

MediaFile makeFile(const QString& id, ProcessingStatus status)
{
    MediaFile file;
    file.id = id;
    file.fileName = id;
    file.status = status;
    return file;
}

} // namespace

class MessageStatusResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void deriveStatusReturnsProcessingWhenAnyFileProcessing();
    void deriveStatusReturnsPausedWhenExplicitlyPaused();
    void deriveStatusReturnsRecoverableWhenAnyFileRecoverable();
    void deriveStatusReturnsPendingWhenOnlyPendingFilesRemain();
    void deriveStatusReturnsCancelledWhenAllFilesCancelled();
    void deriveStatusReturnsFailedWhenNoCompletedFilesRemain();
    void deriveStatusReturnsCompletedForMixedCompletedAndFailedFiles();
    void shouldSyncStatusDoesNotSuppressTerminalTransition();
};

void MessageStatusResolverTest::deriveStatusReturnsProcessingWhenAnyFileProcessing()
{
    const MessageStatusSummary summary = summarizeMessageMediaFiles({
        makeFile(QStringLiteral("file-1"), ProcessingStatus::Completed),
        makeFile(QStringLiteral("file-2"), ProcessingStatus::Processing)
    });

    QCOMPARE(deriveMessageStatus(summary, false), ProcessingStatus::Processing);
}

void MessageStatusResolverTest::deriveStatusReturnsPausedWhenExplicitlyPaused()
{
    const MessageStatusSummary summary = summarizeMessageMediaFiles({
        makeFile(QStringLiteral("file-1"), ProcessingStatus::Pending),
        makeFile(QStringLiteral("file-2"), ProcessingStatus::Completed)
    });

    QCOMPARE(deriveMessageStatus(summary, true), ProcessingStatus::Paused);
}

void MessageStatusResolverTest::deriveStatusReturnsRecoverableWhenAnyFileRecoverable()
{
    const MessageStatusSummary summary = summarizeMessageMediaFiles({
        makeFile(QStringLiteral("file-1"), ProcessingStatus::Recoverable),
        makeFile(QStringLiteral("file-2"), ProcessingStatus::Pending)
    });

    QCOMPARE(deriveMessageStatus(summary, false), ProcessingStatus::Recoverable);
}

void MessageStatusResolverTest::deriveStatusReturnsPendingWhenOnlyPendingFilesRemain()
{
    const MessageStatusSummary summary = summarizeMessageMediaFiles({
        makeFile(QStringLiteral("file-1"), ProcessingStatus::Pending),
        makeFile(QStringLiteral("file-2"), ProcessingStatus::Pending)
    });

    QCOMPARE(deriveMessageStatus(summary, false), ProcessingStatus::Pending);
}

void MessageStatusResolverTest::deriveStatusReturnsCancelledWhenAllFilesCancelled()
{
    const MessageStatusSummary summary = summarizeMessageMediaFiles({
        makeFile(QStringLiteral("file-1"), ProcessingStatus::Cancelled),
        makeFile(QStringLiteral("file-2"), ProcessingStatus::Cancelled)
    });

    QCOMPARE(deriveMessageStatus(summary, false), ProcessingStatus::Cancelled);
}

void MessageStatusResolverTest::deriveStatusReturnsFailedWhenNoCompletedFilesRemain()
{
    const MessageStatusSummary summary = summarizeMessageMediaFiles({
        makeFile(QStringLiteral("file-1"), ProcessingStatus::Failed),
        makeFile(QStringLiteral("file-2"), ProcessingStatus::Cancelled)
    });

    QCOMPARE(deriveMessageStatus(summary, false), ProcessingStatus::Failed);
}

void MessageStatusResolverTest::deriveStatusReturnsCompletedForMixedCompletedAndFailedFiles()
{
    const MessageStatusSummary summary = summarizeMessageMediaFiles({
        makeFile(QStringLiteral("file-1"), ProcessingStatus::Completed),
        makeFile(QStringLiteral("file-2"), ProcessingStatus::Failed)
    });

    QCOMPARE(deriveMessageStatus(summary, false), ProcessingStatus::Completed);
}

void MessageStatusResolverTest::shouldSyncStatusDoesNotSuppressTerminalTransition()
{
    QVERIFY(shouldSyncMessageStatus(ProcessingStatus::Processing, ProcessingStatus::Completed, true));
    QVERIFY(!shouldSyncMessageStatus(ProcessingStatus::Completed, ProcessingStatus::Completed, true));
    QVERIFY(shouldSyncMessageStatus(ProcessingStatus::Pending, ProcessingStatus::Pending, false));
}

QTEST_MAIN(MessageStatusResolverTest)

#include "MessageStatusResolverTest.moc"
