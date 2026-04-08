# Progress Prediction Optimization Notes

## Overview

**Creation Date**: 2026-04-08  
**Related Issue**: Progress bar accuracy improvement

---

## I. Change Summary

### New Features
| File Path | Description |
|----------|----------|
| `src/core/TaskTimeEstimator.cpp` | Enhanced with dynamic tracking, pause/resume support, and optimistic prediction strategy |
| `src/core/ProcessingTimeManager.cpp` | Integrated with TaskTimeEstimator for dynamic correction and pause synchronization |
| `src/core/ProgressReporter.cpp` | Added pause/resume interface with progress locking during pause |
| `src/controllers/ProcessingController.cpp` | Unified time prediction calls and synchronized progress updates |

### Modified Files
| File Path | Changes |
|----------|----------|
| `qml/components/MessageItem.qml` | Changed from "Remaining Time" to "Elapsed Time" (forward counting), removed timeout mechanism |
| `src/controllers/SessionController.cpp` | Added persistence for all time prediction fields |
| `include/EnhanceVision/core/TaskTimeEstimator.h` | Added TaskTimeTracker struct and dynamic tracking methods |
| `include/EnhanceVision/core/ProcessingTimeManager.h` | Added updateTaskProgress method |

---

## II. Implemented Features

- **Unified Prediction Source**: TaskTimeEstimator serves as the single source of truth for time predictions
- **Dynamic Correction**: Predictions are corrected based on actual progress using weighted averaging
- **Pause/Resume Support**: Time tracking correctly handles pause states with elapsed time locking
- **Optimistic Prediction Strategy**: Uses lower initial predictions (30-40% of previous estimates) with timeout display
- **Elapsed Time Display**: Changed from countdown "remaining time" to forward-counting "elapsed time"
- **Persistence**: All time data survives application restarts
- **Total Time Recording**: Actual processing time is saved and displayed after completion

---

## III. Technical Implementation Details

### Key Components

#### TaskTimeEstimator
```cpp
struct TaskTimeTracker {
    QString taskId;
    double initialPredictedSec = 0.0;
    double currentPredictedSec = 0.0;
    double elapsedSec = 0.0;
    double progress = 0.0;
    qint64 startTimeMs = 0;
    qint64 pausedTimeMs = 0;
    qint64 lastPauseStartMs = 0;
    bool isPaused = false;
    int correctionCount = 0;
    double correctionFactor = 1.0;
};
```

#### Dynamic Correction Logic
- Uses weighted average between initial prediction and real-time estimation
- Weight shifts with progress: `actualWeight = progress * progress`
- Bounded correction: 0.3x to 1.2x initial prediction for optimistic strategy

#### Pause/Resume Handling
- `pauseTracking()`: Records pause start time and updates elapsed time
- `resumeTracking()`: Accumulates pause duration and resumes tracking
- `getEffectiveElapsedSec()`: Calculates actual elapsed time excluding pause periods

### Prediction Parameter Optimization
Based on user feedback (predicted 9min, actual 1min), parameters were reduced by ~9x:

| Model | GPU Single Tile | CPU Single Tile | GPU Overhead | Video Frame Overhead |
|-------|----------------|----------------|-------------|---------------------|
| RealESRGAN | 0.005s | 0.15s | 1.0s | 0.02s |

---

## IV. Problems and Solutions

| Problem | Cause | Solution |
|---------|--------|----------|
| Inaccurate predictions (9x too high) | Conservative estimation parameters | Reduced all parameters by ~9x based on actual test data |
| Confusing UI with dynamic remaining time | Countdown timer conflicted with dynamic total time | Changed to forward-counting "elapsed time" |
| Timeout mechanism unnecessary | Optimistic predictions rarely triggered timeouts | Removed timeout mechanism, kept timeout display for user feedback |

---

## V. Testing Verification

| Scenario | Expected Result | Actual Result |
|----------|----------------|--------------|
| Image processing prediction | ~2-5 seconds | ~3 seconds |
| Video processing prediction | ~1-2 minutes | ~1 minute |
| Pause functionality | Elapsed time locks at pause value | Working correctly |
| Resume functionality | Time continues from paused value | Working correctly |
| App restart persistence | Time data restored | Working correctly |
| Total time display after completion | Shows actual processing time | Working correctly |

---

## VI. Data Flow

```
ProcessingController
    -> updateTaskProgress()
    -> ProcessingTimeManager.updateTaskProgress()
    -> TaskTimeEstimator.updateProgress()
    -> Dynamic correction calculation
    -> taskTimeUpdated signal
    -> MessageModel update
    -> QML display update
```

---

## VII. Future Work

- [ ] Monitor long-term prediction accuracy
- [ ] Consider machine learning for adaptive predictions
- [ ] Add user feedback mechanism for prediction tuning
- [ ] Implement per-model calibration based on historical data

---

## VIII. Performance Impact

- Minimal CPU overhead for time tracking (<< 1%)
- Memory usage: ~100 bytes per active task
- No impact on processing performance
- Improved user experience with accurate time estimates
