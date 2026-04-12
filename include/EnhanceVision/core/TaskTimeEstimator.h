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
#include <QHash>
#include <QDateTime>

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
 * @brief 任务时间跟踪数据结构
 * 用于动态修正预测时间
 */
struct TaskTimeTracker {
    QString taskId;                   ///< 任务ID（messageId）
    double initialPredictedSec = 0.0; ///< 初始预测时间（秒）
    double currentPredictedSec = 0.0; ///< 当前修正后预测时间（秒）
    double elapsedSec = 0.0;          ///< 已用时间（秒）
    double progress = 0.0;            ///< 当前进度 (0.0 ~ 1.0)
    qint64 startTimeMs = 0;           ///< 开始时间戳
    qint64 pausedTimeMs = 0;          ///< 暂停时累计的时间（毫秒）
    qint64 lastPauseStartMs = 0;      ///< 最后一次暂停开始时间
    bool isPaused = false;            ///< 是否暂停
    int correctionCount = 0;          ///< 修正次数
    double correctionFactor = 1.0;    ///< 动态修正系数
    
    TaskTimeTracker() = default;
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

    // ========== 功能3：动态时间跟踪与修正 ==========

    /**
     * @brief 开始跟踪任务时间
     * @param taskId 任务ID（通常是 messageId）
     * @param initialPredictedSec 初始预测时间（秒）
     * @param startTimeMs 开始时间戳（毫秒），0表示使用当前时间
     */
    void startTracking(const QString& taskId, double initialPredictedSec, qint64 startTimeMs = 0);

    /**
     * @brief 更新任务进度并动态修正预测时间
     * @param taskId 任务ID
     * @param progress 当前进度 (0.0 ~ 1.0)
     * @return 修正后的剩余时间（秒），如果任务不存在返回 -1
     */
    Q_INVOKABLE double updateProgress(const QString& taskId, double progress);

    /**
     * @brief 暂停任务计时
     * @param taskId 任务ID
     */
    void pauseTracking(const QString& taskId);

    /**
     * @brief 恢复任务计时
     * @param taskId 任务ID
     */
    void resumeTracking(const QString& taskId);

    /**
     * @brief 停止跟踪任务
     * @param taskId 任务ID
     */
    void stopTracking(const QString& taskId);

    /**
     * @brief 获取任务的当前时间信息
     * @param taskId 任务ID
     * @return QVariantMap 包含 predictedSec, elapsedSec, remainingSec, isPaused, isOvertime
     */
    Q_INVOKABLE QVariantMap getTaskTimeInfo(const QString& taskId) const;

    /**
     * @brief 检查任务是否正在被跟踪
     * @param taskId 任务ID
     * @return 是否正在跟踪
     */
    Q_INVOKABLE bool isTracking(const QString& taskId) const;

    /**
     * @brief 检查任务是否暂停
     * @param taskId 任务ID
     * @return 是否暂停
     */
    Q_INVOKABLE bool isTaskPaused(const QString& taskId) const;

signals:
    void gpuAvailableChanged();
    
    /**
     * @brief 任务时间信息更新信号
     * @param taskId 任务ID
     * @param predictedSec 预测总时间
     * @param elapsedSec 已用时间
     * @param remainingSec 剩余时间（可为负数表示超时）
     * @param isOvertime 是否超时
     */
    void taskTimeUpdated(const QString& taskId, double predictedSec, 
                         double elapsedSec, double remainingSec, bool isOvertime);

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

    /**
     * @brief 计算动态修正后的剩余时间
     * @param tracker 任务跟踪器
     * @return 修正后的剩余时间（秒）
     */
    double calculateCorrectedRemainingTime(TaskTimeTracker& tracker) const;

    /**
     * @brief 获取任务的实际已用时间（排除暂停时间）
     * @param tracker 任务跟踪器
     * @return 实际已用时间（秒）
     */
    double getEffectiveElapsedSec(const TaskTimeTracker& tracker) const;

    static TaskTimeEstimator* s_instance;
    HardwareInfo m_hwInfo;
    mutable QMutex m_mutex;
    bool m_initialized = false;
    
    // 任务时间跟踪数据
    QHash<QString, TaskTimeTracker> m_trackers;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_TASKTIMEESTIMATOR_H
