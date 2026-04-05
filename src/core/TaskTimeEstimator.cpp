/**
 * @file TaskTimeEstimator.cpp
 * @brief 任务时间预测器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/TaskTimeEstimator.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include <QThread>
#include <QSysInfo>
#include <QtMath>
#include <QOperatingSystemVersion>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace EnhanceVision {

TaskTimeEstimator* TaskTimeEstimator::s_instance = nullptr;

TaskTimeEstimator::TaskTimeEstimator(QObject* parent)
    : QObject(parent)
{
}

TaskTimeEstimator* TaskTimeEstimator::instance()
{
    if (!s_instance) {
        s_instance = new TaskTimeEstimator();
    }
    return s_instance;
}

void TaskTimeEstimator::initialize()
{
    QMutexLocker locker(&m_mutex);
    if (m_initialized) return;

    detectCpuInfo();
    detectSystemMemory();
    // GPU 信息由 AIEngine::probeGpuAvailability() 完成后通过 setGpuInfo() 设置
    m_initialized = true;

    qInfo() << "[TaskTimeEstimator] Initialized:"
            << "CPU:" << m_hwInfo.cpuModel
            << "cores:" << m_hwInfo.cpuCoreCount
            << "threads:" << m_hwInfo.cpuThreadCount
            << "freq:" << m_hwInfo.cpuFrequencyGHz << "GHz"
            << "RAM:" << m_hwInfo.systemMemoryMB << "MB"
            << "GPU:" << (m_hwInfo.gpuAvailable ? m_hwInfo.gpuModel : "N/A")
            << "VRAM:" << m_hwInfo.gpuMemoryMB << "MB";
}

// ========== 硬件信息查询 ==========

QString TaskTimeEstimator::cpuModel() const { return m_hwInfo.cpuModel; }
int TaskTimeEstimator::cpuCoreCount() const { return m_hwInfo.cpuCoreCount; }
QString TaskTimeEstimator::gpuModel() const { return m_hwInfo.gpuModel; }
qint64 TaskTimeEstimator::gpuMemoryMB() const { return m_hwInfo.gpuMemoryMB; }
bool TaskTimeEstimator::gpuAvailable() const { return m_hwInfo.gpuAvailable; }

void TaskTimeEstimator::setGpuInfo(const QString& gpuModel, qint64 gpuMemoryMB, bool available)
{
    QMutexLocker locker(&m_mutex);
    m_hwInfo.gpuModel = gpuModel;
    m_hwInfo.gpuMemoryMB = gpuMemoryMB;
    m_hwInfo.gpuAvailable = available;

    qInfo() << "[TaskTimeEstimator] GPU info updated:"
            << "model:" << gpuModel
            << "VRAM:" << gpuMemoryMB << "MB"
            << "available:" << available;

    emit gpuAvailableChanged();
}

// ========== 硬件检测 ==========

void TaskTimeEstimator::detectCpuInfo()
{
    m_hwInfo.cpuCoreCount = QThread::idealThreadCount();
    if (m_hwInfo.cpuCoreCount <= 0) {
        m_hwInfo.cpuCoreCount = 1;
    }
    m_hwInfo.cpuThreadCount = m_hwInfo.cpuCoreCount;

#ifdef Q_OS_WIN
    // 从注册表获取 CPU 型号和频率
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t cpuName[256] = {0};
        DWORD bufSize = sizeof(cpuName);
        if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(cpuName), &bufSize) == ERROR_SUCCESS) {
            m_hwInfo.cpuModel = QString::fromWCharArray(cpuName).trimmed();
        }

        DWORD mhz = 0;
        bufSize = sizeof(mhz);
        if (RegQueryValueExW(hKey, L"~MHz", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&mhz), &bufSize) == ERROR_SUCCESS) {
            m_hwInfo.cpuFrequencyGHz = mhz / 1000.0;
        }

        RegCloseKey(hKey);
    }

    // 获取逻辑处理器数量（包括超线程）
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_hwInfo.cpuThreadCount = static_cast<int>(sysInfo.dwNumberOfProcessors);
    // 物理核心数约为逻辑核心数的一半（超线程情况下）
    if (m_hwInfo.cpuThreadCount > m_hwInfo.cpuCoreCount) {
        m_hwInfo.cpuCoreCount = m_hwInfo.cpuThreadCount / 2;
        if (m_hwInfo.cpuCoreCount < 1) m_hwInfo.cpuCoreCount = 1;
    }
#else
    m_hwInfo.cpuModel = QSysInfo::currentCpuArchitecture();
    m_hwInfo.cpuFrequencyGHz = 2.5; // 默认估算
#endif

    if (m_hwInfo.cpuModel.isEmpty()) {
        m_hwInfo.cpuModel = QStringLiteral("Unknown CPU");
    }
    if (m_hwInfo.cpuFrequencyGHz <= 0) {
        m_hwInfo.cpuFrequencyGHz = 2.0;
    }
}

void TaskTimeEstimator::detectSystemMemory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        m_hwInfo.systemMemoryMB = static_cast<qint64>(memInfo.ullTotalPhys / (1024 * 1024));
    }
#else
    m_hwInfo.systemMemoryMB = 8192; // 默认 8GB
#endif
}

// ========== 硬件校准系数 ==========

double TaskTimeEstimator::getCpuCalibrationFactor() const
{
    // 基准：12线程 3.3GHz (Ryzen 5 5600H) = 1.0
    // NCNN 使用多线程，线程数更重要
    const int threads = qMax(1, m_hwInfo.cpuThreadCount > 0 ? m_hwInfo.cpuThreadCount : m_hwInfo.cpuCoreCount);
    const double threadRatio = 12.0 / threads;
    const double freqRatio = 3.3 / qMax(0.5, m_hwInfo.cpuFrequencyGHz);
    // 线程数影响 60%，频率影响 40%
    double factor = (threadRatio * 0.6 + freqRatio * 0.4);
    return qBound(0.3, factor, 5.0);
}

double TaskTimeEstimator::getGpuCalibrationFactor() const
{
    if (!m_hwInfo.gpuAvailable || m_hwInfo.gpuMemoryMB <= 0) {
        return 1.0;
    }
    // 基准：4GB VRAM (RTX 3050 Laptop) = 1.0
    // 显存影响较小，主要是 GPU 计算能力
    // RTX 3050 Laptop ~= 1.0, RTX 3060 ~= 0.7, RTX 3080 ~= 0.4
    const double memRatio = 4096.0 / qMax(static_cast<qint64>(2048), m_hwInfo.gpuMemoryMB);
    return qBound(0.3, qSqrt(memRatio), 2.5);
}

// ========== AI 耗时参数 ==========

TaskTimeEstimator::AITimingParams TaskTimeEstimator::getTimingParams(const QString& modelId) const
{
    AITimingParams params;

    // 基于 Analyze-Inference.md 文档的基准数据
    // 测试环境：Ryzen 5 5600H + RTX 3050 Laptop (4GB)
    // 
    // CPU 1920x1080: ~1-5min (平均 180s), tileSize=160 → 12x7=84块
    //   → 每块约 180s / 84 = 2.14s
    // GPU 1920x1080: ~0.5-5s (平均 2.5s), 84块
    //   → 每块约 2.5s / 84 = 0.03s
    //
    // 视频处理 (CPU): 10s@30fps=300帧, 预计 25-300min
    //   → 每帧约 5-60s
    // 视频处理 (GPU): 60s@30fps=1800帧, 预计 3-60min
    //   → 每帧约 0.1-2s
    
    QString lowerModelId = modelId.toLower();
    
    // 基于 Analyze-Inference.md 文档的实际基准数据
    // 测试环境：Ryzen 5 5600H 12线程 + RTX 3050 Laptop 4GB
    //
    // 图片处理 (1920x1080, tileSize=160, 84块):
    //   CPU: ~1-5min (60-300s), 平均 ~90s → 每块 ~1.07s
    //   GPU: ~0.5-5s, 平均 ~2s → 每块 ~0.024s
    //
    // 视频处理 (10s@30fps = 300帧):
    //   CPU: ~25-300min → 每帧 ~5-60s
    //   GPU: ~0.5-10min (30-600s) → 每帧 ~0.1-2s, 平均 ~0.5s
    
    if (lowerModelId.contains("realesr") || lowerModelId.contains("animevideo")) {
        // RealESRGAN AnimeVideo V3 (4x) - 主要模型
        params.cpuSingleTileSec = 1.0;    // ~90s / 84块 ≈ 1.07s
        params.gpuSingleTileSec = 0.024;  // ~2s / 84块 ≈ 0.024s
        params.cpuOverheadSec = 2.0;      // 模型加载 + 预处理
        params.gpuOverheadSec = 1.0;      // Vulkan 初始化 + 模型加载
        params.videoFrameOverheadSec = 0.03;  // 视频编解码开销 (解码+编码)
        params.videoSetupSec = 1.0;       // 视频初始化
    } else if (lowerModelId.contains("anime") || lowerModelId.contains("waifu")) {
        // 其他动漫模型
        params.cpuSingleTileSec = 0.9;
        params.gpuSingleTileSec = 0.022;
        params.cpuOverheadSec = 1.5;
        params.gpuOverheadSec = 0.8;
        params.videoFrameOverheadSec = 0.03;
        params.videoSetupSec = 0.8;
    } else if (lowerModelId.contains("photo") || lowerModelId.contains("general")) {
        // 通用/照片模型
        params.cpuSingleTileSec = 1.1;
        params.gpuSingleTileSec = 0.026;
        params.cpuOverheadSec = 2.0;
        params.gpuOverheadSec = 1.0;
        params.videoFrameOverheadSec = 0.03;
        params.videoSetupSec = 1.0;
    } else {
        // 默认参数
        params.cpuSingleTileSec = 1.0;
        params.gpuSingleTileSec = 0.024;
        params.cpuOverheadSec = 2.0;
        params.gpuOverheadSec = 1.0;
        params.videoFrameOverheadSec = 0.03;
        params.videoSetupSec = 1.0;
    }

    return params;
}

// ========== 分块计算 ==========

int TaskTimeEstimator::calculateTileCount(int width, int height, int tileSize) const
{
    if (tileSize <= 0) {
        // 自动分块大小：根据图像尺寸动态计算
        // 参考 AutoParamCalculator 逻辑
        const qint64 pixels = static_cast<qint64>(width) * height;
        if (pixels <= 640 * 480) {
            tileSize = qMax(width, height); // 不分块
        } else if (pixels <= 1280 * 720) {
            tileSize = 200;
        } else {
            tileSize = 160;
        }
    }

    if (tileSize >= qMax(width, height)) {
        return 1; // 不需要分块
    }

    const int tilesX = qCeil(static_cast<double>(width) / tileSize);
    const int tilesY = qCeil(static_cast<double>(height) / tileSize);
    return qMax(1, tilesX * tilesY);
}

// ========== 功能1：模型性能提示 ==========

QVariantMap TaskTimeEstimator::getModelPerformanceHint(const QString& modelId) const
{
    QMutexLocker locker(&m_mutex);
    QVariantMap result;

    // 标准参考：1920x1080 图片 + 10秒30fps视频
    const int refWidth = 1920;
    const int refHeight = 1080;
    const double refVideoDurationSec = 10.0;
    const double refFps = 30.0;

    const double cpuImageSec = estimateAITime(modelId, false, refWidth, refHeight,
                                               false, 0, 0);
    const double cpuVideoSec = estimateAITime(modelId, false, refWidth, refHeight,
                                               true, refVideoDurationSec, refFps);
    const double gpuImageSec = estimateAITime(modelId, true, refWidth, refHeight,
                                               false, 0, 0);
    const double gpuVideoSec = estimateAITime(modelId, true, refWidth, refHeight,
                                               true, refVideoDurationSec, refFps);

    result["cpuImageSec"] = QString::number(cpuImageSec, 'f', 2).toDouble();
    result["cpuVideoSec"] = QString::number(cpuVideoSec, 'f', 2).toDouble();
    result["gpuImageSec"] = QString::number(gpuImageSec, 'f', 2).toDouble();
    result["gpuVideoSec"] = QString::number(gpuVideoSec, 'f', 2).toDouble();

    return result;
}

// ========== 功能2：任务时间预测 ==========

double TaskTimeEstimator::estimateFileTime(int mode, bool useGpu,
                                            const QString& modelId,
                                            int width, int height,
                                            bool isVideo, qint64 durationMs,
                                            double fps) const
{
    const double durationSec = durationMs / 1000.0;

    if (mode == 0) {
        // Shader 模式
        return estimateShaderTime(width, height, isVideo, durationSec, fps);
    } else {
        // AI 推理模式
        return estimateAITime(modelId, useGpu, width, height, isVideo, durationSec, fps);
    }
}

double TaskTimeEstimator::estimateMessageTotalTime(int mode, bool useGpu,
                                                    const QString& modelId,
                                                    const QVariantList& files) const
{
    double totalSec = 0.0;
    for (const QVariant& fileVar : files) {
        QVariantMap file = fileVar.toMap();
        const int width = file.value("width", 1920).toInt();
        const int height = file.value("height", 1080).toInt();
        const bool isVideo = file.value("isVideo", false).toBool();
        const qint64 durationMs = file.value("durationMs", 0).toLongLong();
        const double fps = file.value("fps", 30.0).toDouble();

        totalSec += estimateFileTime(mode, useGpu, modelId, width, height,
                                     isVideo, durationMs, fps);
    }
    return totalSec;
}

// ========== Shader 耗时估算 ==========

double TaskTimeEstimator::estimateShaderTime(int width, int height, bool isVideo,
                                              double durationSec, double fps) const
{
    // 基于 Analyze-Inference.md:
    // Shader 图片：加载~30ms + 预处理~12ms + 色彩调整~125ms + 效果处理~300ms + 保存~60ms ≈ 0.5s
    // Shader 视频：单帧 ~25-100ms × 帧数 + 初始化~500ms + 收尾~300ms

    const qint64 pixels = static_cast<qint64>(width) * height;
    const double pixelFactor = pixels / (1920.0 * 1080.0); // 相对于1080p的像素比

    if (!isVideo) {
        // 图片处理：基础 0.5s，按像素比例缩放
        return qMax(0.1, 0.5 * qSqrt(pixelFactor));
    }

    // 视频处理
    if (durationSec <= 0 || fps <= 0) {
        durationSec = 10.0;
        fps = 30.0;
    }
    const int frameCount = qMax(1, static_cast<int>(durationSec * fps));
    const double perFrameSec = 0.06 * qSqrt(pixelFactor); // ~60ms/帧 @ 1080p
    const double setupSec = 0.8;

    return setupSec + frameCount * perFrameSec;
}

// ========== AI 推理耗时估算 ==========

double TaskTimeEstimator::estimateAITime(const QString& modelId, bool useGpu,
                                          int width, int height, bool isVideo,
                                          double durationSec, double fps) const
{
    const AITimingParams timing = getTimingParams(modelId);

    // 获取模型分块大小
    int tileSize = 160; // 默认
    ModelRegistry* registry = ModelRegistry::instance();
    if (registry && registry->hasModel(modelId)) {
        ModelInfo info = registry->getModelInfo(modelId);
        if (info.tileSize > 0) {
            tileSize = info.tileSize;
        }
    }

    const int tileCount = calculateTileCount(width, height, tileSize);

    // 硬件校准
    const double calibration = useGpu ? getGpuCalibrationFactor() : getCpuCalibrationFactor();

    if (!isVideo) {
        // 图片推理
        const double tileSec = useGpu ? timing.gpuSingleTileSec : timing.cpuSingleTileSec;
        const double overhead = useGpu ? timing.gpuOverheadSec : timing.cpuOverheadSec;
        return (tileSec * tileCount * calibration) + overhead;
    }

    // 视频推理
    if (durationSec <= 0 || fps <= 0) {
        durationSec = 10.0;
        fps = 30.0;
    }
    const int frameCount = qMax(1, static_cast<int>(durationSec * fps));

    // 每帧耗时 = 分块推理 + 编解码开销
    const double tileSec = useGpu ? timing.gpuSingleTileSec : timing.cpuSingleTileSec;
    const double perFrameSec = (tileSec * tileCount * calibration) + timing.videoFrameOverheadSec;

    // 总耗时 = 初始化 + 帧处理 + 收尾
    const double overhead = useGpu ? timing.gpuOverheadSec : timing.cpuOverheadSec;
    return overhead + (frameCount * perFrameSec) + timing.videoSetupSec;
}

} // namespace EnhanceVision
