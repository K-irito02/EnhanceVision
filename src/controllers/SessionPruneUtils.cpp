#include "EnhanceVision/controllers/SessionPruneUtils.h"

namespace EnhanceVision {

void recomputeMessageStatus(Message& message)
{
    bool hasRecoverable = false;
    bool hasPaused = false;
    bool hasProcessing = false;
    bool hasPending = false;
    bool hasFailed = false;
    bool hasCancelled = false;
    bool hasCompleted = false;

    for (const MediaFile& file : message.mediaFiles) {
        switch (file.status) {
        case ProcessingStatus::Recoverable:
            hasRecoverable = true;
            break;
        case ProcessingStatus::Paused:
            hasPaused = true;
            break;
        case ProcessingStatus::Processing:
            hasProcessing = true;
            break;
        case ProcessingStatus::Pending:
            hasPending = true;
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
    } else {
        message.status = ProcessingStatus::Pending;
    }
}

void normalizeMessageAfterPrune(Message& message)
{
    recomputeMessageStatus(message);

    if (message.status != ProcessingStatus::Processing &&
        message.status != ProcessingStatus::Paused &&
        message.status != ProcessingStatus::Recoverable) {
        message.progress = (message.status == ProcessingStatus::Completed) ? 100 : 0;
        message.queuePosition = -1;
        message.processingStartTime = 0;
        message.predictedTotalSec = 0;
        message.elapsedSec = 0;
        message.remainingSec = 0;
        message.isOvertime = false;
    }

    bool hasFailedFile = false;
    bool hasCancelledFile = false;
    for (const MediaFile& file : message.mediaFiles) {
        if (file.status == ProcessingStatus::Failed) {
            hasFailedFile = true;
        } else if (file.status == ProcessingStatus::Cancelled) {
            hasCancelledFile = true;
        }
    }

    if (!hasFailedFile && !hasCancelledFile) {
        message.errorMessage.clear();
        message.errorTipDismissed = false;
    }
}

MessagePruneResult pruneMediaFilesFromMessage(Message& message, const QSet<QString>& fileIds)
{
    MessagePruneResult result;
    if (fileIds.isEmpty()) {
        return result;
    }

    for (int fileIndex = message.mediaFiles.size() - 1; fileIndex >= 0; --fileIndex) {
        if (!fileIds.contains(message.mediaFiles[fileIndex].id)) {
            continue;
        }

        message.mediaFiles.removeAt(fileIndex);
        ++result.removedFiles;
    }

    if (message.mediaFiles.isEmpty()) {
        result.removedMessage = (result.removedFiles > 0);
        return result;
    }

    if (result.removedFiles > 0) {
        normalizeMessageAfterPrune(message);
    }

    return result;
}

} // namespace EnhanceVision
