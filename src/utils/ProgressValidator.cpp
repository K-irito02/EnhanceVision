#include "EnhanceVision/utils/ProgressValidator.h"
#include <algorithm>

namespace EnhanceVision {

bool ProgressValidator::isValid(int progress)
{
    return progress >= 0 && progress <= 100;
}

bool ProgressValidator::isValidTransition(int current, int previous)
{
    if (!isValid(current) || !isValid(previous)) {
        return false;
    }
    
    if (current == 0 && previous > 0) {
        return true;
    }
    
    if (current == 100 && previous < 100) {
        return true;
    }
    
    return current >= previous;
}

bool ProgressValidator::isJumpTooLarge(int current, int previous, int maxJump)
{
    if (current <= previous) {
        return false;
    }
    return (current - previous) > maxJump;
}

QList<int> ProgressValidator::interpolateSteps(int from, int to, int maxStep)
{
    QList<int> steps;
    
    if (from >= to) {
        return steps;
    }
    
    int current = from;
    while (current < to) {
        int next = std::min(current + maxStep, to);
        if (next != current) {
            steps.append(next);
        }
        current = next;
    }
    
    return steps;
}

int ProgressValidator::clampToMax(int progress, int max)
{
    return std::clamp(progress, 0, max);
}

int ProgressValidator::normalizeProgress(double progress)
{
    int intProgress = static_cast<int>(std::round(progress * 100.0));
    return std::clamp(intProgress, 0, 100);
}

int ProgressValidator::phaseToProgress(ProgressPhase phase)
{
    return static_cast<int>(phase);
}

ProgressPhase ProgressValidator::getPhase(int progress)
{
    if (progress >= 100) return ProgressPhase::Completed;
    if (progress >= 95) return ProgressPhase::Saving;
    if (progress >= 85) return ProgressPhase::Postprocess;
    if (progress >= 15) return ProgressPhase::Inference;
    if (progress >= 5) return ProgressPhase::Preprocess;
    return ProgressPhase::Init;
}

}
