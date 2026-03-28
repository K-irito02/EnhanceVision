#ifndef ENHANCEVISION_UTILS_PROGRESSVALIDATOR_H
#define ENHANCEVISION_UTILS_PROGRESSVALIDATOR_H

#include <QList>

namespace EnhanceVision {

enum class ProgressPhase {
    Init = 0,
    Preprocess = 5,
    Inference = 15,
    Postprocess = 85,
    Saving = 95,
    Completed = 100
};

class ProgressValidator {
public:
    static constexpr int kMaxProgressBeforeComplete = 99;
    static constexpr int kMaxJumpThreshold = 15;
    static constexpr int kDefaultMaxStep = 5;

    static bool isValid(int progress);
    
    static bool isValidTransition(int current, int previous);
    
    static bool isJumpTooLarge(int current, int previous, int maxJump = kMaxJumpThreshold);
    
    static QList<int> interpolateSteps(int from, int to, int maxStep = kDefaultMaxStep);
    
    static int clampToMax(int progress, int max = kMaxProgressBeforeComplete);
    
    static int normalizeProgress(double progress);
    
    static int phaseToProgress(ProgressPhase phase);
    
    static ProgressPhase getPhase(int progress);
};

} 

#endif
