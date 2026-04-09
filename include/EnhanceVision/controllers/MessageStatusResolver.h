#ifndef ENHANCEVISION_MESSAGESTATUSRESOLVER_H
#define ENHANCEVISION_MESSAGESTATUSRESOLVER_H

#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

struct MessageStatusSummary {
    int pending = 0;
    int processing = 0;
    int completed = 0;
    int failed = 0;
    int cancelled = 0;
    int paused = 0;
    int recoverable = 0;

    [[nodiscard]] int total() const;
};

MessageStatusSummary summarizeMessageMediaFiles(const QList<MediaFile>& mediaFiles);
ProcessingStatus deriveMessageStatus(const MessageStatusSummary& summary, bool explicitlyPaused);
bool shouldSyncMessageStatus(ProcessingStatus lastStatus, ProcessingStatus nextStatus, bool hasLastStatus);

} // namespace EnhanceVision

#endif // ENHANCEVISION_MESSAGESTATUSRESOLVER_H
