/**
 * @file InferenceTypes.h
 * @brief 推理层核心类型定义
 * @author EnhanceVision Team
 * 
 * 定义推理后端的核心类型、枚举和数据结构。
 * 支持多种推理框架和硬件加速的统一抽象。
 */

#ifndef ENHANCEVISION_INFERENCETYPES_H
#define ENHANCEVISION_INFERENCETYPES_H

#include <QString>
#include <QVariantMap>
#include <QSize>
#include <QImage>
#include <vector>
#include <memory>

namespace EnhanceVision {

/**
 * @brief 推理后端类型
 */
enum class BackendType {
    Auto,           ///< 自动选择（推荐）
    NCNN_CPU,       ///< NCNN CPU 后端
    NCNN_Vulkan,    ///< NCNN Vulkan GPU 后端
    ONNX_CPU,       ///< ONNX Runtime CPU
    ONNX_GPU,       ///< ONNX Runtime GPU (CUDA/DirectML)
    TensorRT,       ///< NVIDIA TensorRT
    CoreML,         ///< Apple CoreML
    Custom          ///< 自定义后端
};

/**
 * @brief 推理后端能力标志
 */
struct BackendCapabilities {
    bool supportsGPU = false;           ///< 支持 GPU 加速
    bool supportsHalfPrecision = false; ///< 支持半精度 (FP16)
    bool supportsInt8 = false;          ///< 支持 INT8 量化
    bool supportsDynamicBatch = false;  ///< 支持动态批处理
    bool supportsDynamicInput = false;  ///< 支持动态输入尺寸
    int maxBatchSize = 1;               ///< 最大批处理大小
    qint64 maxMemoryMB = 1024;          ///< 最大可用内存 (MB)
    QString deviceName;                 ///< 设备名称
    QString driverVersion;              ///< 驱动版本
};

/**
 * @brief 后端配置
 */
struct BackendConfig {
    BackendType type = BackendType::Auto;
    int numThreads = 0;                 ///< 线程数 (0 = 自动)
    bool enableHalfPrecision = false;   ///< 启用半精度
    bool enableInt8 = false;            ///< 启用 INT8
    int gpuDeviceId = 0;                ///< GPU 设备 ID
    qint64 memoryLimitMB = 0;           ///< 内存限制 (0 = 无限制)
    QVariantMap extraParams;            ///< 额外参数
};

/**
 * @brief 推理请求
 */
struct InferenceRequest {
    QImage input;                       ///< 输入图像
    QVariantMap params;                 ///< 推理参数
    int tileSize = 0;                   ///< 分块大小 (0 = 不分块)
    int tilePadding = 0;                ///< 分块填充
    bool ttaMode = false;               ///< TTA 模式
    double outscale = 1.0;              ///< 输出缩放
    
    InferenceRequest() = default;
    
    explicit InferenceRequest(const QImage& img) 
        : input(img) {}
};

/**
 * @brief 推理结果
 */
struct InferenceResult {
    bool success = false;               ///< 是否成功
    QImage output;                      ///< 输出图像
    QString errorMessage;               ///< 错误信息
    int errorCode = 0;                  ///< 错误代码
    qint64 inferenceTimeMs = 0;         ///< 推理耗时 (ms)
    qint64 memoryUsedMB = 0;            ///< 内存使用 (MB)
    
    static InferenceResult makeSuccess(const QImage& output, qint64 timeMs = 0) {
        InferenceResult r;
        r.success = true;
        r.output = output;
        r.inferenceTimeMs = timeMs;
        return r;
    }
    
