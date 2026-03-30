/**
 * @file AIInferenceTypes.h
 * @brief AI推理系统类型定义
 * @author EnhanceVision Team
 * 
 * 定义AI推理模块的核心类型、枚举和数据结构。
 * 所有AI推理相关组件共享这些定义。
 */

#ifndef ENHANCEVISION_AIINFERENCETYPES_H
#define ENHANCEVISION_AIINFERENCETYPES_H

#include <QString>
#include <QVariantMap>
#include <QImage>
#include <QDateTime>
#include <atomic>
#include <memory>
#include <functional>

namespace EnhanceVision {

/**
 * @brief AI任务状态枚举
 * 
 * 使用严格的状态机模型，确保状态转换的原子性和一致性。
 * 状态转换规则：
 * - Idle -> Queued (任务入队)
 * - Queued -> Preparing (开始准备)
 * - Preparing -> Processing (开始处理)
 * - Processing -> Completed/Failed/Cancelled (处理结束)
 * - Any -> Cancelled (取消请求)
 */
enum class AITaskState {
    Idle,           ///< 空闲状态（初始状态）
    Queued,         ///< 已入队等待处理
    Preparing,      ///< 准备中（加载模型、分配资源）
    Processing,     ///< 处理中（推理进行中）
    Completed,      ///< 已完成（成功）
    Failed,         ///< 已失败（错误）
    Cancelled       ///< 已取消（用户取消）
};

/**
 * @brief AI任务媒体类型
 */
enum class AIMediaType {
    Image,          ///< 图像文件
    Video           ///< 视频文件
};

/**
 * @brief AI任务优先级
 */
enum class AITaskPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief 取消原因枚举
 */
enum class AICancelReason {
    None,           ///< 无取消
    UserRequest,    ///< 用户主动取消
    Timeout,        ///< 超时取消
    ResourceLimit,  ///< 资源限制
    SystemShutdown, ///< 系统关闭
    TaskReplaced    ///< 任务被替换
};

/**
 * @brief 错误类型枚举
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
 * @brief AI任务请求结构
 * 
 * 封装创建AI任务所需的所有信息。
 * 使用值语义，线程安全。
 */
struct AITaskRequest {
    QString taskId;             ///< 任务唯一ID
    QString messageId;          ///< 关联的消息ID
    QString sessionId;          ///< 关联的会话ID
    QString fileId;             ///< 关联的文件ID
    QString inputPath;          ///< 输入文件路径
    QString outputPath;         ///< 输出文件路径
    QString modelId;            ///< 模型ID
    AIMediaType mediaType;      ///< 媒体类型
    AITaskPriority priority;    ///< 任务优先级
    QVariantMap params;         ///< 模型参数
    QDateTime createTime;       ///< 创建时间
    
    AITaskRequest()
        : mediaType(AIMediaType::Image)
        , priority(AITaskPriority::Normal)
        , createTime(QDateTime::currentDateTime())
    {}
};

/**
 * @brief AI任务结果结构
 */
struct AITaskResult {
    QString taskId;             ///< 任务ID
    bool success;               ///< 是否成功
    QString outputPath;         ///< 输出文件路径
    QString errorMessage;       ///< 错误信息
    AIErrorType errorType;      ///< 错误类型
    qint64 processingTimeMs;    ///< 处理耗时（毫秒）
    QDateTime completeTime;     ///< 完成时间
    
    AITaskResult()
        : success(false)
        , errorType(AIErrorType::None)
        , processingTimeMs(0)
        , completeTime(QDateTime::currentDateTime())
    {}
    
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
 * @brief AI任务进度信息
 */
struct AITaskProgress {
    QString taskId;
    double progress;            ///< 进度 [0.0, 1.0]
    QString stage;              ///< 当前阶段描述
    qint64 elapsedMs;           ///< 已用时间
    qint64 estimatedRemainingMs;///< 预估剩余时间
    
    AITaskProgress()
        : progress(0.0)
        , elapsedMs(0)
        , estimatedRemainingMs(-1)
    {}
};

/**
 * @brief 任务回调函数类型
 */
using AIProgressCallback = std::function<void(const AITaskProgress&)>;
using AICompletionCallback = std::function<void(const AITaskResult&)>;
using AIStateChangeCallback = std::function<void(const QString& taskId, AITaskState oldState, AITaskState newState)>;

/**
 * @brief 将状态枚举转为字符串（用于日志）
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
        case AIErrorType::Unknown: return QStringLiteral("Unknown");
        default: return QStringLiteral("Unknown");
    }
}

} // namespace EnhanceVision

// Qt 元类型注册
Q_DECLARE_METATYPE(EnhanceVision::AITaskRequest)
Q_DECLARE_METATYPE(EnhanceVision::AITaskResult)
Q_DECLARE_METATYPE(EnhanceVision::AITaskProgress)
Q_DECLARE_METATYPE(EnhanceVision::AITaskState)
Q_DECLARE_METATYPE(EnhanceVision::AIErrorType)
Q_DECLARE_METATYPE(EnhanceVision::AICancelReason)

#endif // ENHANCEVISION_AIINFERENCETYPES_H
