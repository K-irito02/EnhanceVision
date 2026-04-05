/**
 * @file TaskTimeEstimator.h
 * @brief 任务时间预测器 - 硬件检测、性能估算、模型性能提示
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_TASKTIMEESTIMATOR_H
#define ENHANCEVISION_TASKTIMEESTIMATOR_H

#include <QObject>
#include <QString>
#include <QSize>
#include <QVariantMap>
#include <QVariantList>
#include <QMutex>

namespace EnhanceVision {

/**
 * @brief 硬件信息结构
 */
struct HardwareInfo {
    // CPU
    QString cpuModel;
    int cpuCoreCount = 1;
    int cpuThreadCount = 1;
    double cpuFrequencyGHz = 2.0;

    // GPU
    QString gpuModel;
    qint64 gpuMemoryMB = 0;
    bool gpuAvailable = false;

    // 系统
    qint64 systemMemoryMB = 0;
};

/**
 * @brief 单个文件的预估耗时结构
 */
struct FileTimeEstimate {
    QString fileId;
    double estimatedSeconds = 0.0;  ///< 预估耗时（秒）
    bool isVideo = false;
    int width = 0;
    int height = 0;
    double durationSec = 0.0;       ///< 视频时长（秒）
};

/**
 * @brief 任务时间预测器
 *
 * 单例类，负责：
 * 1. 应用启动时检测硬件信息（CPU/GPU）
 * 2. 为 AI 模型提供性能提示数据（悬停提示）
 * 3. 为消息卡片计算任务总预估时间
 */
class TaskTimeEstimator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString cpuModel READ cpuModel CONSTANT)
    Q_PROPERTY(int cpuCoreCount READ cpuCoreCount CONSTANT)
    Q_PROPERTY(QString gpuModel READ gpuModel CONSTANT)
    Q_PROPERTY(qint64 gpuMemoryMB READ gpuMemoryMB CONSTANT)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY gpuAvailableChanged)

public:
    static TaskTimeEstimator* instance();

    /**
     * @brief 初始化，检测硬件信息
     */
    void initialize();

    // ========== 硬件信息查询 ==========
    QString cpuModel() const;
    int cpuCoreCount() const;
    QString gpuModel() const;
    qint64 gpuMemoryMB() const;
    bool gpuAvailable() const;
    void setGpuInfo(const QString& gpuModel, qint64 gpuMemoryMB, bool available);

    // ========== 功能1：模型性能提示 ==========

    /**
     * @brief 获取模型性能提示数据（供 QML 悬停提示使用）
     * @param modelId 模型 ID
     * @return QVariantMap 包含:
     *   cpuImageSec: CPU处理1920x1080图片耗时（秒）
     *   cpuVideoSec: CPU处理10秒视频耗时（秒）
     *   gpuImageSec: GPU处理1920x1080图片耗时（秒）
     *   gpuVideoSec: GPU处理10秒视频耗时（秒）
     */
    Q_INVOKABLE QVariantMap getModelPerformanceHint(const QString& modelId) const;

    // ========== 功能2：任务时间预测 ==========

    /**
     * @brief 估算单个文件的处理时间
     * @param mode 处理模式（0=Shader, 1=AI推理）
     * @param useGpu 是否使用GPU（AI模式）
     * @param modelId AI模型ID（AI模式）
     * @param width 图片/视频宽度
     * @param height 图片/视频高度
     * @param isVideo 是否为视频
     * @param durationMs 视频时长（毫秒）
     * @param fps 视频帧率
     * @return 预估秒数
     */
    Q_INVOKABLE double estimateFileTime(int mode, bool useGpu,
                                         const QString& modelId,
                                         int width, int height,
                                         bool isVideo, qint64 durationMs,
                                         double fps = 30.0) const;

    /**
     * @brief 估算消息中所有文件的总处理时间
     * @param mode 处理模式
     * @param useGpu 是否使用GPU
     * @param modelId AI模型ID
     * @param files 文件列表 [{width, height, isVideo, durationMs, fps}]
     * @return 总预估秒数
     */
    Q_INVOKABLE double estimateMessageTotalTime(int mode, bool useGpu,
                                                 const QString& modelId,
                                                 const QVariantList& files) const;

    /**
     * @brief 估算Shader模式的文件处理时间
     */
    double estimateShaderTime(int width, int height, bool isVideo,
                              double durationSec, double fps) const;

    /**
     * @brief 估算AI推理模式的文件处理时间
     */
    double estimateAITime(const QString& modelId, bool useGpu,
                          int width, int height, bool isVideo,
                          double durationSec, double fps) const;

signals:
    void gpuAvailableChanged();

private:
    explicit TaskTimeEstimator(QObject* parent = nullptr);
    ~TaskTimeEstimator() override = default;

    void detectCpuInfo();
    void detectSystemMemory();

    /**
     * @brief 计算分块数量
     */
    int calculateTileCount(int width, int height, int tileSize) const;

    /**
     * @brief 获取硬件校准系数
     * CPU 基准：8核 3.0GHz 系数为 1.0
     * GPU 基准：RTX 3060 (12GB) 系数为 1.0
     */
    double getCpuCalibrationFactor() const;
    double getGpuCalibrationFactor() const;

    /**
     * @brief AI推理核心耗时参数（秒）
     * 基于 Analyze-Inference.md 的数据，针对 RealESRGAN 模型
     */
    struct AITimingParams {
        double cpuSingleTileSec = 5.0;    ///< CPU 单块推理耗时（秒），基准值
        double gpuSingleTileSec = 0.02;   ///< GPU 单块推理耗时（秒），基准值
        double cpuOverheadSec = 2.0;      ///< CPU 预/后处理开销（秒）
        double gpuOverheadSec = 1.5;      ///< GPU 预/后处理 + Vulkan初始化开销（秒）
        double videoFrameOverheadSec = 0.05; ///< 视频每帧编解码开销（秒）
        double videoSetupSec = 1.0;       ///< 视频初始化/收尾开销（秒）
    };

    AITimingParams getTimingParams(const QString& modelId) const;

    static TaskTimeEstimator* s_instance;
    HardwareInfo m_hwInfo;
    mutable QMutex m_mutex;
    bool m_initialized = false;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_TASKTIMEESTIMATOR_H
