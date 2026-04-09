#include "EnhanceVision/controllers/MessageStatusResolver.h"

namespace EnhanceVision {

int MessageStatusSummary::total() const
{
    return pending + processing + completed + failed + cancelled + paused + recoverable;
}

MessageStatusSummary summarizeMessageMediaFiles(const QList<MediaFile>& mediaFiles)
{
    MessageStatusSummary summary;

    for (const MediaFile& file : mediaFiles) {
        switch (file.status) {
        case ProcessingStatus::Pending:
            summary.pending++;
            break;
        case ProcessingStatus::Processing:
            summary.processing++;
            break;
        case ProcessingStatus::Completed:
            summary.completed++;
            break;
        case ProcessingStatus::Failed:
            summary.failed++;
            break;
        case ProcessingStatus::Cancelled:
            summary.cancelled++;
            break;
        case ProcessingStatus::Paused:
            summary.paused++;
            break;
        case ProcessingStatus::Recoverable:
            summary.recoverable++;
            break;
        }
    }

    return summary;
}

ProcessingStatus deriveMessageStatus(const MessageStatusSummary& summary, bool explicitlyPaused)
{
    if (summary.recoverable > 0) {
        return ProcessingStatus::Recoverable;
    }

    if (explicitlyPaused) {
        return ProcessingStatus::Paused;
    }

    if (summary.processing > 0) {
        return ProcessingStatus::Processing;
    }

    if (summary.paused > 0) {
        return ProcessingStatus::Paused;
    }

    if (summary.pending > 0) {
        return ProcessingStatus::Pending;
    }

    const int total = summary.total();
    if (total == 0) {
        return ProcessingStatus::Pending;
    }

    if (summary.cancelled == total) {
        return ProcessingStatus::Cancelled;
    }

    if (summary.completed == 0 && (summary.failed > 0 || summary.cancelled > 0)) {
        return ProcessingStatus::Failed;
    }

    return ProcessingStatus::Completed;
}

bool shouldSyncMessageStatus(ProcessingStatus lastStatus, ProcessingStatus nextStatus, bool hasLastStatus)
{
    return !hasLastStatus || lastStatus != nextStatus;
}

} // namespace EnhanceVision