    static InferenceResult makeFailure(const QString& error, int code = -1) {
        InferenceResult r;
        r.success = false;
        r.errorMessage = error;
        r.errorCode = code;
        return r;
    }
};

/**
 * @brief 分块信息
 */
struct TileInfo {
    int x = 0;                          ///< 起始 X 坐标
    int y = 0;                          ///< 起始 Y 坐标
    int width = 0;                      ///< 宽度
    int height = 0;                     ///< 高度
    int padding = 0;                    ///< 填充
    int index = 0;                      ///< 分块索引
    int total = 1;                      ///< 总分块数
};

/**
 * @brief 预处理配置
 */
struct PreprocessConfig {
    bool normalize = true;              ///< 是否归一化
    std::vector<float> mean = {0.5f, 0.5f, 0.5f};   ///< 均值
    std::vector<float> scale = {0.5f, 0.5f, 0.5f};  ///< 缩放
    bool swapRB = false;                ///< 交换 R/B 通道
    int targetWidth = 0;                ///< 目标宽度 (0 = 保持)
    int targetHeight = 0;               ///< 目标高度 (0 = 保持)
};

/**
 * @brief 后处理配置
 */
struct PostprocessConfig {
    bool denormalize = true;            ///< 是否反归一化
    std::vector<float> mean = {0.5f, 0.5f, 0.5f};   ///< 均值
    std::vector<float> scale = {0.5f, 0.5f, 0.5f};  ///< 缩放
    bool swapRB = false;                ///< 交换 R/B 通道
    bool clampValues = true;            ///< 限制值范围 [0, 255]
    double outscale = 1.0;              ///< 输出缩放
};

/**
 * @brief 推理错误类型
 */
enum class InferenceError {
    None = 0,
    ModelNotLoaded,
    InvalidInput,
    InvalidModel,
    MemoryAllocation,
    InferenceFailed,
    BackendNotInitialized,
    BackendNotSupported,
    GPUNotAvailable,
    OutOfMemory,
    Timeout,
    Cancelled,
    Unknown
};

/**
 * @brief 获取后端类型名称
 */
inline QString backendTypeName(BackendType type) {
    switch (type) {
        case BackendType::Auto: return QStringLiteral("Auto");
        case BackendType::NCNN_CPU: return QStringLiteral("NCNN_CPU");
        case BackendType::NCNN_Vulkan: return QStringLiteral("NCNN_Vulkan");
        case BackendType::ONNX_CPU: return QStringLiteral("ONNX_CPU");
        case BackendType::ONNX_GPU: return QStringLiteral("ONNX_GPU");
        case BackendType::TensorRT: return QStringLiteral("TensorRT");
        case BackendType::CoreML: return QStringLiteral("CoreML");
        case BackendType::Custom: return QStringLiteral("Custom");
        default: return QStringLiteral("Unknown");
    }
}

/**
 * @brief 获取推理错误描述
 */
inline QString inferenceErrorString(InferenceError error) {
    switch (error) {
        case InferenceError::None: return QStringLiteral("无错误");
        case InferenceError::ModelNotLoaded: return QStringLiteral("模型未加载");
        case InferenceError::InvalidInput: return QStringLiteral("无效输入");
        case InferenceError::InvalidModel: return QStringLiteral("无效模型");
        case InferenceError::MemoryAllocation: return QStringLiteral("内存分配失败");
        case InferenceError::InferenceFailed: return QStringLiteral("推理失败");
        case InferenceError::BackendNotInitialized: return QStringLiteral("后端未初始化");
        case InferenceError::BackendNotSupported: return QStringLiteral("后端不支持");
        case InferenceError::GPUNotAvailable: return QStringLiteral("GPU 不可用");
        case InferenceError::OutOfMemory: return QStringLiteral("内存不足");
        case InferenceError::Timeout: return QStringLiteral("超时");
        case InferenceError::Cancelled: return QStringLiteral("已取消");
        case InferenceError::Unknown: 
        default: return QStringLiteral("未知错误");
    }
}

} // namespace EnhanceVision

Q_DECLARE_METATYPE(EnhanceVision::BackendType)
Q_DECLARE_METATYPE(EnhanceVision::BackendCapabilities)
Q_DECLARE_METATYPE(EnhanceVision::BackendConfig)
Q_DECLARE_METATYPE(EnhanceVision::InferenceRequest)
Q_DECLARE_METATYPE(EnhanceVision::InferenceResult)
Q_DECLARE_METATYPE(EnhanceVision::InferenceError)

#endif // ENHANCEVISION_INFERENCETYPES_H
