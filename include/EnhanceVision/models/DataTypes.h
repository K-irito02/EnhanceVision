/**
 * @file DataTypes.h
 * @brief 公共数据类型和结构定义
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_DATATYPES_H
#define ENHANCEVISION_DATATYPES_H

#include <QString>
#include <QDateTime>
#include <QSize>
#include <QImage>
#include <QVariantMap>
#include <QList>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <functional>
#include <memory>
#include <atomic>

namespace EnhanceVision {

/**
 * @brief 媒体文件类型枚举
 */
enum class MediaType {
    Image,  ///< 图片类型
    Video   ///< 视频类型
};

/**
 * @brief 处理状态枚举
 */
enum class ProcessingStatus {
    Pending,      ///< 待处理
    Processing,   ///< 处理中
    Completed,    ///< 已完成
    Failed,       ///< 失败
    Cancelled     ///< 已取消
};

/**
 * @brief 处理模式枚举
 */
enum class ProcessingMode {
    Shader,       ///< Shader 滤镜模式
    AIInference,  ///< AI 推理模式
    Browse        ///< 浏览模式
};

/**
 * @brief AI 模型类别枚举
 */
enum class ModelCategory {
    SuperResolution,      ///< 超分辨率
    Denoising,            ///< 去噪
    Deblurring,           ///< 去模糊
    Dehazing,             ///< 去雾
    Colorization,         ///< 上色
    LowLight,             ///< 低光增强
    FrameInterpolation,   ///< 视频插帧
    Inpainting            ///< 图像修复
};

/**
 * @brief AI 模型信息结构
 */
struct ModelInfo {
    QString id;                ///< 模型唯一标识
    QString name;              ///< 显示名称
    QString description;       ///< 模型描述（中文）
    QString descriptionEn;     ///< 模型描述（英文）
    ModelCategory category;    ///< 模型类别
    QString paramPath;         ///< .param 文件路径
    QString binPath;           ///< .bin 文件路径
    QString inputBlobName;     ///< NCNN 输入节点名
    QString outputBlobName;    ///< NCNN 输出节点名
    int scaleFactor = 1;       ///< 放大倍数（超分专用）
    int inputChannels = 3;     ///< 输入通道数
    int outputChannels = 3;    ///< 输出通道数
    float normMean[3] = {0.f, 0.f, 0.f};   ///< 归一化均值
    float normScale[3] = {1/255.f, 1/255.f, 1/255.f}; ///< 归一化缩放
    float denormScale[3] = {255.f, 255.f, 255.f};      ///< 反归一化缩放
    float denormMean[3] = {0.f, 0.f, 0.f};             ///< 反归一化均值
    int tileSize = 0;          ///< 分块大小（0=不分块）
    int tilePadding = 10;      ///< 分块重叠像素
    qint64 sizeBytes = 0;      ///< 模型文件大小
    bool isAvailable = false;  ///< 文件是否存在
    bool isLoaded = false;     ///< 是否已加载到内存
    int layerCount = 0;        ///< 模型层数（加载后设置，用于动态padding计算）
    QVariantMap supportedParams; ///< 支持的可调参数及范围

    ModelInfo() : category(ModelCategory::SuperResolution) {}
};

/**
 * @brief AI 推理参数结构
 */
struct AIParams {
    QString modelId;           ///< 选择的模型 ID
    ModelCategory category;    ///< 模型类别
    bool useGpu = false;       ///< 是否使用 GPU（向后兼容）
    QString backendType = "NCNN_CPU";  ///< 精确后端类型 ("NCNN_CPU" / "NCNN_Vulkan")
    int tileSize = 0;          ///< 自定义分块大小（0=自动）
    bool autoTileSize = true;  ///< 分块大小是否为自动模式（true=根据图像尺寸自动计算，false=使用 tileSize）
    QVariantMap modelParams;   ///< 模型特定参数（如去噪等级等）

    AIParams() : category(ModelCategory::SuperResolution) {}
};

/**
 * @brief 队列状态枚举
 */
enum class QueueStatus {
    Running,   ///< 运行中
    Paused     ///< 已暂停
};

