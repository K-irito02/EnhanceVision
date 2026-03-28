/**
 * @file AIEngine.cpp
 * @brief NCNN AI 推理引擎实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include "EnhanceVision/core/ProgressReporter.h"
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

namespace EnhanceVision {

AIEngine::AIEngine(QObject *parent)
    : QObject(parent)
    , m_progressReporter(std::make_unique<ProgressReporter>(this))
{
    initVulkan();
}

AIEngine::~AIEngine()
{
    unloadModel();
    destroyVulkan();
}

// ========== Vulkan 初始化 ==========

void AIEngine::initVulkan()
{
#if NCNN_VULKAN
    ncnn::create_gpu_instance();

    if (ncnn::get_gpu_count() > 0) {
        m_gpuAvailable = true;
        int gpuIndex = ncnn::get_default_gpu_index();
        m_vkdev = ncnn::get_gpu_device(gpuIndex);
        emit gpuAvailableChanged(true);
    } else {
        m_gpuAvailable = false;
        qWarning() << "[AIEngine] No Vulkan GPU found, using CPU mode";
        emit gpuAvailableChanged(false);
    }
#else
    m_gpuAvailable = false;
    qWarning() << "[AIEngine] NCNN built without Vulkan support, using CPU mode";
    emit gpuAvailableChanged(false);
#endif

    updateOptions();
}

void AIEngine::destroyVulkan()
{
#if NCNN_VULKAN
    if (m_blobVkAllocator && m_vkdev) {
        m_vkdev->reclaim_blob_allocator(m_blobVkAllocator);
        m_blobVkAllocator = nullptr;
    }
    if (m_stagingVkAllocator && m_vkdev) {
        m_vkdev->reclaim_staging_allocator(m_stagingVkAllocator);
        m_stagingVkAllocator = nullptr;
    }
    m_opt.blob_vkallocator = nullptr;
    m_opt.workspace_vkallocator = nullptr;
    m_vkdev = nullptr;
    ncnn::destroy_gpu_instance();
#endif
}

void AIEngine::updateOptions()
{
    m_opt.num_threads = std::max(1, QThread::idealThreadCount() - 1);
    m_opt.use_packing_layout = true;

#if NCNN_VULKAN
    // 释放旧的 allocator，避免显存泄漏
    if (m_blobVkAllocator && m_vkdev) {
        m_vkdev->reclaim_blob_allocator(m_blobVkAllocator);
        m_blobVkAllocator = nullptr;
    }
    if (m_stagingVkAllocator && m_vkdev) {
        m_vkdev->reclaim_staging_allocator(m_stagingVkAllocator);
        m_stagingVkAllocator = nullptr;
    }
    m_opt.blob_vkallocator = nullptr;
    m_opt.workspace_vkallocator = nullptr;

    if (m_gpuAvailable && m_useGpu && m_vkdev) {
        m_opt.use_vulkan_compute = true;
        m_blobVkAllocator    = m_vkdev->acquire_blob_allocator();
        m_stagingVkAllocator = m_vkdev->acquire_staging_allocator();
        m_opt.blob_vkallocator      = m_blobVkAllocator;
        m_opt.workspace_vkallocator = m_stagingVkAllocator;
    } else {
        m_opt.use_vulkan_compute = false;
    }
#else
    m_opt.use_vulkan_compute = false;
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

    // 若推理正在进行，拒绝切换模型
    if (m_isProcessing.load()) {
        qWarning() << "[AIEngine][loadModel] rejected: inference in progress, modelId:" << modelId;
        emit processError(tr("推理进行中，无法切换模型"));
        return false;
    }

    if (!m_modelRegistry) {
        emit processError(tr("ModelRegistry 未初始化"));
        return false;
    }

    if (!m_modelRegistry->hasModel(modelId)) {
        emit processError(tr("模型未注册: %1").arg(modelId));
        return false;
    }

    ModelInfo info = m_modelRegistry->getModelInfo(modelId);

    // OpenCV inpainting 模型不需要加载 NCNN 网络
    if (info.paramPath.isEmpty() && info.binPath.isEmpty()) {
        m_currentModelId = modelId;
        m_currentModel = info;
        emit modelLoaded(modelId);
        emit modelChanged();
        return true;
    }

    if (!info.isAvailable) {
        emit processError(tr("模型文件不可用: %1").arg(modelId));
        return false;
    }

    // 如果已经加载了相同模型，跳过
    if (m_currentModelId == modelId && m_currentModel.isLoaded) {
        return true;
    }

    // 卸载旧模型并清理 GPU 内存
    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_currentModel.isLoaded = false;
        
        // 清理 GPU 内存，防止切换模型时 GPU 状态损坏导致崩溃
#if NCNN_VULKAN
        if (m_vkdev) {
            ncnn::VkAllocator* blobAlloc = m_vkdev->acquire_blob_allocator();
            if (blobAlloc) {
                blobAlloc->clear();
                m_vkdev->reclaim_blob_allocator(blobAlloc);
            }
            ncnn::VkAllocator* stagingAlloc = m_vkdev->acquire_staging_allocator();
            if (stagingAlloc) {
                stagingAlloc->clear();
                m_vkdev->reclaim_staging_allocator(stagingAlloc);
            }
            qInfo() << "[AIEngine][loadModel] GPU memory cleared before loading new model";
        }
#endif
    }

    // 重置 GPU OOM 状态（换模型时重置）
    m_gpuOomDetected.store(false);

    // 加载新模型
    m_net.opt = m_opt;

    qInfo() << "[AIEngine][loadModel] loading model:" << modelId;

    int ret = m_net.load_param(info.paramPath.toStdString().c_str());
    if (ret != 0) {
        emit processError(tr("加载模型参数失败: %1").arg(info.paramPath));
        return false;
    }

    ret = m_net.load_model(info.binPath.toStdString().c_str());
    if (ret != 0) {
        m_net.clear();
        emit processError(tr("加载模型权重失败: %1").arg(info.binPath));
        return false;
    }

    qInfo() << "[AIEngine][loadModel] model loaded:" << modelId;

    m_currentModelId = modelId;
    m_currentModel = info;
    m_currentModel.isLoaded = true;
    m_currentModel.layerCount = loadedLayerCount;

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
            error = tr("模型加载失败: %1").arg(modelId);
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
        m_gpuOomDetected.store(false);
        emit modelUnloaded();
        emit modelChanged();
    }
}

// ========== 推理接口 ==========

QImage AIEngine::process(const QImage &input)
{
    // 推理互斥：同一时刻只允许一次推理
    QMutexLocker inferenceLocker(&m_inferenceMutex);

    // 检查模型状态
    if (m_currentModelId.isEmpty() || !m_currentModel.isLoaded) {
        if (!m_currentModelId.isEmpty() && m_currentModel.paramPath.isEmpty()) {
            // OpenCV 类模型不需要 isLoaded 标记
        } else {
            emitError(tr("未加载模型"));
            return QImage();
        }
    }

    if (input.isNull()) {
        emitError(tr("输入图像为空"));
        return QImage();
    }

    // 输入尺寸合法性检查
    if (input.width() <= 0 || input.height() <= 0) {
        emitError(tr("输入图像尺寸无效: %1x%2").arg(input.width()).arg(input.height()));
        return QImage();
    }

    // 快照当前模型副本（避免推理过程中 loadModel 修改 m_currentModel 导致数据竞争）
    ModelInfo currentModel;
    QString currentModelId;
    {
        QMutexLocker modelLocker(&m_mutex);
        currentModel = m_currentModel;
        currentModelId = m_currentModelId;
    }

    // 快照当前参数（避免推理过程中 setParameter 导致数据竞争）
    QVariantMap effectiveParams;
    {
        QMutexLocker paramLocker(&m_paramsMutex);
        effectiveParams = getEffectiveParamsLocked(currentModel);
    }

    // 分块大小决策
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

    // 极大图像安全检查：若 tileSize==0 且图像超过安全阈值，强制启用分块
    QImage workInput = input;
    if (tileSize == 0 && currentModel.tileSize > 0) {
        const qint64 pixels = static_cast<qint64>(input.width()) * input.height();
        if (pixels > 1024LL * 1024) {
            tileSize = currentModel.tileSize;
        }
    }

    // 进一步安全检查：防止超大输入图像在无分块时 OOM 崩溃
    // 无分块推理时最大安全像素数为 2048×2048（约 16 MB float RGB）
    if (tileSize == 0) {
        const qint64 pixels = static_cast<qint64>(workInput.width()) * workInput.height();
        const qint64 kMaxSafePixels = 2048LL * 2048;
        if (pixels > kMaxSafePixels && currentModel.tileSize > 0) {
            tileSize = currentModel.tileSize;
            qWarning() << "[AIEngine] Safety fallback: tileSize set to" << tileSize
                       << "for oversized input" << workInput.width() << "x" << workInput.height();
        }
    }

    // ── GPU OOM 自动降级：若上次推理检测到显存不足，强制启用分块 ──────────────
    if (m_gpuOomDetected.load() && tileSize == 0) {
        // 使用模型推荐分块，若模型无推荐则用保守值 200
        tileSize = (currentModel.tileSize > 0) ? currentModel.tileSize : 200;
        qWarning() << "[AIEngine] GPU OOM previously detected: forcing tileSize=" << tileSize;
    }

    bool needTile = (tileSize > 0) &&
                    (workInput.width() > tileSize || workInput.height() > tileSize);

    ModelInfo effectiveModel = currentModel;
    if (tileSize > 0) {
        effectiveModel.tileSize = tileSize;
    }

    QImage result;
    if (ttaMode && !needTile) {
        result = processWithTTA(workInput, effectiveModel);
    } else if (needTile) {
        result = processTiled(workInput, effectiveModel);
    } else {
        result = processSingle(workInput, effectiveModel);
    }

    // ── GPU OOM 自动降级重试：推理失败且是 GPU OOM，以更小分块模式多次重试 ────────
    if (result.isNull() && m_gpuOomDetected.load()) {
        // 保存当前进度，防止重试时进度条倒退
        const double progressBeforeRetry = m_progress.load();
        
        // 尝试一系列递减的分块大小，从较保守的值开始
        const int fallbackTiles[] = {128, 96, 64};
        const int numFallbacks = sizeof(fallbackTiles) / sizeof(fallbackTiles[0]);
        
        for (int i = 0; i < numFallbacks && result.isNull(); ++i) {
            int fallbackTile = fallbackTiles[i];
            qWarning() << "[AIEngine] OOM retry attempt" << (i + 1) << "with tileSize=" << fallbackTile;
            
            // 尝试清理 GPU 内存
#if NCNN_VULKAN
            if (m_vkdev) {
                ncnn::VkAllocator* allocator = m_vkdev->acquire_blob_allocator();
                if (allocator) {
                    allocator->clear();
                    m_vkdev->reclaim_blob_allocator(allocator);
                }
                allocator = m_vkdev->acquire_staging_allocator();
                if (allocator) {
                    allocator->clear();
                    m_vkdev->reclaim_staging_allocator(allocator);
                }
            }
#endif
            // 重置 OOM 标志以便重试
            m_gpuOomDetected.store(false);
            
            effectiveModel.tileSize = fallbackTile;
            result = processTiled(workInput, effectiveModel);
            
            if (!result.isNull()) {
                qInfo() << "[AIEngine] OOM retry succeeded with tileSize=" << fallbackTile;
                break;
            }
        }
        
        // 如果所有 GPU 重试都失败，考虑 CPU 回退（如果启用了 GPU）
        if (result.isNull() && m_useGpu) {
            qWarning() << "[AIEngine] All GPU OOM retries failed, consider using CPU mode";
        }
    }

    if (!result.isNull() && std::abs(outscale - currentModel.scaleFactor) > 0.01) {
        result = applyOutscale(result, outscale / currentModel.scaleFactor);
    }

    setProgress(1.0);
    setProcessing(false);

    // 释放互斥锁后发出 UI 通知信号（避免持锁期间触发重入）
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
    // 检查是否已有推理在进行（使用 m_isProcessing 标志，避免锁竞争问题）
    if (m_isProcessing.load()) {
        emit processError(tr("已有推理任务正在进行"));
        emit processFileCompleted(false, QString(), tr("已有推理任务正在进行"));
        return;
    }

    // ── 在派发到工作线程之前，在主线程快照模型和参数 ─────────────────────────
    // 这样工作线程就不需要访问任何需要主线程保护的对象
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

    // 标记推理开始（在工作线程启动前设置，避免竞争）
    setProcessing(true);
    setProgress(0.0);
    m_cancelRequested = false;

    QtConcurrent::run([this, inputPath, outputPath, snapshotParams]() {
        QElapsedTimer perfTimer;
        perfTimer.start();

        QImage inputImage(inputPath);
        if (inputImage.isNull()) {
            QString error = tr("无法读取图像: %1").arg(inputPath);
            qWarning() << "[AIEngine][processAsync]" << error;
            setProcessing(false);
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        // 格式转换保障：确保输入为支持格式
        if (inputImage.format() == QImage::Format_Invalid) {
            QString error = tr("图像格式无效: %1").arg(inputPath);
            setProcessing(false);
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        const qint64 loadInputCostMs = perfTimer.elapsed();

        // 捕获推理过程中的错误信息
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
            emit processFileCompleted(false, QString(), tr("推理已取消"));
            return;
        }

        if (result.isNull()) {
            QString error = lastError.isEmpty() ? tr("推理失败，请检查模型兼容性和输入图像") : lastError;
            qWarning() << "[AIEngine] inference failed:" << error;
            setProcessing(false);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        if (!result.save(outputPath)) {
            QString error = tr("无法保存结果: %1").arg(outputPath);
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
    m_cancelRequested = true;
    setProcessing(false);
}

void AIEngine::forceCancel()
{
    m_forceCancelled = true;
    m_cancelRequested = true;
    // 立即重置 m_isProcessing 标志，允许新任务启动
    setProcessing(false);
    qWarning() << "[AIEngine] Force cancel requested, m_isProcessing reset";
}

bool AIEngine::isForceCancelled() const
{
    return m_forceCancelled.load();
}

// ========== OpenCV Inpainting (Qt 原生实现) ==========

QImage AIEngine::inpaint(const QImage &input, const QImage &mask, int radius, int method)
{
    Q_UNUSED(method)

    if (input.isNull() || mask.isNull()) {
        emit processError(tr("修复输入无效"));
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
        emit processError(tr("已有任务正在进行"));
        emit processFileCompleted(false, QString(), tr("已有任务正在进行"));
        return;
    }
    QtConcurrent::run([this, inputPath, maskPath, outputPath, radius, method]() {
        setProcessing(true);
        setProgress(0.0);
        QImage input(inputPath), mask(maskPath);
        if (input.isNull() || mask.isNull()) {
            setProcessing(false);
            emit processFileCompleted(false, QString(), tr("无法读取图像: %1").arg(
                input.isNull() ? inputPath : maskPath));
            return;
        }
        setProgress(0.2);
        QImage result = inpaint(input, mask, radius, method);
        setProgress(0.8);
        if (result.isNull()) { setProcessing(false); emit processFileCompleted(false, QString(), tr("修复失败")); return; }
        if (!result.save(outputPath)) { setProcessing(false); emit processFileCompleted(false, QString(), tr("保存失败: %1").arg(outputPath)); return; }
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

// ========== 状态查询 ==========

bool AIEngine::isProcessing() const { return m_isProcessing; }
double AIEngine::progress() const { return m_progress; }
QString AIEngine::currentModelId() const { return m_currentModelId; }
bool AIEngine::gpuAvailable() const { return m_gpuAvailable; }
bool AIEngine::useGpu() const { return m_useGpu; }

void AIEngine::setUseGpu(bool use)
{
    if (m_useGpu != use) {
        m_useGpu = use;
        updateOptions();
        if (!m_currentModelId.isEmpty() && m_currentModel.isLoaded) {
            QString modelId = m_currentModelId;
            unloadModel();
            loadModel(modelId);
        }
        emit useGpuChanged(m_useGpu);
    }
}

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

ncnn::Mat AIEngine::qimageToMat(const QImage &image, const ModelInfo &model)
{
    QImage img = (image.format() == QImage::Format_RGB888)
        ? image.copy()
        : image.convertToFormat(QImage::Format_RGB888);

    if (img.isNull() || img.width() <= 0 || img.height() <= 0) {
        qWarning() << "[AIEngine][qimageToMat] invalid converted image";
        return ncnn::Mat();
    }

    int w = img.width();
    int h = img.height();
    const int stride = img.bytesPerLine();

    // 使用带 stride 的重载，避免 Qt 行对齐导致的越界读取
    ncnn::Mat in = ncnn::Mat::from_pixels(img.constBits(), ncnn::Mat::PIXEL_RGB, w, h, stride);
    if (in.empty()) {
        qWarning() << "[AIEngine][qimageToMat] from_pixels failed";
        return ncnn::Mat();
    }

    in.substract_mean_normalize(model.normMean, model.normScale);
    return in;
}

QImage AIEngine::matToQimage(const ncnn::Mat &mat, const ModelInfo &model)
{
    // ── 严格空 mat 防护 ─────────────────────────────────────────────────────
    // ncnn::Mat::to_pixels 在 w/h/c==0 或 data==nullptr 时读取无效内存导致崩溃
    if (mat.empty() || mat.w <= 0 || mat.h <= 0 || mat.c <= 0 || mat.data == nullptr) {
        qWarning() << "[AIEngine][matToQimage] received empty/invalid mat, returning null QImage"
                   << "w:" << mat.w << "h:" << mat.h << "c:" << mat.c
                   << "data:" << (mat.data != nullptr ? "non-null" : "null");
        return QImage();
    }
    int w = mat.w, h = mat.h;
    ncnn::Mat out = mat.clone();
    if (out.empty() || out.data == nullptr) {
        qWarning() << "[AIEngine][matToQimage] mat.clone() produced empty mat";
        return QImage();
    }
    out.substract_mean_normalize(model.denormMean, model.denormScale);
    float *data = (float *)out.data;
    const int total = w * h * out.c;
    float minVal = data[0], maxVal = data[0];
    for (int i = 0; i < total; ++i) {
        data[i] = std::clamp(data[i], 0.f, 255.f);
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    QImage result(w, h, QImage::Format_RGB888);
    // 必须传入 stride（bytesPerLine），因为 Qt 可能对 scanline 做 4-byte 对齐，
    // 导致 bytesPerLine > w*3。不传 stride 会使每行累积偏移，产生斜条纹伪影。
    out.to_pixels(result.bits(), ncnn::Mat::PIXEL_RGB, result.bytesPerLine());
    return result;
}

ncnn::Mat AIEngine::runInference(const ncnn::Mat &input, const ModelInfo &model)
{
    if (input.empty() || input.w <= 0 || input.h <= 0) {
        qWarning() << "[AIEngine][Inference] received empty input mat"
                   << "w:" << input.w << "h:" << input.h << "c:" << input.c;
        return ncnn::Mat();
    }
    if (!model.isLoaded && !model.paramPath.isEmpty()) {
        qWarning() << "[AIEngine][Inference] model not loaded, cannot run inference";
        return ncnn::Mat();
    }

    QStringList inputCandidates;
    inputCandidates << model.inputBlobName << "input" << "data" << "in0" << "Input1";
    inputCandidates.removeAll(QString());
    inputCandidates.removeDuplicates();

    QStringList outputCandidates;
    outputCandidates << model.outputBlobName << "output" << "out0" << "prob";
    outputCandidates.removeAll(QString());
    outputCandidates.removeDuplicates();

    if (inputCandidates.isEmpty() || outputCandidates.isEmpty()) {
        qWarning() << "[AIEngine][Inference] empty blob candidate list"
                   << "inputCandidates:" << inputCandidates
                   << "outputCandidates:" << outputCandidates;
        return ncnn::Mat();
    }

    // 由于 m_mutex 与 loadModel/unloadModel 共享，推理期间也会持续持有
    // 但 loadModel 端会通过 m_isProcessing 检查被阻止，因此实际上不会发生死锁
    QMutexLocker netLocker(&m_mutex);
    ncnn::Extractor ex = m_net.create_extractor();
    ex.set_light_mode(true);

    QString selectedInputBlob;
    int inputRet = -1;
    for (const QString &candidate : inputCandidates) {
        inputRet = ex.input(candidate.toStdString().c_str(), input);
        if (inputRet == 0) {
            selectedInputBlob = candidate;
            break;
        }
    }

    if (inputRet != 0) {
        qWarning() << "[AIEngine][Inference] Failed to set input blob"
                   << "requested:" << model.inputBlobName
                   << "tried:" << inputCandidates
                   << "ret:" << inputRet;
        emitError(tr("推理输入失败，模型输入节点不匹配"));
        return ncnn::Mat();
    }

    ncnn::Mat output;
    int extractRet = -1;
    bool gpuOomSeen = false;
    QString selectedOutputBlob;
    
    for (const QString &candidate : outputCandidates) {
        ncnn::Mat tmp;
        extractRet = ex.extract(candidate.toStdString().c_str(), tmp);
        if (extractRet == -100) {
            gpuOomSeen = true;
        }
        if (extractRet == 0 && !tmp.empty()) {
            output = std::move(tmp);
            selectedOutputBlob = candidate;
            break;
        }
    }

    if (extractRet != 0 || output.empty()) {
        qWarning() << "[AIEngine][Inference] Failed to extract output blob"
                   << "requested:" << model.outputBlobName
                   << "tried:" << outputCandidates
                   << "extractRet:" << extractRet
                   << "gpuOomSeen:" << gpuOomSeen
                   << "empty:" << output.empty();
        // extractRet == -100 或任意候选节点返回 -100 均视为 Vulkan OOM，标记后供上层自动降级
        if (gpuOomSeen) {
            m_gpuOomDetected.store(true);
            qWarning() << "[AIEngine][Inference] GPU OOM detected (gpuOomSeen=true), flagging for tile fallback";
            emitError(tr("GPU 显存不足 (OOM)，将自动切换为分块模式重试"));
        } else {
            emitError(tr("推理输出失败，模型输出节点不匹配或设备不兼容 (ret=%1)").arg(extractRet));
        }
        return ncnn::Mat();
    }

    return output;
}

QImage AIEngine::processSingle(const QImage &input, const ModelInfo &model)
{
    setProgress(0.02);
    setProgress(0.05);
    ncnn::Mat in = qimageToMat(input, model);
    if (in.empty()) {
        qWarning() << "[AIEngine][Single] qimageToMat failed";
        return QImage();
    }
    if (m_cancelRequested) return QImage();
    setProgress(0.15);
    ncnn::Mat out = runInference(in, model);
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
    
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIEngine] invalid input image";
        return QImage();
    }

    setProgress(0.02);
    
    int tileSize = model.tileSize;
    int scale = model.scaleFactor;
    int w = input.width();
    int h = input.height();

    if (tileSize <= 0) {
        qWarning() << "[AIEngine] invalid tileSize, falling back to processSingle";
        return processSingle(input, model);
    }

    setProgress(0.03);

    int padding = model.tilePadding;
    const int layerCount = model.layerCount;
    if (layerCount > 500) {
        padding = std::max(padding, 64);
    } else if (layerCount > 200) {
        padding = std::max(padding, 48);
    } else if (layerCount > 50) {
        padding = std::max(padding, 24);
    }
    
    QImage normalizedInput;
    if (input.format() == QImage::Format_RGB888) {
        normalizedInput = input;
    } else {
        normalizedInput = input.convertToFormat(QImage::Format_RGB888);
    }

    if (normalizedInput.isNull() || normalizedInput.format() != QImage::Format_RGB888) {
        qWarning() << "[AIEngine] failed to normalize input format";
        return processSingle(input, model);
    }

    QImage paddedInput(w + 2 * padding, h + 2 * padding, QImage::Format_RGB888);
    if (paddedInput.isNull()) {
        qWarning() << "[AIEngine] failed to create padded image";
        return processSingle(input, model);
    }
    paddedInput.fill(Qt::black);
    
    // 使用正确的字节宽度进行复制
    const int srcBytesPerLine = normalizedInput.bytesPerLine();
    const int expectedBytesPerLine = w * 3;
    
    // 使用简单的像素复制代替 QPainter（更稳定）
    // 复制原图到中心位置
    for (int y = 0; y < h; ++y) {
        const uchar* srcLine = normalizedInput.constScanLine(y);
        uchar* dstLine = paddedInput.scanLine(y + padding);
        // 使用实际的字节宽度，而不是假定的 w * 3
        if (srcBytesPerLine >= expectedBytesPerLine) {
            std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
        } else {
            std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
            std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
        }
    }
    
    // 镜像填充边缘（使用直接像素操作）
    // 上边缘
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
    // 下边缘
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
    // 左边缘和右边缘（逐像素镜像）
    for (int y = 0; y < h; ++y) {
        uchar* dstLine = paddedInput.scanLine(y + padding);
        const uchar* srcLine = normalizedInput.constScanLine(y);
        // 左边缘镜像
        for (int x = 0; x < std::min(padding, w); ++x) {
            int srcX = x;
            int dstX = padding - 1 - x;
            dstLine[dstX * 3 + 0] = srcLine[srcX * 3 + 0];
            dstLine[dstX * 3 + 1] = srcLine[srcX * 3 + 1];
            dstLine[dstX * 3 + 2] = srcLine[srcX * 3 + 2];
        }
        // 右边缘镜像
        for (int x = 0; x < std::min(padding, w); ++x) {
            int srcX = w - 1 - x;
            int dstX = padding + w + x;
            dstLine[dstX * 3 + 0] = srcLine[srcX * 3 + 0];
            dstLine[dstX * 3 + 1] = srcLine[srcX * 3 + 1];
            dstLine[dstX * 3 + 2] = srcLine[srcX * 3 + 2];
        }
    }
    
    // 四个角落的镜像填充（避免黑色伪影导致失真）
    for (int y = 0; y < std::min(padding, h); ++y) {
        uchar* dstLineTop = paddedInput.scanLine(padding - 1 - y);
        uchar* dstLineBot = paddedInput.scanLine(padding + h + y);
        const uchar* srcLineTop = normalizedInput.constScanLine(y);
        const uchar* srcLineBot = normalizedInput.constScanLine(h - 1 - y);
        for (int x = 0; x < std::min(padding, w); ++x) {
            int dstXL = padding - 1 - x;
            int dstXR = padding + w + x;
            int srcXL = x;
            int srcXR = w - 1 - x;
            // 左上角
            dstLineTop[dstXL * 3 + 0] = srcLineTop[srcXL * 3 + 0];
            dstLineTop[dstXL * 3 + 1] = srcLineTop[srcXL * 3 + 1];
            dstLineTop[dstXL * 3 + 2] = srcLineTop[srcXL * 3 + 2];
            // 右上角
            dstLineTop[dstXR * 3 + 0] = srcLineTop[srcXR * 3 + 0];
            dstLineTop[dstXR * 3 + 1] = srcLineTop[srcXR * 3 + 1];
            dstLineTop[dstXR * 3 + 2] = srcLineTop[srcXR * 3 + 2];
            // 左下角
            dstLineBot[dstXL * 3 + 0] = srcLineBot[srcXL * 3 + 0];
            dstLineBot[dstXL * 3 + 1] = srcLineBot[srcXL * 3 + 1];
            dstLineBot[dstXL * 3 + 2] = srcLineBot[srcXL * 3 + 2];
            // 右下角
            dstLineBot[dstXR * 3 + 0] = srcLineBot[srcXR * 3 + 0];
            dstLineBot[dstXR * 3 + 1] = srcLineBot[srcXR * 3 + 1];
            dstLineBot[dstXR * 3 + 2] = srcLineBot[srcXR * 3 + 2];
        }
    }
    
    // 使用扩展后的图像进行分块处理
    const int paddedW = paddedInput.width();
    const int paddedH = paddedInput.height();

    int tilesX = (w + tileSize - 1) / tileSize;
    int tilesY = (h + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;

    qInfo() << "[AIEngine][Tiled] start processing"
            << "input:" << w << "x" << h
            << "tileSize:" << tileSize
            << "padding:" << padding
            << "scale:" << scale
            << "tiles:" << tilesX << "x" << tilesY
            << "total:" << totalTiles
            << "outputSize:" << (w * scale) << "x" << (h * scale)
            << "paddedInput:" << paddedW << "x" << paddedH;

    setProgress(0.10);
    
    QImage output(w * scale, h * scale, QImage::Format_RGB888);
    output.fill(Qt::black);
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    
    setProgress(0.15);

    qInfo() << "[AIEngine][Tiled] processing"
            << "input:" << w << "x" << h
            << "output:" << (w * scale) << "x" << (h * scale)
            << "tiles:" << tilesX << "x" << tilesY << "=" << totalTiles
            << "tileSize:" << tileSize << "padding:" << padding;

    int tileIndex = 0;
    int successfulTiles = 0;
    int consecutiveFailures = 0;
    const int kMaxConsecutiveFailures = 2;

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            if (m_cancelRequested) { painter.end(); return QImage(); }

            // 快速失败：如果连续多个分块失败（可能是 GPU OOM 或状态损坏），立即返回
            if (consecutiveFailures >= kMaxConsecutiveFailures) {
                qWarning() << "[AIEngine][Tiled] fast-fail: too many consecutive failures"
                           << "(" << consecutiveFailures << "), aborting tiled processing";
                painter.end();
                emitError(tr("分块处理连续失败，可能是显存不足"));
                return QImage();
            }

            // 原始图像中的分块位置（用于最终输出定位）
            int x0 = tx * tileSize;
            int y0 = ty * tileSize;
            int x1 = std::min(x0 + tileSize, w);
            int y1 = std::min(y0 + tileSize, h);
            int actualTileW = x1 - x0;
            int actualTileH = y1 - y0;

            // 在 paddedInput 中的提取位置（所有分块都有完整 padding）
            // paddedInput 中，原图从 (padding, padding) 开始
            int px0 = x0;  // 在 paddedInput 中 = 原始 x0（因为左边已经有 padding 像素的填充）
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
                
                if (m_gpuOomDetected.load()) {
                    qWarning() << "[AIEngine][Tiled] GPU OOM detected, aborting to allow retry with smaller tiles";
                    painter.end();
                    return QImage();
                }
                continue;
            }

            QImage tileResult = matToQimage(out, model);
            if (tileResult.isNull()) {
                qWarning() << "[AIEngine][Tiled] tile" << tileIndex << "matToQimage failed";
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            // 动态计算裁剪位置：基于实际模型输出尺寸，而非假设 output = input * scale
            // 某些模型内部有额外 padding/rounding，输出尺寸可能不是精确的 input * scale
            const int expectedOutW = extractW * scale;
            const int expectedOutH = extractH * scale;
            const int actualOutW = tileResult.width();
            const int actualOutH = tileResult.height();

            // 计算实际 padding 在输出中的像素数
            int outPadLeft, outPadTop, outW, outH;
            if (actualOutW == expectedOutW && actualOutH == expectedOutH) {
                // 精确匹配：直接使用理论值
                outPadLeft = padding * scale;
                outPadTop  = padding * scale;
                outW = actualTileW * scale;
                outH = actualTileH * scale;
            } else {
                // 模型输出尺寸与预期不同：按比例动态计算裁剪区域
                double scaleX = static_cast<double>(actualOutW) / extractW;
                double scaleY = static_cast<double>(actualOutH) / extractH;
                outPadLeft = static_cast<int>(std::round(padding * scaleX));
                outPadTop  = static_cast<int>(std::round(padding * scaleY));
                outW = static_cast<int>(std::round(actualTileW * scaleX));
                outH = static_cast<int>(std::round(actualTileH * scaleY));
            }

            // 安全边界修正
            outW = std::min(outW, actualOutW - outPadLeft);
            outH = std::min(outH, actualOutH - outPadTop);

            if (outPadLeft < 0 || outPadTop < 0 || outW <= 0 || outH <= 0 ||
                outPadLeft + outW > actualOutW || outPadTop + outH > actualOutH) {
                qWarning() << "[AIEngine][Tiled] tile" << tileIndex
                           << "crop region out of bounds, tileResult:" << tileResult.size()
                           << "expected:" << expectedOutW << "x" << expectedOutH
                           << "crop:(" << outPadLeft << "," << outPadTop << ") size:" << outW << "x" << outH;
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            QImage croppedResult = tileResult.copy(outPadLeft, outPadTop, outW, outH);

            // 目标绘制区域
            int dstX = x0 * scale;
            int dstY = y0 * scale;
            int dstW = actualTileW * scale;
            int dstH = actualTileH * scale;

            // 若裁剪尺寸与目标不一致，缩放到目标尺寸保证无缝拼接
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
        emitError(tr("推理未完整完成（%1/%2 分块成功）").arg(successfulTiles).arg(totalTiles));
        return QImage();
    }
    
    setProgress(0.95);

    qInfo() << "[AIEngine][Tiled] all" << totalTiles << "tiles processed successfully";
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
            Q_UNUSED(i)
            return QImage();
        }
        emit progressTextChanged(tr("TTA 处理中: %1/%2").arg(i + 1).arg(totalSteps));
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
        qWarning() << "[AIEngine] TTA: all steps failed, returning null";
        return QImage();
    }
    
    setProgress(0.92);
    emit progressTextChanged(tr("合并 TTA 结果..."));
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
    Q_UNUSED(std::min(w, h));
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

    qInfo() << "[AIEngine][AutoParams]"
             << "model:" << modelId
             << "mediaSize:" << w << "x" << h
             << "isVideo:" << isVideo
             << "aspectRatio:" << aspectRatio
             << "isUltraWide:" << isUltraWide
             << "isPortrait:" << isPortrait
             << "category:" << static_cast<int>(model.category)
             << "result:" << result;
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

    // ── 根据模型复杂度动态调整 kFactor ──────────────────────────────────
    // kFactor 估算中间特征图占用显存与 I/O 张量的倍数关系
    // 方法1: 基于模型文件大小（权重越多通常中间层越多）
    // 方法2: 基于模型的层数（加载时缓存在 model.layerCount）
    double kFactor = 8.0;  // 基准值（轻量模型）
    
    // 使用缓存的层数（避免直接访问 m_net，确保线程安全）
    const int layerCount = model.layerCount;
    
    // 优先使用层数判断复杂度（更准确）
    if (layerCount > 500) {
        kFactor = 48.0;   // 超大模型（如 realesrgan_x4plus 999层）
    } else if (layerCount > 200) {
        kFactor = 32.0;   // 大模型（如 realesrgan_x4plus_anime 268层）
    } else if (layerCount > 50) {
        kFactor = 16.0;   // 中等模型
    } else if (layerCount > 0) {
        kFactor = 10.0;   // 轻量模型（如 animevideov3 41层）
    }
    // 回退: 基于文件大小判断（模型未加载时）
    else if (model.sizeBytes > 50LL * 1024 * 1024) {
        kFactor = 48.0;   // >50MB 模型
    } else if (model.sizeBytes > 20LL * 1024 * 1024) {
        kFactor = 32.0;   // >20MB 模型
    } else if (model.sizeBytes > 5LL * 1024 * 1024) {
        kFactor = 16.0;   // >5MB 模型
    }
    
    constexpr qint64 kBytesPerMB = 1024LL * 1024;

    // ── 估算可用显存（保守估计，避免触发 OOM）──────────────────────────
    // 注意：实际可用显存 < 物理显存，因为 OS/驱动/其他程序也占用
    qint64 availableMB = 0;
    if (m_gpuAvailable && m_useGpu) {
#if NCNN_VULKAN
        // 保守估计：假设只有 1GB 可用于推理（即使显卡有更多显存）
        // 因为 Vulkan 管线、纹理缓存、命令缓冲区等也占用显存
        if (m_vkdev) availableMB = 1024;
#endif
        if (availableMB <= 0) availableMB = 512;
    } else {
        availableMB = 256;  // CPU 模式更保守
    }

    // GPU OOM 已检测到：大幅降低可用显存估算
    if (m_gpuOomDetected.load()) {
        availableMB = std::max(128LL, availableMB / 4);
        qWarning() << "[AIEngine][AutoTile] GPU OOM flag active, reducing availableMB to" << availableMB;
    }

    auto memForTile = [&](int tile) -> double {
        const qint64 px    = static_cast<qint64>(tile) * tile;
        const qint64 bytes = px * channels
                             * static_cast<qint64>(sizeof(float))
                             * static_cast<qint64>(scale * scale);
        return static_cast<double>(bytes) * kFactor / kBytesPerMB;
    };

    const int maxPossibleTile = std::max(w, h);
    const int minTile = 64;
    const int maxTile = std::min(512, maxPossibleTile);  // 降低最大分块到 512
    const int step    = 32;  // 更细的步进，找到更精确的分块

    int bestTile = minTile;
    for (int t = maxTile; t >= minTile; t -= step) {
        if (memForTile(t) <= static_cast<double>(availableMB)) {
            bestTile = t;
            break;
        }
    }

    // 如果图像足够小，不需要分块
    if (w <= bestTile && h <= bestTile) {
        qInfo() << "[AIEngine][AutoTile] no tiling needed:"
                << "input:" << w << "x" << h
                << "bestTile:" << bestTile
                << "layers:" << layerCount
                << "sizeBytes:" << model.sizeBytes
                << "kFactor:" << kFactor;
        return 0;
    }

    qInfo() << "[AIEngine][AutoTile]"
            << "input:" << w << "x" << h
            << "scale:" << scale
            << "layers:" << layerCount
            << "sizeBytes:" << model.sizeBytes
            << "kFactor:" << kFactor
            << "availableMB:" << availableMB
            << "memForBestTile:" << memForTile(bestTile)
            << "bestTile:" << bestTile;
    return bestTile;
}

} // namespace EnhanceVision 