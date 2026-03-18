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
#include <functional>

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
 * @brief 队列状态枚举
 */
enum class QueueStatus {
    Running,   ///< 运行中
    Paused     ///< 已暂停
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
    ProcessingStatus status;       ///< 处理状态
    QString errorMessage;          ///< 错误信息（失败时）
    bool isSelected;               ///< 是否被选中（批量操作）
    int progress;                  ///< 处理进度（0-100）
    int queuePosition;             ///< 队列位置（-1 表示不在队列）

    Message()
        : mode(ProcessingMode::Shader)
        , status(ProcessingStatus::Pending)
        , isSelected(false)
        , progress(0)
        , queuePosition(-1)
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
    int sortIndex;           ///< 排序索引（用于拖拽排序）

    Session()
        : isActive(false)
        , isSelected(false)
        , isPinned(false)
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