/**
 * @brief 任务状态枚举（扩展）
 */
enum class TaskState {
    Pending,        ///< 队列等待
    Preparing,      ///< 准备资源
    Running,        ///< 处理中
    Pausing,        ///< 暂停中
    Paused,         ///< 已暂停
    Cancelling,     ///< 取消中
    Cancelled,      ///< 已取消
    Completed,      ///< 已完成
    Failed,         ///< 失败
    Orphaned        ///< 孤儿任务（关联的消息/会话已删除）
};

/**
 * @brief 任务优先级枚举
 */
enum class TaskPriority {
    UserInteractive = 0,  ///< 用户当前关注的会话
    UserInitiated = 1,    ///< 用户主动发起
    Utility = 2,          ///< 后台任务
    Background = 3        ///< 低优先级
};

/**
 * @brief 资源配额结构
 */
struct ResourceQuota {
    int maxConcurrentTasks = 4;      ///< 最大并发任务数
    qint64 maxMemoryMB = 4096;       ///< 最大内存使用（MB）
    qint64 maxGpuMemoryMB = 2048;    ///< 最大 GPU 显存使用（MB）
    int maxTasksPerSession = 100;    ///< 每个会话最大任务数
    int taskTimeoutMs = 300000;      ///< 任务超时时间（毫秒），默认 5 分钟
    int cancelTimeoutMs = 5000;      ///< 取消等待超时（毫秒）
};

/**
 * @brief 任务上下文结构
 */
struct TaskContext {
    QString taskId;                  ///< 任务唯一标识
    QString messageId;               ///< 关联消息 ID
    QString sessionId;               ///< 关联会话 ID
    QString fileId;                  ///< 关联文件 ID
    
    qint64 estimatedMemoryMB = 0;    ///< 预估内存使用（MB）
    qint64 estimatedGpuMemoryMB = 0; ///< 预估 GPU 显存使用（MB）
    
    TaskState state = TaskState::Pending;  ///< 任务状态
    TaskPriority priority = TaskPriority::UserInitiated; ///< 任务优先级
    bool isCancellable = true;       ///< 是否可取消
    bool isPausable = true;          ///< 是否可暂停
    
    std::shared_ptr<std::atomic<bool>> cancelToken;  ///< 取消令牌
    std::shared_ptr<std::atomic<bool>> pauseToken;   ///< 暂停令牌
    
    TaskContext() = default;
    
    TaskContext(const QString& tid, const QString& mid, const QString& sid, const QString& fid)
        : taskId(tid)
        , messageId(mid)
        , sessionId(sid)
        , fileId(fid)
        , cancelToken(std::make_shared<std::atomic<bool>>(false))
        , pauseToken(std::make_shared<std::atomic<bool>>(false))
    {}
};



/**
 * @brief Shader 参数结构
 */
struct ShaderParams {
    float brightness;    ///< 亮度 (-1.0 ~ 1.0)
    float contrast;      ///< 对比度 (0.0 ~ 3.0)
    float saturation;    ///< 饱和度 (0.0 ~ 3.0)
    float sharpness;     ///< 锐度 (0.0 ~ 2.0)
    float blur;          ///< 模糊 (0.0 ~ 1.0)
    float denoise;       ///< 降噪 (0.0 ~ 1.0)
    float hue;           ///< 色相 (-0.5 ~ 0.5)
    float exposure;      ///< 曝光 (-2.0 ~ 2.0)
    float gamma;         ///< 伽马 (0.3 ~ 3.0)
    float temperature;   ///< 色温 (-1.0 ~ 1.0)
    float tint;          ///< 色调 (-1.0 ~ 1.0)
    float vignette;      ///< 晕影 (0.0 ~ 1.0)
    float highlights;    ///< 高光 (-1.0 ~ 1.0)
    float shadows;       ///< 阴影 (-1.0 ~ 1.0)

