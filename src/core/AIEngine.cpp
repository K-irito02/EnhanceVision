/**
 * @file AIEngine.cpp
 * @brief NCNN AI 推理引擎实现（ CPU/Vulkan 双模式）
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include "EnhanceVision/core/ProgressReporter.h"
#include "EnhanceVision/core/TaskTimeEstimator.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QtConcurrent/QtConcurrent>
#include <QDateTime>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <functional>

#ifdef NCNN_VULKAN_AVAILABLE
#include <gpu.h>
#endif

namespace EnhanceVision {

// ========== Vulkan 实例单例化静态成员定义 ==========
std::once_flag AIEngine::s_gpuInstanceOnceFlag;
std::atomic<int> AIEngine::s_gpuInstanceRefCount{0};
std::mutex AIEngine::s_gpuInstanceMutex;
std::atomic<bool> AIEngine::s_gpuInstanceCreated{false};

AIEngine::AIEngine(QObject *parent)
    : QObject(parent)
    , m_progressReporter(std::make_unique<ProgressReporter>(this))
{
    qInfo() << "[AIEngine] Initializing AI engine (CPU/Vulkan dual-mode)";

    m_backendType = BackendType::NCNN_CPU;
    m_opt.num_threads = 1;
    m_opt.use_packing_layout = false;
    m_opt.use_vulkan_compute = false;
    m_opt.lightmode = false;
    m_opt.use_local_pool_allocator = false;
    m_opt.use_sgemm_convolution = false;
    m_opt.use_winograd_convolution = false;

    qInfo() << "[AIEngine] Engine created, default mode: CPU (safe configuration)";

    QTimer::singleShot(0, this, &AIEngine::probeGpuAvailability);
}

AIEngine::~AIEngine()
{
    shutdownVulkan();
    unloadModel();
    releaseGpuInstanceRef();
}

// ========== 启动时 GPU 探测 ==========

void AIEngine::probeGpuAvailability()
{
#ifdef NCNN_VULKAN_AVAILABLE
    try {
        qInfo() << "[AIEngine] Probing Vulkan GPU availability at startup...";
        
        ensureGpuInstanceCreated();
        
        if (!s_gpuInstanceCreated.load()) {
            qInfo() << "[AIEngine] Vulkan GPU not available (create_gpu_instance failed)";
            m_gpuAvailable.store(false);
            emit gpuDeviceInfoChanged();
            return;
        }

        int deviceCount = ncnn::get_gpu_count();
        if (deviceCount <= 0) {
            qInfo() << "[AIEngine] No Vulkan GPU devices found";
            m_gpuAvailable.store(false);
            emit gpuDeviceInfoChanged();
            return;
        }

        const ncnn::GpuInfo& gpuInfo = ncnn::get_gpu_info(0);
        m_gpuDeviceName = QString::fromUtf8(gpuInfo.device_name());
        m_gpuDeviceId = 0;
        m_gpuAvailable.store(true);

        qInfo() << "[AIEngine] Vulkan GPU available at startup:"
                << "device:" << m_gpuDeviceName
                << "count:" << deviceCount;

        // 同步 GPU 信息到 TaskTimeEstimator
        {
            auto* estimator = TaskTimeEstimator::instance();
            if (estimator) {
                // GPU VRAM 估算：通过 heap budget 获取（如不可用则使用默认值）
                qint64 vramMB = 0;
                const ncnn::GpuInfo& info = ncnn::get_gpu_info(0);
                vramMB = static_cast<qint64>(info.max_shared_memory_size() / (1024 * 1024));
                if (vramMB <= 0) vramMB = 4096; // 默认 4GB
                estimator->setGpuInfo(m_gpuDeviceName, vramMB, true);
            }
        }

        emit gpuDeviceInfoChanged();
    } catch (const std::exception& e) {
        qWarning() << "[AIEngine] Vulkan probe exception:" << e.what();
        m_gpuAvailable.store(false);
        emit gpuDeviceInfoChanged();
    } catch (...) {
        qWarning() << "[AIEngine] Vulkan probe unknown exception";
        m_gpuAvailable.store(false);
        emit gpuDeviceInfoChanged();
    }
#else
    qInfo() << "[AIEngine] Vulkan support not compiled in (NCNN_VULKAN_AVAILABLE undefined)";
    m_gpuAvailable.store(false);
    emit gpuDeviceInfoChanged();
#endif
}

// ========== 模型管理 ==========

void AIEngine::setModelRegistry(ModelRegistry *registry)
{
    m_modelRegistry = registry;
}

bool AIEngine::loadModel(const QString &modelId)
{
    QMutexLocker locker(&m_mutex);
    
    qInfo() << "[AIEngine][loadModel] Starting, modelId:" << modelId
            << "backendType:" << static_cast<int>(m_backendType)
            << "isProcessing:" << m_isProcessing.load();
    fflush(stdout);
    
    if (m_isProcessing.load()) {
        qWarning() << "[AIEngine][loadModel] rejected: inference in progress, modelId:" << modelId;
        emit processError(tr("Inference in progress, cannot switch model"));
        return false;
    }

    if (!m_modelRegistry) {
        emit processError(tr("ModelRegistry not initialized"));
        return false;
    }

    if (!m_modelRegistry->hasModel(modelId)) {
        emit processError(tr("Model not registered: %1").arg(modelId));
        return false;
    }

    ModelInfo info = m_modelRegistry->getModelInfo(modelId);

    if (info.paramPath.isEmpty() && info.binPath.isEmpty()) {
        m_currentModelId = modelId;
        m_currentModel = info;
        emit modelLoaded(modelId);
        emit modelChanged();
        return true;
    }

    if (!info.isAvailable) {
        emit processError(tr("Model file unavailable: %1").arg(modelId));
        return false;
    }

    if (m_currentModelId == modelId && m_currentModel.isLoaded) {
        if (m_modelLoadedBackendType != m_backendType) {
            qInfo() << "[AIEngine][loadModel] Model loaded with different backend, forcing reload"
                    << "loadedBackend:" << static_cast<int>(m_modelLoadedBackendType)
                    << "currentBackend:" << static_cast<int>(m_backendType);
        } else {
            qInfo() << "[AIEngine][loadModel] Model already loaded:" << modelId;
            return true;
        }
    }

    qInfo() << "[AIEngine][loadModel] Clearing previous model, oldModelId:" << m_currentModelId;
    fflush(stdout);
    
    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_currentModel.isLoaded = false;
        qInfo() << "[AIEngine][loadModel] Previous model cleared";
        fflush(stdout);
    }

    qInfo() << "[AIEngine][loadModel] Setting net options, use_vulkan_compute:" << m_opt.use_vulkan_compute;
    fflush(stdout);
    
    m_net.opt = m_opt;

#ifdef NCNN_VULKAN_AVAILABLE
    if (m_backendType == BackendType::NCNN_Vulkan && m_gpuAvailable.load() && m_gpuDeviceId >= 0) {
        qInfo() << "[AIEngine][loadModel] Setting Vulkan device:" << m_gpuDeviceId
                << "gpuAvailable:" << m_gpuAvailable.load()
                << "vulkanInstanceCreated:" << m_vulkanInstanceCreated;
        fflush(stdout);
        
        if (!s_gpuInstanceCreated.load()) {
            qWarning() << "[AIEngine][loadModel] Global Vulkan instance not created, cannot use GPU";
            emit processError(tr("Vulkan instance not created, GPU unavailable"));
            return false;
        }
        
        m_net.set_vulkan_device(m_gpuDeviceId);
        qInfo() << "[AIEngine][loadModel] Vulkan device set successfully:" << m_gpuDeviceId;
        fflush(stdout);
    } else {
        qInfo() << "[AIEngine][loadModel] Not using Vulkan: backendType:" << static_cast<int>(m_backendType)
                << "gpuAvailable:" << m_gpuAvailable.load()
                << "gpuDeviceId:" << m_gpuDeviceId;
        fflush(stdout);
    }
#else
    qInfo() << "[AIEngine][loadModel] NCNN_VULKAN_AVAILABLE not defined";
    fflush(stdout);
#endif

    qInfo() << "[AIEngine][loadModel] Loading param file:" << info.paramPath;
    fflush(stdout);
    
    int ret = m_net.load_param(info.paramPath.toStdString().c_str());
    if (ret != 0) {
        qWarning() << "[AIEngine][loadModel] Failed to load param file, ret:" << ret;
        emit processError(tr("Failed to load model parameters: %1").arg(info.paramPath));
        return false;
    }
    
    qInfo() << "[AIEngine][loadModel] Param file loaded, loading bin file:" << info.binPath;
    fflush(stdout);

    ret = m_net.load_model(info.binPath.toStdString().c_str());
    if (ret != 0) {
        qWarning() << "[AIEngine][loadModel] Failed to load bin file, ret:" << ret;
        m_net.clear();
        emit processError(tr("Failed to load model weights: %1").arg(info.binPath));
        return false;
    }

    m_currentModelId = modelId;
    m_currentModel = info;
    m_currentModel.isLoaded = true;
    m_currentModel.layerCount = static_cast<int>(m_net.layers().size());
    m_modelLoadedBackendType = m_backendType;

    qInfo() << "[AIEngine][loadModel] Model loaded successfully:" << modelId
            << "layers:" << m_currentModel.layerCount
            << "backendType:" << static_cast<int>(m_backendType);
    fflush(stdout);

    emit modelLoaded(modelId);
    emit modelChanged();
    return true;
}

void AIEngine::loadModelAsync(const QString &modelId)
{
    QtConcurrent::run([this, modelId]() {
        bool success = loadModel(modelId);
        QString error;
        if (!success) {
            error = tr("Model loading failed: %1").arg(modelId);
        }
        emit modelLoadCompleted(success, modelId, error);
    });
}

void AIEngine::unloadModel()
{
    QMutexLocker locker(&m_mutex);

    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_currentModel.isLoaded = false;
        m_currentModelId.clear();
        emit modelUnloaded();
        emit modelChanged();
    }
}

// ========== 推理接口 ==========

QImage AIEngine::process(const QImage &input)
{
    QMutexLocker inferenceLocker(&m_inferenceMutex);
    
    qInfo() << "[AIEngine] process() called:"
            << "inputSize:" << input.width() << "x" << input.height()
            << "format:" << input.format()
            << "modelId:" << m_currentModelId
            << "modelLoaded:" << m_currentModel.isLoaded;

    if (m_currentModelId.isEmpty() || !m_currentModel.isLoaded) {
        if (!m_currentModelId.isEmpty() && m_currentModel.paramPath.isEmpty()) {
        } else {
            emitError(tr("No model loaded"));
            return QImage();
        }
    }

    if (input.isNull() || input.bits() == nullptr) {
        emitError(tr("Input image is empty or data invalid"));
        return QImage();
    }

    if (input.width() <= 0 || input.height() <= 0) {
        emitError(tr("Invalid input image size: %1x%2").arg(input.width()).arg(input.height()));
        return QImage();
    }

    ModelInfo currentModel;
    QString currentModelId;
    {
        QMutexLocker modelLocker(&m_mutex);
        currentModel = m_currentModel;
        currentModelId = m_currentModelId;
    }

    QVariantMap effectiveParams;
    {
        QMutexLocker paramLocker(&m_paramsMutex);
        effectiveParams = getEffectiveParamsLocked(currentModel);
    }

    int tileSize = 0;
    int autoTileForUI = -1;
    QVariantMap allAutoForUI;

    if (effectiveParams.contains("tileSize") && effectiveParams["tileSize"].toInt() > 0) {
        tileSize = effectiveParams["tileSize"].toInt();
    } else {
        tileSize = computeAutoTileSizeForModel(input.size(), currentModel);
        autoTileForUI = tileSize;
        allAutoForUI = computeAutoParamsForModel(input.size(), false, currentModel, currentModelId);
    }

    double outscale = effectiveParams.value("outscale", currentModel.scaleFactor).toDouble();
    bool ttaMode = effectiveParams.value("tta_mode", false).toBool();

    QImage workInput = input;
    if (tileSize == 0 && currentModel.tileSize > 0) {
        const qint64 pixels = static_cast<qint64>(input.width()) * input.height();
        if (pixels > 1024LL * 1024) {
            tileSize = currentModel.tileSize;
        }
    }

    if (tileSize == 0) {
        const qint64 pixels = static_cast<qint64>(workInput.width()) * workInput.height();
        const qint64 kMaxSafePixels = 2048LL * 2048;
        if (pixels > kMaxSafePixels && currentModel.tileSize > 0) {
            tileSize = currentModel.tileSize;
            qWarning() << "[AIEngine] Safety fallback: tileSize set to" << tileSize
                       << "for oversized input" << workInput.width() << "x" << workInput.height();
        }
    }

    if (tileSize > 0) {
        int minDim = std::min(workInput.width(), workInput.height());
        if (tileSize > minDim) {
            tileSize = 0;
            qInfo() << "[AIEngine] tileSize exceeds image dimension, disabling tiling for"
                    << workInput.width() << "x" << workInput.height();
        }
    }
    
    bool needTile = (tileSize > 0) &&
                    (workInput.width() > tileSize || workInput.height() > tileSize);

    ModelInfo effectiveModel = currentModel;
    if (tileSize > 0) {
        effectiveModel.tileSize = tileSize;
    }

    // processTiled 已修复，移除强制禁用代码

    QImage result;
    if (ttaMode && !needTile) {
        result = processWithTTA(workInput, effectiveModel);
    } else if (needTile) {
        result = processTiled(workInput, effectiveModel);
    } else {
        result = processSingle(workInput, effectiveModel);
    }

    if (!result.isNull() && std::abs(outscale - currentModel.scaleFactor) > 0.01) {
        result = applyOutscale(result, outscale / currentModel.scaleFactor);
    }

    setProgress(1.0);
    setProcessing(false);

    inferenceLocker.unlock();

    if (autoTileForUI >= 0) {
        QMetaObject::invokeMethod(this, [this, autoTileForUI]() {
            emit autoParamsComputed(autoTileForUI);
        }, Qt::QueuedConnection);
    }
    if (!allAutoForUI.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, allAutoForUI]() {
            emit allAutoParamsComputed(allAutoForUI);
        }, Qt::QueuedConnection);
    }

    if (!result.isNull()) {
        QMetaObject::invokeMethod(this, [this, result]() {
            emit processCompleted(result);
        }, Qt::QueuedConnection);
    } else {
        qWarning() << "[AIEngine] Processing failed or returned empty image";
    }

    return result;
}

void AIEngine::processAsync(const QString &inputPath, const QString &outputPath)
{
    if (m_isProcessing.load()) {
        emit processError(tr("Inference task already in progress"));
        emit processFileCompleted(false, QString(), tr("Inference task already in progress"));
        return;
    }

    ModelInfo snapshotModel;
    QString snapshotModelId;
    QVariantMap snapshotParams;
    {
        QMutexLocker modelLocker(&m_mutex);
        snapshotModel   = m_currentModel;
        snapshotModelId = m_currentModelId;
    }
    {
        QMutexLocker paramLocker(&m_paramsMutex);
        snapshotParams = getEffectiveParamsLocked(snapshotModel);
    }

    setProcessing(true);
    setProgress(0.0);
    m_cancelRequested = false;

    QtConcurrent::run([this, inputPath, outputPath, snapshotParams]() {
        QElapsedTimer perfTimer;
        perfTimer.start();

        QImage inputImage(inputPath);
        if (inputImage.isNull()) {
            QString error = tr("Cannot read image: %1").arg(inputPath);
            qWarning() << "[AIEngine][processAsync]" << error;
            setProcessing(false);
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        if (inputImage.format() == QImage::Format_Invalid) {
            QString error = tr("Invalid image format: %1").arg(inputPath);
            setProcessing(false);
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        const qint64 loadInputCostMs = perfTimer.elapsed();

        QString lastError;
        QMetaObject::Connection errorConn = connect(
            this, &AIEngine::processError,
            this, [&lastError](const QString &err) { lastError = err; },
            Qt::DirectConnection);

        QImage result = process(inputImage);

        const qint64 processCostMs = perfTimer.elapsed() - loadInputCostMs;

        disconnect(errorConn);

        if (m_cancelRequested) {
            setProcessing(false);
            emit processFileCompleted(false, QString(), tr("Inference cancelled"));
            return;
        }

        if (result.isNull()) {
            QString error = lastError.isEmpty() ? tr("Inference failed, please check model compatibility and input image") : lastError;
            qWarning() << "[AIEngine] inference failed:" << error;
            setProcessing(false);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        if (!result.save(outputPath)) {
            QString error = tr("Cannot save result: %1").arg(outputPath);
            setProcessing(false);
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        setProcessing(false);
        emit processFileCompleted(true, outputPath, QString());
    });
}

void AIEngine::cancelProcess()
{
    qInfo() << "[AIEngine] cancelProcess() called, isProcessing:" << m_isProcessing.load();
    m_cancelRequested = true;
    // 注意：不要在这里设置 m_isProcessing = false
    // 让后台线程在检测到取消后自己设置，确保推理真正停止
}

void AIEngine::resetCancelFlag()
{
    m_cancelRequested = false;
    m_forceCancelled = false;
    qInfo() << "[AIEngine] resetCancelFlag() called - cancel flags reset";
}

void AIEngine::forceCancel()
{
    m_forceCancelled = true;
    m_cancelRequested = true;
    // 注意：不要在这里设置 m_isProcessing = false
    // 让后台线程在检测到取消后自己设置
    qWarning() << "[AIEngine] Force cancel requested";
}

bool AIEngine::isForceCancelled() const
{
    return m_forceCancelled.load();
}

void AIEngine::resetState()
{
    qInfo() << "[AIEngine] resetState() called"
            << "currentModel:" << m_currentModelId
            << "wasProcessing:" << m_isProcessing.load();
    
    {
        QMutexLocker inferenceLocker(&m_inferenceMutex);
    }
    
    m_cancelRequested.store(false);
    m_forceCancelled.store(false);
    m_isProcessing.store(false);
    m_progress.store(0.0);

    stopProgressSimulation();
    m_lastProgressEmitMs.store(0);

    stopProgressSimulation();
    
    {
        QMutexLocker locker(&m_lastErrorMutex);
        m_lastError.clear();
    }
    
    clearParameters();
    
    if (m_progressReporter) {
        m_progressReporter->reset();
    }
}

void AIEngine::safeCleanup()
{
    qInfo() << "[AIEngine] safeCleanup() called";
    
    QElapsedTimer waitTimer;
    waitTimer.start();
    const int kMaxWaitMs = 2000;
    
    while (m_isProcessing.load() && waitTimer.elapsed() < kMaxWaitMs) {
        QThread::msleep(10);
    }
    
    if (m_isProcessing.load()) {
        qWarning() << "[AIEngine] safeCleanup: inference still running after timeout";
    }
    
    {
        QMutexLocker inferenceLocker(&m_inferenceMutex);
    }
    
    m_cancelRequested.store(false);
    m_forceCancelled.store(false);
    m_isProcessing.store(false);
    m_progress.store(0.0);
    
    clearParameters();
    
    if (m_progressReporter) {
        m_progressReporter->reset();
    }
    
    qInfo() << "[AIEngine] safeCleanup() completed";
}

// ========== OpenCV Inpainting (Qt 原生实现) ==========

QImage AIEngine::inpaint(const QImage &input, const QImage &mask, int radius, int method)
{
    Q_UNUSED(method)

    if (input.isNull() || mask.isNull()) {
        emit processError(tr("Invalid repair input"));
        return QImage();
    }

    QImage src = input.convertToFormat(QImage::Format_ARGB32);
    QImage msk = mask.convertToFormat(QImage::Format_Grayscale8);

    if (src.size() != msk.size()) {
        msk = msk.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QImage result = src.copy();
    int w = result.width();
    int h = result.height();

    int maxIter = radius * 3;
    QImage working = result.copy();

    for (int iter = 0; iter < maxIter; ++iter) {
        bool anyChanged = false;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (*(msk.constScanLine(y) + x) == 0) continue;
                int sumR = 0, sumG = 0, sumB = 0, count = 0;
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                        if (*(msk.constScanLine(ny) + nx) == 0) {
                            QRgb pixel = working.pixel(nx, ny);
                            sumR += qRed(pixel); sumG += qGreen(pixel); sumB += qBlue(pixel);
                            count++;
                        }
                    }
                }
                if (count > 0) {
                    result.setPixel(x, y, qRgb(sumR/count, sumG/count, sumB/count));
                    anyChanged = true;
                }
            }
        }
        working = result.copy();
        if (!anyChanged) break;
        if (m_cancelRequested) break;
    }
    return result;
}

void AIEngine::inpaintAsync(const QString &inputPath, const QString &maskPath,
                             const QString &outputPath, int radius, int method)
{
    if (m_isProcessing.load()) {
        emit processError(tr("Task already in progress"));
        emit processFileCompleted(false, QString(), tr("Task already in progress"));
        return;
    }
    QtConcurrent::run([this, inputPath, maskPath, outputPath, radius, method]() {
        setProcessing(true);
        setProgress(0.0);
        QImage input(inputPath), mask(maskPath);
        if (input.isNull() || mask.isNull()) {
            setProcessing(false);
            emit processFileCompleted(false, QString(), tr("Cannot read image: %1").arg(
                input.isNull() ? inputPath : maskPath));
            return;
        }
        setProgress(0.2);
        QImage result = inpaint(input, mask, radius, method);
        setProgress(0.8);
        if (result.isNull()) { setProcessing(false); emit processFileCompleted(false, QString(), tr("Repair failed")); return; }
        if (!result.save(outputPath)) { setProcessing(false); emit processFileCompleted(false, QString(), tr("Save failed: %1").arg(outputPath)); return; }
        setProgress(1.0);
        setProcessing(false);
        emit processFileCompleted(true, outputPath, QString());
    });
}

// ========== 参数设置 ==========

void AIEngine::setParameter(const QString &name, const QVariant &value)
{
    QMutexLocker locker(&m_paramsMutex);
    m_parameters[name] = value;
}

QVariant AIEngine::getParameter(const QString &name) const
{
    QMutexLocker locker(&m_paramsMutex);
    return m_parameters.value(name);
}

void AIEngine::clearParameters()
{
    QMutexLocker locker(&m_paramsMutex);
    m_parameters.clear();
}

QVariantMap AIEngine::mergeWithDefaultParams() const
{
    QMutexLocker locker(&m_paramsMutex);
    ModelInfo model = m_currentModel;
    return getEffectiveParamsLocked(model);
}

QVariantMap AIEngine::getEffectiveParamsLocked(const ModelInfo &model) const
{
    QVariantMap merged = m_parameters;
    if (!model.id.isEmpty() && !model.supportedParams.isEmpty()) {
        for (auto it = model.supportedParams.begin(); it != model.supportedParams.end(); ++it) {
            const QString &paramKey = it.key();
            const QVariantMap &paramMeta = it.value().toMap();
            if (!merged.contains(paramKey)) {
                if (paramMeta.contains("default"))
                    merged[paramKey] = paramMeta["default"];
            } else {
                QVariant &val = merged[paramKey];
                QString type = paramMeta["type"].toString();
                if (type == "int" || type == "float") {
                    double minVal = paramMeta["min"].toDouble();
                    double maxVal = paramMeta["max"].toDouble();
                    val = std::clamp(val.toDouble(), minVal, maxVal);
                } else if (type == "bool") {
                    val = val.toBool();
                }
            }
        }
    }
    return merged;
}

QVariantMap AIEngine::getEffectiveParams() const
{
    QMutexLocker locker(&m_paramsMutex);
    return getEffectiveParamsLocked(m_currentModel);
}

// ========== 后端类型管理（CPU / Vulkan GPU） ==========

bool AIEngine::setBackendType(BackendType type)
{
    if (m_isProcessing.load()) {
        qWarning() << "[AIEngine] Cannot switch backend during inference";
        emit processError(tr("Inference in progress, cannot switch backend"));
        return false;
    }

    if (type == m_backendType) {
        qInfo() << "[AIEngine] Already on backend:" << static_cast<int>(type);
        return true;
    }

    QString currentModelId = m_currentModelId;

#ifdef NCNN_VULKAN_AVAILABLE
    if (type == BackendType::NCNN_Vulkan) {
        {
            std::lock_guard<std::mutex> lock(m_gpuReadyMutex);
            m_gpuReady.store(false);
        }
        
        {
            std::lock_guard<std::mutex> lock(m_modelSyncMutex);
            m_modelSyncComplete.store(false);
        }
        
        if (!initializeVulkan()) {
            qWarning() << "[AIEngine] Vulkan init failed, staying on CPU";
            {
                std::lock_guard<std::mutex> lock(m_modelSyncMutex);
                m_modelSyncComplete.store(true);
            }
            m_modelSyncCv.notify_all();
            return false;
        }
        
        if (!waitForGpuReady(5000)) {
            qWarning() << "[AIEngine] GPU ready timeout after Vulkan init";
            {
                std::lock_guard<std::mutex> lock(m_modelSyncMutex);
                m_modelSyncComplete.store(true);
            }
            m_modelSyncCv.notify_all();
            return false;
        }
        
        m_backendType = BackendType::NCNN_Vulkan;
        applyBackendOptions(BackendType::NCNN_Vulkan);
        
        if (!currentModelId.isEmpty()) {
            qInfo() << "[AIEngine] Reloading model for Vulkan backend:" << currentModelId;
            if (!loadModel(currentModelId)) {
                qWarning() << "[AIEngine] Failed to reload model after backend switch to Vulkan";
                m_backendType = BackendType::NCNN_CPU;
                applyBackendOptions(BackendType::NCNN_CPU);
                {
                    std::lock_guard<std::mutex> lock(m_modelSyncMutex);
                    m_modelSyncComplete.store(true);
                }
                m_modelSyncCv.notify_all();
                return false;
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(m_modelSyncMutex);
            m_modelSyncComplete.store(true);
        }
        m_modelSyncCv.notify_all();
        
        qInfo() << "[AIEngine] Switched to Vulkan GPU backend successfully";
    } else {
        shutdownVulkan();
        m_backendType = BackendType::NCNN_CPU;
        applyBackendOptions(BackendType::NCNN_CPU);
        
        if (!currentModelId.isEmpty()) {
            qInfo() << "[AIEngine] Reloading model for CPU backend:" << currentModelId;
            loadModel(currentModelId);
        }
        
        {
            std::lock_guard<std::mutex> lock(m_modelSyncMutex);
            m_modelSyncComplete.store(true);
        }
        m_modelSyncCv.notify_all();
        
        qInfo() << "[AIEngine] Switched to CPU backend";
    }
#else
    if (type == BackendType::NCNN_Vulkan) {
        qWarning() << "[AIEngine] Vulkan not available at compile time";
        emit processError(tr("Vulkan support not enabled at compile time"));
        return false;
    }
#endif

    emit backendTypeChanged(m_backendType);
    return true;
}

bool AIEngine::initializeVulkan(int gpuDeviceId)
{
#ifdef NCNN_VULKAN_AVAILABLE
    try {
        qInfo() << "[AIEngine] initializeVulkan() called, requested device:" << gpuDeviceId;
        
        ensureGpuInstanceCreated();
        
        if (!s_gpuInstanceCreated.load()) {
            emit processError(tr("Failed to create Vulkan instance"));
            return false;
        }

        int deviceCount = ncnn::get_gpu_count();
        if (deviceCount <= 0) {
            emit processError(tr("No Vulkan-capable GPU device found"));
            return false;
        }

        if (gpuDeviceId >= deviceCount) {
            qWarning() << "[AIEngine] Requested GPU device" << gpuDeviceId
                       << "not available, using device 0";
            gpuDeviceId = 0;
        }

        const ncnn::GpuInfo& gpuInfo = ncnn::get_gpu_info(gpuDeviceId);
        m_gpuDeviceId = gpuDeviceId;
        m_gpuDeviceName = QString::fromUtf8(gpuInfo.device_name());
        m_gpuAvailable.store(true);
        m_vulkanInstanceCreated = true;

        qInfo() << "[AIEngine] Vulkan initialized successfully"
                << "device:" << m_gpuDeviceName
                << "id:" << m_gpuDeviceId
                << "apiVersion:" << (gpuInfo.api_version() >> 22) << "."
                << ((gpuInfo.api_version() >> 12) & 0x3ff) << "."
                << (gpuInfo.api_version() & 0xfff);

        {
            std::lock_guard<std::mutex> lock(m_gpuReadyMutex);
            m_gpuReady.store(true);
        }
        m_gpuReadyCv.notify_all();

        emit gpuDeviceInfoChanged();
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[AIEngine] Vulkan init exception:" << e.what();
        shutdownVulkan();
        emit processError(tr("GPU initialization failed: %1").arg(e.what()));
        return false;
    } catch (...) {
        qWarning() << "[AIEngine] Vulkan init unknown exception";
        shutdownVulkan();
        emit processError(tr("GPU initialization failed: unknown exception"));
        return false;
    }
#else
    Q_UNUSED(gpuDeviceId);
    emit processError(tr("Application not compiled with Vulkan support"));
    return false;
#endif
}

void AIEngine::shutdownVulkan()
{
#ifdef NCNN_VULKAN_AVAILABLE
    m_vulkanInstanceCreated = false;
    
    {
        std::lock_guard<std::mutex> lock(m_gpuReadyMutex);
        m_gpuReady.store(false);
    }
#endif
    m_gpuAvailable.store(false);
    m_gpuDeviceName.clear();
    m_gpuDeviceId = -1;
    qInfo() << "[AIEngine] Vulkan resources released (instance preserved for reuse)";
}

bool AIEngine::isVulkanReady() const
{
#ifdef NCNN_VULKAN_AVAILABLE
    return m_gpuAvailable.load() && m_vulkanInstanceCreated;
#else
    return false;
#endif
}

bool AIEngine::applyBackendOptions(BackendType target_type)
{
    if (target_type == BackendType::NCNN_Vulkan) {
        // Vulkan GPU 模式配置（性能优先）
        m_opt.use_vulkan_compute = true;
        m_opt.num_threads = 0;              // GPU 模式不需要 CPU 多线程
        m_opt.lightmode = true;             // GPU 上可以启用轻量模式
        m_opt.use_packing_layout = true;     // GPU 支持打包布局
        m_opt.use_local_pool_allocator = true;
        // 以下选项保持关闭以确保稳定性
        m_opt.use_sgemm_convolution = false;
        m_opt.use_winograd_convolution = false;
    } else {
        // CPU 模式配置（安全稳定优先，参考 crash-fix-notes）
        m_opt.use_vulkan_compute = false;
        m_opt.num_threads = 1;              // 单线程推理
        m_opt.lightmode = false;
        m_opt.use_packing_layout = false;
        m_opt.use_local_pool_allocator = false;
        m_opt.use_sgemm_convolution = false;
        m_opt.use_winograd_convolution = false;
    }

    m_net.opt = m_opt;
    return true;
}

// ========== 状态查询 ==========

bool AIEngine::isProcessing() const { return m_isProcessing; }
double AIEngine::progress() const { return m_progress; }
QString AIEngine::progressText() const { return m_progressText; }
QString AIEngine::currentModelId() const { return m_currentModelId; }

void AIEngine::emitError(const QString &error)
{
    qWarning() << "[AIEngine] Error:" << error;
    if (QThread::currentThread() == thread()) {
        emit processError(error);
    } else {
        QMetaObject::invokeMethod(this, [this, error]() {
            emit processError(error);
        }, Qt::QueuedConnection);
    }
}

ProgressReporter* AIEngine::progressReporter()
{
    return m_progressReporter.get();
}

// ========== 内部方法 ==========

void AIEngine::setProgress(double value, bool forceEmit)
{
    if (m_progressReporter) {
        if (forceEmit) {
            m_progressReporter->forceUpdate(value);
        } else {
            m_progressReporter->setProgress(value);
        }
        return;
    }
    
    constexpr double kProgressEmitDelta = 0.01;
    constexpr qint64 kProgressEmitIntervalMs = 66;
    const double clampedValue = std::clamp(value, 0.0, 1.0);
    
    double previous = m_progress.load();
    const bool isReset = (clampedValue < 0.01);
    const bool isComplete = (clampedValue >= 0.99);
    const bool isForward = (clampedValue > previous);
    
    if (!forceEmit && !isReset && !isComplete && !isForward) {
        return;
    }
    
    previous = m_progress.exchange(clampedValue);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastEmit = m_lastProgressEmitMs.load();
    const bool firstEmit = (lastEmit == 0);
    const bool reachedTerminal = (clampedValue >= 1.0);
    const bool progressedEnough = (std::abs(clampedValue - previous) >= kProgressEmitDelta);
    const bool timeoutReached = (nowMs - lastEmit) >= kProgressEmitIntervalMs;
    if (forceEmit || firstEmit || reachedTerminal || progressedEnough || timeoutReached) {
        m_lastProgressEmitMs.store(nowMs);
        if (QThread::currentThread() == thread()) {
            emit progressChanged(clampedValue);
        } else {
            QMetaObject::invokeMethod(this, [this, clampedValue]() {
                emit progressChanged(clampedValue);
            }, Qt::QueuedConnection);
        }
    }
}

void AIEngine::setProcessing(bool processing)
{
    m_isProcessing = processing;
    if (QThread::currentThread() == thread()) {
        emit processingChanged(processing);
    } else {
        QMetaObject::invokeMethod(this, [this, processing]() {
            emit processingChanged(processing);
        }, Qt::QueuedConnection);
    }
}

void AIEngine::startProgressSimulation(double fromProgress, double toProgress, qint64 estimatedMs)
{
    m_simulateStartProgress = fromProgress;
    m_simulateTargetProgress = toProgress;
    m_simulateEstimatedMs.store(std::max(qint64(500), estimatedMs));
    m_inferStartTime.start();
    m_simulatingProgress.store(true);
}

void AIEngine::stopProgressSimulation()
{
    m_simulatingProgress.store(false);
}

bool AIEngine::isSimulatingProgress() const
{
    return m_simulatingProgress.load();
}

ncnn::Mat AIEngine::qimageToMat(const QImage &image, const ModelInfo &model)
{
    QImage img;
    if (image.format() == QImage::Format_RGB888) {
        img = image;
    } else {
        img = image.convertToFormat(QImage::Format_RGB888);
        if (img.isNull()) {
            qWarning() << "[AIEngine][qimageToMat] failed to convert image format";
            return ncnn::Mat();
        }
    }

    if (img.isNull() || img.width() <= 0 || img.height() <= 0) {
        qWarning() << "[AIEngine][qimageToMat] invalid converted image";
        return ncnn::Mat();
    }

    const int w = img.width();
    const int h = img.height();
    const int srcStride = img.bytesPerLine();
    const int dstStride = w * 3;  // RGB888 紧凑排列
    
    // 【关键修复】如果 QImage 的 bytesPerLine 与紧凑排列不同，
    // 需要先拷贝到连续内存缓冲区，避免 NCNN 内部的 stride 处理问题
    std::vector<unsigned char> buffer;
    const unsigned char* pixelData = nullptr;
    int actualStride = 0;
    
    if (srcStride != dstStride) {
        // QImage 有额外的行填充，需要拷贝到紧凑缓冲区
        buffer.resize(static_cast<size_t>(w) * h * 3);
        for (int y = 0; y < h; ++y) {
            const uchar* srcLine = img.constScanLine(y);
            std::memcpy(buffer.data() + y * dstStride, srcLine, dstStride);
        }
        pixelData = buffer.data();
        actualStride = dstStride;
    } else {
        // QImage 已经是紧凑排列，直接使用
        pixelData = img.constBits();
        actualStride = srcStride;
    }

    ncnn::Mat in = ncnn::Mat::from_pixels(pixelData, ncnn::Mat::PIXEL_RGB, w, h, actualStride);
    
    if (in.empty()) {
        qWarning() << "[AIEngine][qimageToMat] from_pixels failed";
        return ncnn::Mat();
    }
    
    in.substract_mean_normalize(model.normMean, model.normScale);
    
    return in;
}

QImage AIEngine::matToQimage(const ncnn::Mat &mat, const ModelInfo &model)
{
    if (mat.empty() || mat.w <= 0 || mat.h <= 0 || mat.c <= 0 || mat.data == nullptr) {
        qWarning() << "[AIEngine][matToQimage] received empty/invalid mat";
        return QImage();
    }
    const int w = mat.w, h = mat.h;
    ncnn::Mat out = mat.clone();
    if (out.empty() || out.data == nullptr) {
        qWarning() << "[AIEngine][matToQimage] mat.clone() produced empty mat";
        return QImage();
    }
    out.substract_mean_normalize(model.denormMean, model.denormScale);
    float *data = (float *)out.data;
    const int total = w * h * out.c;
    for (int i = 0; i < total; ++i) {
        data[i] = std::clamp(data[i], 0.f, 255.f);
    }
    
    // 【关键修复】先写入连续内存缓冲区，再拷贝到 QImage
    // 避免 NCNN 的 to_pixels 直接写入 QImage 时的 stride 问题
    // 使用 32 字节对齐的 stride，与 FFmpeg 保持一致
    const int compactStride = w * 3;  // RGB888 紧凑排列
    const int alignedStride = (compactStride + 31) & ~31;  // 32 字节对齐
    const size_t bufferSize = static_cast<size_t>(alignedStride) * h;
    std::vector<unsigned char> buffer(bufferSize);
    
    // 使用对齐的 stride 调用 to_pixels
    out.to_pixels(buffer.data(), ncnn::Mat::PIXEL_RGB, alignedStride);
    
    QImage result(w, h, QImage::Format_RGB888);
    if (result.isNull()) {
        qWarning() << "[AIEngine][matToQimage] failed to create QImage";
        return QImage();
    }
    
    // 逐行拷贝到 QImage（从对齐的缓冲区拷贝紧凑的像素数据）
    for (int y = 0; y < h; ++y) {
        std::memcpy(result.scanLine(y), buffer.data() + y * alignedStride, compactStride);
    }
    
    return result;
}

ncnn::Mat AIEngine::runInference(const ncnn::Mat &input, const ModelInfo &model)
{
    qInfo() << "[AIEngine] runInference:"
            << "inputMat:" << input.w << "x" << input.h << "x" << input.c
            << "modelId:" << model.id;
    fflush(stdout);
    
    if (input.empty() || input.w <= 0 || input.h <= 0) {
        qWarning() << "[AIEngine][Inference] received empty input mat";
        return ncnn::Mat();
    }
    if (!model.isLoaded && !model.paramPath.isEmpty()) {
        qWarning() << "[AIEngine][Inference] model not loaded";
        return ncnn::Mat();
    }

#ifdef NCNN_VULKAN_AVAILABLE
    if (m_backendType == BackendType::NCNN_Vulkan) {
        if (!waitForModelSyncComplete(5000)) {
            qWarning() << "[AIEngine][Inference] Model sync not complete, aborting inference";
            emitError(tr("Model sync incomplete, cannot start inference"));
            return ncnn::Mat();
        }
        if (!waitForGpuReady(3000)) {
            qWarning() << "[AIEngine][Inference] GPU not ready, aborting inference";
            emitError(tr("GPU not ready, cannot start inference"));
            return ncnn::Mat();
        }
        if (!s_gpuInstanceCreated.load()) {
            qWarning() << "[AIEngine][Inference] Vulkan instance not created";
            emitError(tr("Vulkan instance not created"));
            return ncnn::Mat();
        }
        qInfo() << "[AIEngine][Inference] GPU resources verified, proceeding with inference";
    }
#endif

    qInfo() << "[AIEngine][Inference] Step 1: Preparing blob candidates";
    fflush(stdout);

    QStringList inputCandidates;
    inputCandidates << model.inputBlobName << "input" << "data" << "in0" << "Input1";
    inputCandidates.removeAll(QString());
    inputCandidates.removeDuplicates();

    QStringList outputCandidates;
    outputCandidates << model.outputBlobName << "output" << "out0" << "prob";
    outputCandidates.removeAll(QString());
    outputCandidates.removeDuplicates();

    if (inputCandidates.isEmpty() || outputCandidates.isEmpty()) {
        qWarning() << "[AIEngine][Inference] empty blob candidate list";
        return ncnn::Mat();
    }

    qInfo() << "[AIEngine][Inference] Step 2: Acquiring mutex lock";
    fflush(stdout);
    
    QMutexLocker netLocker(&m_mutex);
    
    qInfo() << "[AIEngine][Inference] Step 3: Creating extractor";
    fflush(stdout);
    
    ncnn::Extractor ex = m_net.create_extractor();
    ex.set_light_mode(false);

    qInfo() << "[AIEngine][Inference] Step 4: Setting input blob, candidates:" << inputCandidates;
    fflush(stdout);
    
    QString selectedInputBlob;
    int inputRet = -1;
    for (const QString &candidate : inputCandidates) {
        qInfo() << "[AIEngine][Inference] Trying input blob:" << candidate;
        fflush(stdout);
        inputRet = ex.input(candidate.toStdString().c_str(), input);
        if (inputRet == 0) {
            selectedInputBlob = candidate;
            qInfo() << "[AIEngine][Inference] Input blob set successfully:" << candidate;
            fflush(stdout);
            break;
        }
    }

    if (inputRet != 0) {
        qWarning() << "[AIEngine][Inference] Failed to set input blob"
                   << "requested:" << model.inputBlobName
                   << "tried:" << inputCandidates
                   << "ret:" << inputRet;
        emitError(tr("Inference input failed, model input node mismatch"));
        return ncnn::Mat();
    }

    qInfo() << "[AIEngine][Inference] Step 5: Extracting output blob";
    fflush(stdout);

    ncnn::Mat output;
    int extractRet = -1;
    QString selectedOutputBlob;
    
#ifdef NCNN_VULKAN_AVAILABLE
    if (m_backendType == BackendType::NCNN_Vulkan) {
        qInfo() << "[AIEngine][Inference] Vulkan mode - checking GPU state before extract:"
                << "s_gpuInstanceCreated:" << s_gpuInstanceCreated.load()
                << "m_gpuAvailable:" << m_gpuAvailable.load()
                << "m_vulkanInstanceCreated:" << m_vulkanInstanceCreated
                << "m_gpuDeviceId:" << m_gpuDeviceId;
        fflush(stdout);
        
        if (!s_gpuInstanceCreated.load()) {
            qWarning() << "[AIEngine][Inference] Vulkan instance not created before extract!";
            emitError(tr("Vulkan instance not created"));
            return ncnn::Mat();
        }
    }
#endif
    
    for (const QString &candidate : outputCandidates) {
        qInfo() << "[AIEngine][Inference] Trying output blob:" << candidate;
        fflush(stdout);
        
        qInfo() << "[AIEngine][Inference] About to call ex.extract()...";
        fflush(stdout);
        
        ncnn::Mat tmp;
        extractRet = ex.extract(candidate.toStdString().c_str(), tmp);
        
        qInfo() << "[AIEngine][Inference] ex.extract() returned:" << extractRet;
        fflush(stdout);
        
        if (extractRet == 0 && !tmp.empty()) {
            output = tmp.clone();
            selectedOutputBlob = candidate;
            qInfo() << "[AIEngine][Inference] Output blob extracted successfully:" << candidate
                    << "size:" << output.w << "x" << output.h << "x" << output.c;
            fflush(stdout);
            break;
        }
    }

    if (extractRet != 0 || output.empty()) {
        qWarning() << "[AIEngine][Inference] Failed to extract output blob"
                   << "requested:" << model.outputBlobName
                   << "tried:" << outputCandidates
                   << "extractRet:" << extractRet;
        emitError(tr("Inference output failed, model output node mismatch (ret=%1)").arg(extractRet));
        return ncnn::Mat();
    }

    qInfo() << "[AIEngine][Inference] Step 6: Inference complete, returning output";
    fflush(stdout);

    // 【关键修复】显式释放 extractor 资源，避免内存累积
    // extractor 在作用域结束时会自动析构，但我们需要确保在返回前完成
    
    return output;
}

// 【新增】清理 NCNN 内部缓存，用于视频处理时定期调用
void AIEngine::clearInferenceCache()
{
#ifdef NCNN_VULKAN_AVAILABLE
    if (m_backendType == BackendType::NCNN_Vulkan && m_vulkanInstanceCreated) {
        // NCNN Vulkan 缓存由内部管理，此处确保同步点
        qInfo() << "[AIEngine] GPU cache sync point";
    }
#endif
}

QImage AIEngine::processSingle(const QImage &input, const ModelInfo &model)
{
    qInfo() << "[AIEngine] processSingle() starting:"
            << "inputSize:" << input.width() << "x" << input.height()
            << "modelId:" << model.id;

    setProgress(0.02);
    setProgress(0.05);

    ncnn::Mat in = qimageToMat(input, model);
    if (in.empty()) {
        qWarning() << "[AIEngine][Single] qimageToMat failed";
        return QImage();
    }

    if (m_cancelRequested) return QImage();
    setProgress(0.15);

    startProgressSimulation(0.15, 0.80, 3000);

    std::atomic<bool> inferComplete{false};
    ncnn::Mat out;

    std::thread simThread([this, &inferComplete]() {
        while (!inferComplete.load() && m_simulatingProgress.load() && !m_cancelRequested) {
            qint64 elapsed = m_inferStartTime.elapsed();
            qint64 estimated = m_simulateEstimatedMs.load();
            double range = m_simulateTargetProgress - m_simulateStartProgress;
            double simProgress = m_simulateStartProgress + range * std::min(1.0, static_cast<double>(elapsed) / static_cast<double>(estimated));
            setProgress(simProgress);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    });

    out = runInference(in, model);

    inferComplete.store(true);
    stopProgressSimulation();
    if (simThread.joinable()) {
        simThread.join();
    }

    if (out.empty() || out.w <= 0 || out.h <= 0) {
        return QImage();
    }
    if (m_cancelRequested) return QImage();
    setProgress(0.85);

    QImage result = matToQimage(out, model);

    setProgress(0.95);
    return result;
}

QImage AIEngine::processTiled(const QImage &input, const ModelInfo &model)
{
    setProgress(0.01);
    
    qInfo() << "[AIEngine][Tiled] Step 1: Entry check";
    fflush(stdout);
    
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIEngine] invalid input image";
        return QImage();
    }
    
    qInfo() << "[AIEngine][Tiled] Step 2: Input valid, bits:" << (input.bits() ? "ok" : "null")
            << "format:" << input.format();
    fflush(stdout);

    setProgress(0.02);
    
    int tileSize = model.tileSize;
    int scale = model.scaleFactor;
    int w = input.width();
    int h = input.height();
    
    if (tileSize <= 0) {
        qWarning() << "[AIEngine] invalid tileSize, falling back to processSingle";
        return processSingle(input, model);
    }
    
    int minDim = std::min(w, h);
    if (tileSize > minDim) {
        qInfo() << "[AIEngine] tileSize" << tileSize << "exceeds min dimension" << minDim
                << ", falling back to processSingle";
        return processSingle(input, model);
    }

    qInfo() << "[AIEngine] processTiled:"
            << "inputSize:" << w << "x" << h
            << "tileSize:" << tileSize
            << "scale:" << scale
            << "padding:" << model.tilePadding;
    fflush(stdout);

    setProgress(0.03);
    
    qInfo() << "[AIEngine][Tiled] Step 3: Computing padding";
    fflush(stdout);

    int padding = model.tilePadding;
    const int layerCount = model.layerCount;
    if (layerCount > 500) {
        padding = std::max(padding, 64);
    } else if (layerCount > 200) {
        padding = std::max(padding, 48);
    } else if (layerCount > 50) {
        padding = std::max(padding, 24);
    }
    
    qInfo() << "[AIEngine][Tiled] Step 4: Padding computed:" << padding << "layerCount:" << layerCount;
    fflush(stdout);
    
    // 【关键修复】手动创建深拷贝，避免 QImage::copy() 在堆损坏时触发崩溃
    QImage normalizedInput(w, h, QImage::Format_RGB888);
    if (normalizedInput.isNull()) {
        qWarning() << "[AIEngine][Tiled] Failed to create normalizedInput";
        return processSingle(input, model);
    }
    
    if (input.format() == QImage::Format_RGB888) {
        qInfo() << "[AIEngine][Tiled] Step 5a: Input already RGB888, creating manual deep copy";
        fflush(stdout);
        
        // 逐行拷贝数据，避免使用 QImage::copy()
        const int srcBytesPerLine = input.bytesPerLine();
        const int dstBytesPerLine = w * 3;  // RGB888 紧凑排列
        for (int y = 0; y < h; ++y) {
            const uchar* srcLine = input.constScanLine(y);
            uchar* dstLine = normalizedInput.scanLine(y);
            std::memcpy(dstLine, srcLine, std::min(srcBytesPerLine, dstBytesPerLine));
        }
        
        qInfo() << "[AIEngine][Tiled] Step 5a-done: Manual deep copy created, normalizedInput.isNull:" << normalizedInput.isNull()
                << "bits:" << (normalizedInput.bits() ? "ok" : "null")
                << "bytesPerLine:" << normalizedInput.bytesPerLine();
        fflush(stdout);
    } else {
        qInfo() << "[AIEngine][Tiled] Step 5b: Converting input to RGB888 from format:" << input.format();
        fflush(stdout);
        
        // 先转换格式，再手动深拷贝
        QImage tempConverted = input.convertToFormat(QImage::Format_RGB888);
        if (tempConverted.isNull()) {
            qWarning() << "[AIEngine][Tiled] Failed to convert input to RGB888";
            return processSingle(input, model);
        }
        
        // 逐行拷贝数据
        const int srcBytesPerLine = tempConverted.bytesPerLine();
        const int dstBytesPerLine = w * 3;  // RGB888 紧凑排列
        for (int y = 0; y < h; ++y) {
            const uchar* srcLine = tempConverted.constScanLine(y);
            uchar* dstLine = normalizedInput.scanLine(y);
            std::memcpy(dstLine, srcLine, std::min(srcBytesPerLine, dstBytesPerLine));
        }
        
        qInfo() << "[AIEngine][Tiled] Step 5b-done: Conversion and manual deep copy complete, normalizedInput.isNull:" << normalizedInput.isNull();
        fflush(stdout);
    }

    if (normalizedInput.isNull() || normalizedInput.format() != QImage::Format_RGB888) {
        qWarning() << "[AIEngine] failed to normalize input format";
        return processSingle(input, model);
    }
    
    qInfo() << "[AIEngine][Tiled] Step 6: Normalized input ready, size:" << normalizedInput.width() << "x" << normalizedInput.height()
            << "bits:" << (normalizedInput.bits() ? "ok" : "null")
            << "bytesPerLine:" << normalizedInput.bytesPerLine();
    fflush(stdout);

    qInfo() << "[AIEngine][Tiled] Step 7: Creating paddedInput, size:" << (w + 2 * padding) << "x" << (h + 2 * padding);
    fflush(stdout);
    
    QImage paddedInput(w + 2 * padding, h + 2 * padding, QImage::Format_RGB888);
    if (paddedInput.isNull()) {
        qWarning() << "[AIEngine] failed to create padded image";
        return processSingle(input, model);
    }
    
    qInfo() << "[AIEngine][Tiled] Step 8: paddedInput created, bits:" << (paddedInput.bits() ? "ok" : "null")
            << "bytesPerLine:" << paddedInput.bytesPerLine();
    fflush(stdout);
    
    paddedInput.fill(Qt::black);
    
    qInfo() << "[AIEngine][Tiled] Step 9: paddedInput filled with black";
    fflush(stdout);
    
    const int srcBytesPerLine = normalizedInput.bytesPerLine();
    const int expectedBytesPerLine = w * 3;
    
    qInfo() << "[AIEngine][Tiled] Step 10: Starting center region copy, srcBytesPerLine:" << srcBytesPerLine
            << "expectedBytesPerLine:" << expectedBytesPerLine;
    fflush(stdout);

    for (int y = 0; y < h; ++y) {
        const uchar* srcLine = normalizedInput.constScanLine(y);
        uchar* dstLine = paddedInput.scanLine(y + padding);
        if (srcBytesPerLine >= expectedBytesPerLine) {
            std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
        } else {
            std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
            std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
        }
    }
    
    qInfo() << "[AIEngine][Tiled] Step 11: Center region copy done";
    fflush(stdout);
    
    for (int y = 0; y < std::min(padding, h); ++y) {
        const uchar* srcLine = normalizedInput.constScanLine(y);
        uchar* dstLine = paddedInput.scanLine(padding - 1 - y);
        if (srcBytesPerLine >= expectedBytesPerLine) {
            std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
        } else {
            std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
            std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
        }
    }
    for (int y = 0; y < std::min(padding, h); ++y) {
        const uchar* srcLine = normalizedInput.constScanLine(h - 1 - y);
        uchar* dstLine = paddedInput.scanLine(padding + h + y);
        if (srcBytesPerLine >= expectedBytesPerLine) {
            std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
        } else {
            std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
            std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
        }
    }
    // 【安全边界检查】获取 paddedInput 的实际尺寸
    const int paddedWidth = paddedInput.width();
    const int paddedHeight = paddedInput.height();
    const int paddedBytesPerLine = paddedInput.bytesPerLine();
    
    // 左右边界镜像填充
    for (int y = 0; y < h; ++y) {
        uchar* dstLine = paddedInput.scanLine(y + padding);
        const uchar* srcLine = normalizedInput.constScanLine(y);
        // 左边界
        for (int x = 0; x < std::min(padding, w); ++x) {
            int srcX = x;
            int dstX = padding - 1 - x;
            if (dstX >= 0 && dstX < paddedWidth && (dstX * 3 + 2) < paddedBytesPerLine) {
                dstLine[dstX * 3 + 0] = srcLine[srcX * 3 + 0];
                dstLine[dstX * 3 + 1] = srcLine[srcX * 3 + 1];
                dstLine[dstX * 3 + 2] = srcLine[srcX * 3 + 2];
            }
        }
        // 右边界
        for (int x = 0; x < std::min(padding, w); ++x) {
            int srcX = w - 1 - x;
            int dstX = padding + w + x;
            if (dstX >= 0 && dstX < paddedWidth && (dstX * 3 + 2) < paddedBytesPerLine) {
                dstLine[dstX * 3 + 0] = srcLine[srcX * 3 + 0];
                dstLine[dstX * 3 + 1] = srcLine[srcX * 3 + 1];
                dstLine[dstX * 3 + 2] = srcLine[srcX * 3 + 2];
            }
        }
    }
    
    // 四角镜像填充
    for (int y = 0; y < std::min(padding, h); ++y) {
        int topY = padding - 1 - y;
        int botY = padding + h + y;
        if (topY < 0 || topY >= paddedHeight || botY < 0 || botY >= paddedHeight) continue;
        
        uchar* dstLineTop = paddedInput.scanLine(topY);
        uchar* dstLineBot = paddedInput.scanLine(botY);
        const uchar* srcLineTop = normalizedInput.constScanLine(y);
        const uchar* srcLineBot = normalizedInput.constScanLine(h - 1 - y);
        
        for (int x = 0; x < std::min(padding, w); ++x) {
            int dstXL = padding - 1 - x;
            int dstXR = padding + w + x;
            int srcXL = x;
            int srcXR = w - 1 - x;
            
            // 左上角和左下角
            if (dstXL >= 0 && dstXL < paddedWidth && (dstXL * 3 + 2) < paddedBytesPerLine) {
                dstLineTop[dstXL * 3 + 0] = srcLineTop[srcXL * 3 + 0];
                dstLineTop[dstXL * 3 + 1] = srcLineTop[srcXL * 3 + 1];
                dstLineTop[dstXL * 3 + 2] = srcLineTop[srcXL * 3 + 2];
                dstLineBot[dstXL * 3 + 0] = srcLineBot[srcXL * 3 + 0];
                dstLineBot[dstXL * 3 + 1] = srcLineBot[srcXL * 3 + 1];
                dstLineBot[dstXL * 3 + 2] = srcLineBot[srcXL * 3 + 2];
            }
            // 右上角和右下角
            if (dstXR >= 0 && dstXR < paddedWidth && (dstXR * 3 + 2) < paddedBytesPerLine) {
                dstLineTop[dstXR * 3 + 0] = srcLineTop[srcXR * 3 + 0];
                dstLineTop[dstXR * 3 + 1] = srcLineTop[srcXR * 3 + 1];
                dstLineTop[dstXR * 3 + 2] = srcLineTop[srcXR * 3 + 2];
                dstLineBot[dstXR * 3 + 0] = srcLineBot[srcXR * 3 + 0];
                dstLineBot[dstXR * 3 + 1] = srcLineBot[srcXR * 3 + 1];
                dstLineBot[dstXR * 3 + 2] = srcLineBot[srcXR * 3 + 2];
            }
        }
    }
    
    const int paddedW = paddedInput.width();
    const int paddedH = paddedInput.height();

    int tilesX = (w + tileSize - 1) / tileSize;
    int tilesY = (h + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;

    setProgress(0.10);
    
    QImage output(w * scale, h * scale, QImage::Format_RGB888);
    output.fill(Qt::black);
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    
    setProgress(0.15);

    int tileIndex = 0;
    int successfulTiles = 0;
    int consecutiveFailures = 0;
    const int kMaxConsecutiveFailures = 3;

    qInfo() << "[AIEngine] Starting tiled processing:"
            << "tilesX:" << tilesX
            << "tilesY:" << tilesY
            << "totalTiles:" << totalTiles;

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            if (m_cancelRequested) {
                qInfo() << "[AIEngine] Tiled processing cancelled at tile" << tileIndex;
                painter.end();
                return QImage();
            }
            
            if (m_forceCancelled.load()) {
                qInfo() << "[AIEngine] Tiled processing force-cancelled at tile" << tileIndex;
                painter.end();
                return QImage();
            }

            if (consecutiveFailures >= kMaxConsecutiveFailures) {
                qWarning() << "[AIEngine][Tiled] fast-fail: too many consecutive failures";
                painter.end();
                emitError(tr("Consecutive tile processing failures"));
                return QImage();
            }

            int x0 = tx * tileSize;
            int y0 = ty * tileSize;
            int x1 = std::min(x0 + tileSize, w);
            int y1 = std::min(y0 + tileSize, h);
            int actualTileW = x1 - x0;
            int actualTileH = y1 - y0;

            int px0 = x0;
            int py0 = y0;
            int extractW = actualTileW + 2 * padding;
            int extractH = actualTileH + 2 * padding;

            QImage tile = paddedInput.copy(px0, py0, extractW, extractH);
            ncnn::Mat in = qimageToMat(tile, model);
            if (in.empty()) {
                qWarning() << "[AIEngine][Tiled] tile" << tileIndex << "qimageToMat failed";
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            ncnn::Mat out = runInference(in, model);
            if (out.empty() || out.w <= 0 || out.h <= 0) {
                qWarning() << "[AIEngine][Tiled] tile" << tileIndex << "inference failed";
                tileIndex++;
                consecutiveFailures++;
                double tileProgress = 0.15 + 0.70 * static_cast<double>(tileIndex) / totalTiles;
                setProgress(tileProgress);
                continue;
            }

            QImage tileResult = matToQimage(out, model);
            if (tileResult.isNull()) {
                qWarning() << "[AIEngine][Tiled] tile" << tileIndex << "matToQimage failed";
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            const int expectedOutW = extractW * scale;
            const int expectedOutH = extractH * scale;
            const int actualOutW = tileResult.width();
            const int actualOutH = tileResult.height();

            int outPadLeft, outPadTop, outW, outH;
            if (actualOutW == expectedOutW && actualOutH == expectedOutH) {
                outPadLeft = padding * scale;
                outPadTop  = padding * scale;
                outW = actualTileW * scale;
                outH = actualTileH * scale;
            } else {
                double scaleX = static_cast<double>(actualOutW) / extractW;
                double scaleY = static_cast<double>(actualOutH) / extractH;
                outPadLeft = static_cast<int>(std::round(padding * scaleX));
                outPadTop  = static_cast<int>(std::round(padding * scaleY));
                outW = static_cast<int>(std::round(actualTileW * scaleX));
                outH = static_cast<int>(std::round(actualTileH * scaleY));
            }

            outW = std::min(outW, actualOutW - outPadLeft);
            outH = std::min(outH, actualOutH - outPadTop);

            if (outPadLeft < 0 || outPadTop < 0 || outW <= 0 || outH <= 0 ||
                outPadLeft + outW > actualOutW || outPadTop + outH > actualOutH) {
                qWarning() << "[AIEngine][Tiled] tile" << tileIndex << "crop region out of bounds";
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            QImage croppedResult = tileResult.copy(outPadLeft, outPadTop, outW, outH);

            int dstX = x0 * scale;
            int dstY = y0 * scale;
            int dstW = actualTileW * scale;
            int dstH = actualTileH * scale;

            if (croppedResult.width() != dstW || croppedResult.height() != dstH) {
                croppedResult = croppedResult.scaled(dstW, dstH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }

            painter.drawImage(dstX, dstY, croppedResult);
            tileIndex++;
            successfulTiles++;
            consecutiveFailures = 0;
            double tileProgress = 0.15 + 0.70 * static_cast<double>(tileIndex) / totalTiles;
            setProgress(tileProgress);
        }
    }
    painter.end();
    
    setProgress(0.85);

    if (successfulTiles < totalTiles) {
        int failedTiles = totalTiles - successfulTiles;
        qWarning() << "[AIEngine][Tiled] tiled inference incomplete"
                   << "successful:" << successfulTiles
                   << "failed:" << failedTiles
                   << "total:" << totalTiles;
        emitError(tr("Inference not fully completed (%1/%2 tiles successful)").arg(successfulTiles).arg(totalTiles));
        return QImage();
    }
    
    setProgress(0.95);

    return output;
}

QImage AIEngine::processWithTTA(const QImage &input, const ModelInfo &model)
{
    setProgress(0.02);
    
    QList<QImage> transformed;
    transformed.append(input);
    transformed.append(input.mirrored(true, false));
    transformed.append(input.mirrored(false, true));
    transformed.append(input.mirrored(true, true));
    QTransform rot90;
    rot90.rotate(90);
    QImage rotated90 = input.transformed(rot90);
    transformed.append(rotated90);
    transformed.append(rotated90.mirrored(true, false));
    transformed.append(rotated90.mirrored(false, true));
    transformed.append(rotated90.mirrored(true, true));

    setProgress(0.05);
    
    const int totalSteps = transformed.size();
    QList<QImage> results;

    for (int i = 0; i < totalSteps; ++i) {
        if (m_cancelRequested) {
            return QImage();
        }
        emit progressTextChanged(tr("TTA processing: %1/%2").arg(i + 1).arg(totalSteps));
        double stepProgress = 0.05 + 0.85 * static_cast<double>(i) / totalSteps;
        setProgress(stepProgress);

        QImage result = processSingle(transformed[i], model);
        if (result.isNull()) {
            qWarning() << "[AIEngine] TTA step" << (i + 1) << "failed, skipping";
            continue;
        }
        if (i >= 4) {
            QTransform rot270; rot270.rotate(270);
            result = result.transformed(rot270);
            if      (i == 5) result = result.mirrored(true, false);
            else if (i == 6) result = result.mirrored(false, true);
            else if (i == 7) result = result.mirrored(true, true);
        } else {
            if      (i == 1) result = result.mirrored(true, false);
            else if (i == 2) result = result.mirrored(false, true);
            else if (i == 3) result = result.mirrored(true, true);
        }
        results.append(result);
    }

    if (results.isEmpty()) {
        qWarning() << "[AIEngine] TTA: all steps failed";
        return QImage();
    }
    
    setProgress(0.92);
    emit progressTextChanged(tr("Merging TTA results..."));
    QImage merged = mergeTTAResults(results);
    setProgress(0.95);
    return merged;
}

QImage AIEngine::mergeTTAResults(const QList<QImage> &results)
{
    if (results.isEmpty()) return QImage();
    int w = results[0].width();
    int h = results[0].height();
    QImage merged(w, h, QImage::Format_RGB888);
    merged.fill(Qt::black);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sumR = 0, sumG = 0, sumB = 0, count = 0;
            for (const auto &img : results) {
                if (x < img.width() && y < img.height()) {
                    QRgb pixel = img.pixel(x, y);
                    sumR += qRed(pixel); sumG += qGreen(pixel); sumB += qBlue(pixel);
                    count++;
                }
            }
            if (count > 0)
                merged.setPixel(x, y, qRgb(sumR/count, sumG/count, sumB/count));
        }
    }
    return merged;
}

QImage AIEngine::applyOutscale(const QImage &input, double scale)
{
    if (input.isNull() || scale <= 0) return input;
    if (std::abs(scale - 1.0) < 0.001) return input;
    int newWidth  = static_cast<int>(input.width()  * scale);
    int newHeight = static_cast<int>(input.height() * scale);
    return input.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// ========== 自动参数计算 ==========

QVariantMap AIEngine::computeAutoParams(const QSize &mediaSize, bool isVideo) const
{
    ModelInfo model;
    QString modelId;
    {
        QMutexLocker locker(&m_mutex);
        model   = m_currentModel;
        modelId = m_currentModelId;
    }
    return computeAutoParamsForModel(mediaSize, isVideo, model, modelId);
}

QVariantMap AIEngine::computeAutoParamsForModel(const QSize &mediaSize, bool isVideo,
                                                 const ModelInfo &model,
                                                 const QString &modelId) const
{
    QVariantMap result;
    if (!model.isLoaded && model.paramPath.isEmpty()) return result;

    const int w = mediaSize.width();
    const int h = mediaSize.height();
    const int maxDim = std::max(w, h);
    const qint64 pixels = static_cast<qint64>(w) * h;
    const double aspectRatio = (h > 0) ? static_cast<double>(w) / h : 1.0;
    const bool isUltraWide = (aspectRatio >= 2.0);
    const bool isPortrait  = (aspectRatio <= 0.7);

    const int autoTile = computeAutoTileSizeForModel(mediaSize, model);
    result["tileSize"] = autoTile;

    auto clampParam = [&](const QString &key, double rawVal) -> double {
        if (!model.supportedParams.contains(key)) return rawVal;
        const QVariantMap &meta = model.supportedParams[key].toMap();
        double lo = meta.contains("min") ? meta["min"].toDouble() : -1e9;
        double hi = meta.contains("max") ? meta["max"].toDouble() :  1e9;
        return std::clamp(rawVal, lo, hi);
    };

    switch (model.category) {
    case ModelCategory::SuperResolution: {
        const double modelScale = static_cast<double>(model.scaleFactor);
        if (model.supportedParams.contains("outscale")) {
            double s = modelScale;
            if (pixels > 2073600)     s = std::max(1.0, modelScale * 0.5);
            else if (pixels > 921600) s = std::max(1.0, modelScale * 0.75);
            s = std::round(s * 2.0) / 2.0;
            result["outscale"] = clampParam("outscale", s);
        }
        if (model.supportedParams.contains("tta_mode"))     result["tta_mode"]     = (!isVideo && pixels <= 262144);
        if (model.supportedParams.contains("face_enhance")) result["face_enhance"] = !isVideo;
        if (model.supportedParams.contains("uhd_mode"))     result["uhd_mode"]     = (pixels >= 8294400);
        if (model.supportedParams.contains("fp32"))         result["fp32"]         = (pixels <= 262144 && !isVideo);
        if (model.supportedParams.contains("denoise")) {
            float dn = isVideo ? 0.3f : (pixels < 65536 ? 0.5f : 0.2f);
            result["denoise"] = static_cast<double>(clampParam("denoise", dn));
        }
        break;
    }
    case ModelCategory::Denoising: {
        if (model.supportedParams.contains("noise_threshold")) {
            float t = 50.0f;
            if      (pixels < 65536)   t = 70.0f;
            else if (pixels < 262144)  t = 60.0f;
            else if (pixels > 2073600) t = 35.0f;
            if (isVideo) t = std::min(t + 10.0f, 80.0f);
            result["noise_threshold"] = static_cast<double>(clampParam("noise_threshold", t));
        }
        if (model.supportedParams.contains("noise_level")) {
            int lv = isVideo ? 2 : 3;
            if      (pixels < 65536)   lv = isVideo ? 3 : 4;
            else if (pixels > 2073600) lv = isVideo ? 1 : 2;
            result["noise_level"] = static_cast<int>(clampParam("noise_level", lv));
        }
        if (model.supportedParams.contains("color_denoise"))      result["color_denoise"]      = !isVideo;
        if (model.supportedParams.contains("sharpness_preserve"))  result["sharpness_preserve"] = (pixels >= 409600);
        break;
    }
    case ModelCategory::Deblurring: {
        if (model.supportedParams.contains("deblur_strength")) {
            float s = 1.0f;
            if      (pixels < 65536)   s = isVideo ? 1.0f : 1.3f;
            else if (pixels > 2073600) s = isVideo ? 0.6f : 0.8f;
            else if (isVideo)          s = 0.8f;
            result["deblur_strength"] = static_cast<double>(clampParam("deblur_strength", s));
        }
        if (model.supportedParams.contains("iterations"))   result["iterations"]   = static_cast<int>(clampParam("iterations", isVideo ? 1.0 : 2.0));
        if (model.supportedParams.contains("motion_blur"))  result["motion_blur"]  = isVideo;
        break;
    }
    case ModelCategory::Dehazing: {
        if (model.supportedParams.contains("dehaze_strength")) {
            float s = 1.0f;
            if (isVideo && (isUltraWide || isPortrait)) s = 1.1f;
            result["dehaze_strength"] = static_cast<double>(clampParam("dehaze_strength", s));
        }
        if (model.supportedParams.contains("sky_protect"))   result["sky_protect"]   = (pixels >= 409600);
        if (model.supportedParams.contains("color_correct")) result["color_correct"]  = true;
        break;
    }
    case ModelCategory::Colorization: {
        if (model.supportedParams.contains("render_factor")) {
            int rf = 35;
            if      (maxDim >= 1920) rf = isVideo ? 38 : 45;
            else if (maxDim >= 1280) rf = isVideo ? 35 : 40;
            else if (maxDim <= 256)  rf = 20;
            result["render_factor"] = static_cast<int>(clampParam("render_factor", rf));
        }
        if (model.supportedParams.contains("artistic_mode"))        result["artistic_mode"]        = !isVideo;
        if (model.supportedParams.contains("temporal_consistency")) result["temporal_consistency"] = isVideo;
        if (model.supportedParams.contains("saturation_boost")) {
            float sat = (pixels > 921600) ? 0.8f : 1.0f;
            result["saturation_boost"] = static_cast<double>(clampParam("saturation_boost", sat));
        }
        break;
    }
    case ModelCategory::LowLight: {
        if (model.supportedParams.contains("enhancement_strength")) {
            float es = 1.0f;
            if      (pixels < 65536)   es = isVideo ? 1.1f : 1.3f;
            else if (pixels > 2073600) es = isVideo ? 0.8f : 0.9f;
            else if (isVideo)          es = 0.9f;
            result["enhancement_strength"] = static_cast<double>(clampParam("enhancement_strength", es));
        }
        if (model.supportedParams.contains("exposure_correction")) result["exposure_correction"] = true;
        if (model.supportedParams.contains("noise_suppression"))   result["noise_suppression"]   = isVideo || (pixels < 262144);
        if (model.supportedParams.contains("gamma_correction"))    result["gamma_correction"]    = true;
        break;
    }
    case ModelCategory::FrameInterpolation: {
        if (model.supportedParams.contains("time_step"))       result["time_step"]       = static_cast<double>(clampParam("time_step", 0.5));
        if (model.supportedParams.contains("uhd_mode"))        result["uhd_mode"]        = (pixels >= 8294400);
        if (model.supportedParams.contains("tta_spatial"))     result["tta_spatial"]     = false;
        if (model.supportedParams.contains("tta_temporal"))    result["tta_temporal"]    = false;
        if (model.supportedParams.contains("scale"))           result["scale"]           = static_cast<int>(clampParam("scale", 1.0));
        if (model.supportedParams.contains("scene_detection")) result["scene_detection"] = true;
        break;
    }
    case ModelCategory::Inpainting: {
        if (model.supportedParams.contains("inpaint_radius")) {
            int r = 3;
            if      (maxDim >= 1920) r = 5;
            else if (maxDim >= 1280) r = 4;
            else if (maxDim <= 256)  r = 2;
            result["inpaint_radius"] = static_cast<int>(clampParam("inpaint_radius", r));
        }
        if (model.supportedParams.contains("inpaint_method"))
            result["inpaint_method"] = static_cast<int>(clampParam("inpaint_method",
                (!isVideo && pixels >= 262144) ? 1.0 : 0.0));
        if (model.supportedParams.contains("feather_edge")) result["feather_edge"] = (pixels >= 262144);
        break;
    }
    default: break;
    }

    return result;
}

int AIEngine::computeAutoTileSize(const QSize &inputSize) const
{
    QMutexLocker locker(&m_mutex);
    return computeAutoTileSizeForModel(inputSize, m_currentModel);
}

int AIEngine::computeAutoTileSizeForModel(const QSize &inputSize, const ModelInfo &model) const
{
    if (!model.isLoaded && model.paramPath.isEmpty()) return 0;
    if (model.tileSize == 0) return 0;

    const int w = inputSize.width();
    const int h = inputSize.height();
    const int scale    = std::max(1, model.scaleFactor);
    const int channels = model.outputChannels > 0 ? model.outputChannels : 3;

    double kFactor = 8.0;
    
    const int layerCount = model.layerCount;
    
    if (layerCount > 500) {
        kFactor = 48.0;
    } else if (layerCount > 200) {
        kFactor = 32.0;
    } else if (layerCount > 50) {
        kFactor = 16.0;
    } else if (layerCount > 0) {
        kFactor = 10.0;
    }
    else if (model.sizeBytes > 50LL * 1024 * 1024) {
        kFactor = 48.0;
    } else if (model.sizeBytes > 20LL * 1024 * 1024) {
        kFactor = 32.0;
    } else if (model.sizeBytes > 5LL * 1024 * 1024) {
        kFactor = 16.0;
    }
    
    constexpr qint64 kBytesPerMB = 1024LL * 1024;

    qint64 availableMB = 256;

    auto memForTile = [&](int tile) -> double {
        const qint64 px    = static_cast<qint64>(tile) * tile;
        const qint64 bytes = px * channels
                             * static_cast<qint64>(sizeof(float))
                             * static_cast<qint64>(scale * scale);
        return static_cast<double>(bytes) * kFactor / kBytesPerMB;
    };

    const int maxPossibleTile = std::max(w, h);
    const int minTile = 64;
    const int maxTile = std::min(512, maxPossibleTile);
    const int step    = 32;

    int bestTile = minTile;
    for (int t = maxTile; t >= minTile; t -= step) {
        if (memForTile(t) <= static_cast<double>(availableMB)) {
            bestTile = t;
            break;
        }
    }

    if (w <= bestTile && h <= bestTile) {
        return 0;
    }

    return bestTile;
}

// ========== Vulkan 实例单例化管理实现 ==========

void AIEngine::ensureGpuInstanceCreated()
{
#ifdef NCNN_VULKAN_AVAILABLE
    std::call_once(s_gpuInstanceOnceFlag, []() {
        qInfo() << "[AIEngine] Creating global Vulkan GPU instance (once)";
        int ret = ncnn::create_gpu_instance();
        if (ret == 0) {
            s_gpuInstanceCreated.store(true);
            qInfo() << "[AIEngine] Global Vulkan GPU instance created successfully";
        } else {
            qWarning() << "[AIEngine] Failed to create global Vulkan GPU instance, code:" << ret;
            s_gpuInstanceCreated.store(false);
        }
    });
    
    if (s_gpuInstanceCreated.load()) {
        std::lock_guard<std::mutex> lock(s_gpuInstanceMutex);
        s_gpuInstanceRefCount++;
        qInfo() << "[AIEngine] GPU instance ref count incremented to:" << s_gpuInstanceRefCount.load();
    }
#endif
}

void AIEngine::releaseGpuInstanceRef()
{
#ifdef NCNN_VULKAN_AVAILABLE
    if (s_gpuInstanceCreated.load()) {
        std::lock_guard<std::mutex> lock(s_gpuInstanceMutex);
        if (s_gpuInstanceRefCount > 0) {
            s_gpuInstanceRefCount--;
            qInfo() << "[AIEngine] GPU instance ref count decremented to:" << s_gpuInstanceRefCount.load();
        }
    }
#endif
}

bool AIEngine::isGpuInstanceCreated()
{
    return s_gpuInstanceCreated.load();
}

// ========== 模型加载同步实现 ==========

bool AIEngine::waitForModelSyncComplete(int timeoutMs)
{
    if (m_modelSyncComplete.load()) {
        return true;
    }
    
    std::unique_lock<std::mutex> lock(m_modelSyncMutex);
    return m_modelSyncCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
        [this] { return m_modelSyncComplete.load(); });
}

// ========== GPU 就绪等待实现 ==========

bool AIEngine::waitForGpuReady(int timeoutMs)
{
    if (m_gpuReady.load()) {
        return true;
    }
    
    std::unique_lock<std::mutex> lock(m_gpuReadyMutex);
    return m_gpuReadyCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
        [this] { return m_gpuReady.load(); });
}

} // namespace EnhanceVision
