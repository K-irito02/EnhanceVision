#ifndef ENHANCEVISION_SESSIONPRUNEUTILS_H
#define ENHANCEVISION_SESSIONPRUNEUTILS_H

#include <QSet>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

struct MessagePruneResult {
    int removedFiles = 0;
    bool removedMessage = false;
};

void recomputeMessageStatus(Message& message);
void normalizeMessageAfterPrune(Message& message);
MessagePruneResult pruneMediaFilesFromMessage(Message& message, const QSet<QString>& fileIds);

} // namespace EnhanceVision

#endif // ENHANCEVISION_SESSIONPRUNEUTILS_H