    ShaderParams()
        : brightness(0.0f)
        , contrast(1.0f)
        , saturation(1.0f)
        , sharpness(0.0f)
        , blur(0.0f)
        , denoise(0.0f)
        , hue(0.0f)
        , exposure(0.0f)
        , gamma(1.0f)
        , temperature(0.0f)
        , tint(0.0f)
        , vignette(0.0f)
        , highlights(0.0f)
        , shadows(0.0f)
    {}
};

/**
 * @brief 媒体文件数据结构
 */
struct MediaFile {
    QString id;              ///< 唯一标识
    QString filePath;        ///< 文件路径
    QString originalPath;    ///< 原始文件路径（处理前的原始文件）
    QString fileName;        ///< 文件名
    qint64 fileSize;         ///< 文件大小（字节）
    MediaType type;          ///< 媒体类型
    QImage thumbnail;        ///< 缩略图
    qint64 duration;         ///< 视频时长（毫秒，仅视频）
    QSize resolution;        ///< 分辨率
    ProcessingStatus status; ///< 处理状态
    QString resultPath;      ///< 处理结果路径

    MediaFile()
        : fileSize(0)
        , type(MediaType::Image)
        , duration(0)
        , status(ProcessingStatus::Pending)
    {}
};

/**
 * @brief 消息数据结构
 */
struct Message {
    QString id;                    ///< 消息唯一标识
    QDateTime timestamp;           ///< 时间戳
    ProcessingMode mode;           ///< 处理模式
    QList<MediaFile> mediaFiles;  ///< 媒体文件列表
    QVariantMap parameters;        ///< 处理参数
    ShaderParams shaderParams;     ///< Shader 参数
    AIParams aiParams;             ///< AI 推理参数
    ProcessingStatus status;       ///< 处理状态
    QString errorMessage;          ///< 错误信息（失败时）
    bool isSelected;               ///< 是否被选中（批量操作）
    int progress;                  ///< 处理进度（0-100）
    int queuePosition;             ///< 队列位置（-1 表示不在队列）
    qint64 actualTotalSec;         ///< 实际总耗时（秒），处理完成后记录

    Message()
        : mode(ProcessingMode::Shader)
        , status(ProcessingStatus::Pending)
        , isSelected(false)
        , progress(0)
        , queuePosition(-1)
        , actualTotalSec(0)
    {}
};

/**
 * @brief 会话数据结构
 */
struct Session {
    QString id;              ///< 会话唯一标识
    QString name;            ///< 会话名称
    QDateTime createdAt;     ///< 创建时间
    QDateTime modifiedAt;    ///< 最后修改时间
    QList<Message> messages; ///< 消息列表
    QList<MediaFile> pendingFiles; ///< 待处理文件列表
    bool isActive;           ///< 是否为当前活动会话
    bool isSelected;         ///< 是否被选中（批量操作模式）
    bool isPinned;           ///< 是否置顶
    bool isProcessing;       ///< 是否有正在处理的消息
    int sortIndex;           ///< 排序索引（用于拖拽排序）

    Session()
        : isActive(false)
        , isSelected(false)
        , isPinned(false)
        , isProcessing(false)
        , sortIndex(0)
    {}
};

/**
 * @brief 队列任务数据结构
 */
struct QueueTask {
    QString taskId;                  ///< 任务唯一标识
    QString messageId;               ///< 关联消息 ID
    QString fileId;                  ///< 关联文件 ID
    int position;                    ///< 队列位置
    int progress;                    ///< 进度 (0-100)
    QDateTime queuedAt;              ///< 入队时间
    QDateTime startedAt;             ///< 开始处理时间
    QDateTime estimatedCompletion;   ///< 预计完成时间
    ProcessingStatus status;         ///< 任务状态

    QueueTask()
        : position(-1)
        , progress(0)
        , status(ProcessingStatus::Pending)
    {}
};

/**
 * @brief 进度回调函数类型
 */
using ProgressCallback = std::function<void(int progress, const QString& status)>;

/**
 * @brief 完成回调函数类型
 */
using FinishCallback = std::function<void(bool success, const QString& resultPath, const QString& error)>;

} // namespace EnhanceVision

#endif // ENHANCEVISION_DATATYPES_H
