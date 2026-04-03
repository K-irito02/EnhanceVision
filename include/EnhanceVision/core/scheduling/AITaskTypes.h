/**
 * @file AITaskTypes.h
 * @brief 调度层任务类型定义
 * @author EnhanceVision Team
 * 
 * 定义任务调度相关的核心类型。
 */

#ifndef ENHANCEVISION_AITASKTYPES_H
#define ENHANCEVISION_AITASKTYPES_H

#include <QString>
#include <QVariantMap>
#include <QDateTime>
#include <atomic>

namespace EnhanceVision {

/**
 * @brief 任务状态枚举
 */
enum class AITaskState {
    Idle,           ///< 空闲状态
    Queued,         ///< 已入队
    Preparing,      ///< 准备中
    Processing,     ///< 处理中
    Completed,      ///< 已完成
    Failed,         ///< 已失败
    Cancelled       ///< 已取消
};

/**
 * @brief 任务媒体类型
 */
enum class AIMediaType {
    Image,
    Video
};

/**
 * @brief 任务优先级
 */
enum class AITaskPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief 取消原因
 */
enum class AICancelReason {
    None,
    UserRequest,
    Timeout,
    ResourceLimit,
    SystemShutdown,
    TaskReplaced
};

/**
 * @brief 错误类型
 */
enum class AIErrorType {
    None,
    ModelLoadFailed,
    InferenceFailed,
    GpuOutOfMemory,
    FileReadError,
    FileWriteError,
    InvalidInput,
    Timeout,
    Cancelled,
    Unknown
};

/**
 * @brief 任务请求
 */
struct AITaskRequest {
    QString taskId;
    QString messageId;
    QString sessionId;
    QString fileId;
    QString inputPath;
    QString outputPath;
    QString modelId;
    AIMediaType mediaType = AIMediaType::Image;
    AITaskPriority priority = AITaskPriority::Normal;
    QVariantMap params;
    QDateTime createTime = QDateTime::currentDateTime();
};

/**
 * @brief 任务结果
 */
struct AITaskResult {
    QString taskId;
    bool success = false;
    QString outputPath;
    QString errorMessage;
    AIErrorType errorType = AIErrorType::None;
    qint64 processingTimeMs = 0;
    QDateTime completeTime = QDateTime::currentDateTime();
    
    static AITaskResult makeSuccess(const QString& taskId, const QString& outputPath, qint64 timeMs) {
        AITaskResult r;
        r.taskId = taskId;
        r.success = true;
        r.outputPath = outputPath;
        r.processingTimeMs = timeMs;
        r.completeTime = QDateTime::currentDateTime();
        return r;
    }
    
    static AITaskResult makeFailure(const QString& taskId, const QString& error, AIErrorType type) {
        AITaskResult r;
        r.taskId = taskId;
        r.success = false;
        r.errorMessage = error;
        r.errorType = type;
        r.completeTime = QDateTime::currentDateTime();
        return r;
    }
    
    static AITaskResult makeCancelled(const QString& taskId) {
        AITaskResult r;
        r.taskId = taskId;
        r.success = false;
        r.errorMessage = QStringLiteral("任务已取消");
        r.errorType = AIErrorType::Cancelled;
        r.completeTime = QDateTime::currentDateTime();
        return r;
    }
};

/**
 * @brief 任务进度
 */
struct AITaskProgress {
    QString taskId;
    double progress = 0.0;
    QString stage;
    qint64 elapsedMs = 0;
    qint64 estimatedRemainingMs = -1;
};

/**
 * @brief 任务项
 */
struct AITaskItem {
    AITaskRequest request;
    AITaskState state = AITaskState::Idle;
    AICancelReason cancelReason = AICancelReason::None;
    QDateTime queuedTime;
    QDateTime stateChangeTime;
    
    AITaskItem() = default;
    
    explicit AITaskItem(const AITaskRequest& req)
        : request(req)
        , state(AITaskState::Queued)
        , cancelReason(AICancelReason::None)
        , queuedTime(QDateTime::currentDateTime())
        , stateChangeTime(QDateTime::currentDateTime())
    {}
};

/**
 * @brief 状态转字符串
 */
inline QString aiTaskStateToString(AITaskState state) {
    switch (state) {
        case AITaskState::Idle: return QStringLiteral("Idle");
        case AITaskState::Queued: return QStringLiteral("Queued");
        case AITaskState::Preparing: return QStringLiteral("Preparing");
        case AITaskState::Processing: return QStringLiteral("Processing");
        case AITaskState::Completed: return QStringLiteral("Completed");
        case AITaskState::Failed: return QStringLiteral("Failed");
        case AITaskState::Cancelled: return QStringLiteral("Cancelled");
        default: return QStringLiteral("Unknown");
    }
}

/**
 * @brief 错误类型转字符串
 */
inline QString aiErrorTypeToString(AIErrorType type) {
    switch (type) {
        case AIErrorType::None: return QStringLiteral("None");
        case AIErrorType::ModelLoadFailed: return QStringLiteral("ModelLoadFailed");
        case AIErrorType::InferenceFailed: return QStringLiteral("InferenceFailed");
        case AIErrorType::GpuOutOfMemory: return QStringLiteral("GpuOutOfMemory");
        case AIErrorType::FileReadError: return QStringLiteral("FileReadError");
        case AIErrorType::FileWriteError: return QStringLiteral("FileWriteError");
        case AIErrorType::InvalidInput: return QStringLiteral("InvalidInput");
        case AIErrorType::Timeout: return QStringLiteral("Timeout");
        case AIErrorType::Cancelled: return QStringLiteral("Cancelled");
        case AIErrorType::Unknown:
        default: return QStringLiteral("Unknown");
    }
}

} // namespace EnhanceVision

Q_DECLARE_METATYPE(EnhanceVision::AITaskRequest)
Q_DECLARE_METATYPE(EnhanceVision::AITaskResult)
Q_DECLARE_METATYPE(EnhanceVision::AITaskProgress)
Q_DECLARE_METATYPE(EnhanceVision::AITaskState)
Q_DECLARE_METATYPE(EnhanceVision::AIErrorType)
Q_DECLARE_METATYPE(EnhanceVision::AICancelReason)

#endif // ENHANCEVISION_AITASKTYPES_H
