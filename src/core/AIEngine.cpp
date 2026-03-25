/**
 * @file AIEngine.cpp
 * @brief NCNN AI 推理引擎实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace EnhanceVision {

AIEngine::AIEngine(QObject *parent)
    : QObject(parent)
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

    qInfo() << "[AIEngine][loadModel] loading model:"
            << "id:" << modelId
            << "param:" << info.paramPath
            << "bin:" << info.binPath
            << "inputBlob:" << info.inputBlobName
            << "outputBlob:" << info.outputBlobName
            << "tileSize:" << info.tileSize
            << "scale:" << info.scaleFactor
            << "sizeBytes:" << info.sizeBytes
            << "sizeMB:" << (info.sizeBytes / (1024.0 * 1024.0))
            << "useVulkan:" << m_opt.use_vulkan_compute;

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

    const int loadedLayerCount = static_cast<int>(m_net.layers().size());
    qInfo() << "[AIEngine][loadModel] model loaded successfully"
            << "layers:" << loadedLayerCount;

    m_currentModelId = modelId;
    m_currentModel = info;
    m_currentModel.isLoaded = true;
    m_currentModel.layerCount = loadedLayerCount;

    emit modelLoaded(modelId);
    emit modelChanged();
    return true;
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

    setProcessing(true);
    setProgress(0.0);
    m_cancelRequested = false;

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

    qInfo() << "[AIEngine] process start"
            << "model:" << currentModelId
            << "input:" << input.size()
            << "gpuEnabled:" << (m_gpuAvailable && m_useGpu)
            << "tileSize:" << tileSize
            << "modelScale:" << currentModel.scaleFactor
            << "outscale:" << outscale
            << "tta:" << ttaMode;

    // 极大图像安全检查：若 tileSize==0 且图像超过安全阈值，强制启用分块
    QImage workInput = input;
    if (tileSize == 0 && currentModel.tileSize > 0) {
        const qint64 pixels = static_cast<qint64>(input.width()) * input.height();
        if (pixels > 1024LL * 1024) {
            tileSize = currentModel.tileSize;
            qInfo() << "[AIEngine] Large image override: forcing tileSize=" << tileSize
                    << "for input" << input.width() << "x" << input.height();
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
            qWarning() << "[AIEngine] OOM retry attempt" << (i + 1) << "with tileSize=" << fallbackTile
                       << "progressBeforeRetry:" << progressBeforeRetry;
            
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
    // 检查是否已有推理在进行
    if (!m_inferenceMutex.tryLock()) {
        emit processError(tr("已有推理任务正在进行"));
        emit processFileCompleted(false, QString(), tr("已有推理任务正在进行"));
        return;
    }
    m_inferenceMutex.unlock();

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

    QtConcurrent::run([this, inputPath, outputPath, snapshotParams]() {
        QElapsedTimer perfTimer;
        perfTimer.start();

        // ── 视频文件检测：视频走逐帧处理管线 ────────────────────────────────
        if (ImageUtils::isVideoFile(inputPath)) {
            qInfo() << "[AIEngine][processAsync] detected video input, routing to video pipeline:"
                    << inputPath;
            processVideoInternal(inputPath, outputPath);
            return;
        }

        QImage inputImage(inputPath);
        if (inputImage.isNull()) {
            QString error = tr("无法读取图像: %1").arg(inputPath);
            qWarning() << "[AIEngine][processAsync]" << error;
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        // 格式转换保障：确保输入为支持格式
        if (inputImage.format() == QImage::Format_Invalid) {
            QString error = tr("图像格式无效: %1").arg(inputPath);
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
            emit processFileCompleted(false, QString(), tr("推理已取消"));
            return;
        }

        if (result.isNull()) {
            QString error = lastError.isEmpty() ? tr("推理失败，请检查模型兼容性和输入图像") : lastError;
            qWarning() << "[AIEngine][processAsync] inference failed:" << error << "input:" << inputPath;
            emit processFileCompleted(false, QString(), error);
            return;
        }

        qInfo() << "[AIEngine][Save] saving result:"
                << "size:" << result.size()
                << "format:" << result.format()
                << "outputPath:" << outputPath;

        if (!result.save(outputPath)) {
            QString error = tr("无法保存结果: %1").arg(outputPath);
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        QImage savedCheck(outputPath);
        qInfo() << "[AIEngine][Save] saved image verification:"
                << "loaded:" << !savedCheck.isNull()
                << "size:" << savedCheck.size();

        const qint64 totalCostMs = perfTimer.elapsed();
        const qint64 saveCostMs = totalCostMs - loadInputCostMs - processCostMs;
        if (totalCostMs >= 24) {
            qInfo() << "[Perf][AIEngine] processAsync cost:" << totalCostMs << "ms"
                    << "load:" << loadInputCostMs << "ms"
                    << "infer:" << processCostMs << "ms"
                    << "save:" << saveCostMs << "ms"
                    << "input:" << inputPath;
        }

        emit processFileCompleted(true, outputPath, QString());
    });
}

void AIEngine::cancelProcess()
{
    m_cancelRequested = true;
}

void AIEngine::forceCancel()
{
    m_forceCancelled = true;
    m_cancelRequested = true;
    qWarning() << "[AIEngine] Force cancel requested";
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

// ========== 内部方法 ==========

void AIEngine::setProgress(double value, bool forceEmit)
{
    constexpr double kProgressEmitDelta = 0.01;
    constexpr qint64 kProgressEmitIntervalMs = 66;
    const double clampedValue = std::clamp(value, 0.0, 1.0);
    
    // 防止进度条倒退：只有在以下情况才更新进度
    // 1. 新值大于当前值（正常前进）
    // 2. 新值为 0（重置）或 1.0（完成）
    // 3. 强制发射
    double previous = m_progress.load();
    const bool isReset = (clampedValue < 0.01);
    const bool isComplete = (clampedValue >= 0.99);
    const bool isForward = (clampedValue > previous);
    
    if (!forceEmit && !isReset && !isComplete && !isForward) {
        // 进度在倒退且不是重置/完成，忽略此次更新
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
    out.to_pixels(result.bits(), ncnn::Mat::PIXEL_RGB);
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
    setProgress(0.1);
    qInfo() << "[AIEngine][Single] start processing"
            << "input:" << input.size()
            << "format:" << input.format();
    ncnn::Mat in = qimageToMat(input, model);
    if (in.empty()) {
        qWarning() << "[AIEngine][Single] qimageToMat failed";
        return QImage();
    }
    qInfo() << "[AIEngine][Single] input mat created"
            << "w:" << in.w << "h:" << in.h << "c:" << in.c;
    if (m_cancelRequested) return QImage();
    setProgress(0.3);
    ncnn::Mat out = runInference(in, model);
    qInfo() << "[AIEngine][Single] inference done output mat:"
            << "w:" << out.w << "h:" << out.h << "c:" << out.c;
    // 推理失败防护：runInference 已记录日志并 emitError
    if (out.empty() || out.w <= 0 || out.h <= 0) {
        return QImage();
    }
    if (m_cancelRequested) return QImage();
    setProgress(0.8);
    QImage result = matToQimage(out, model);
    qInfo() << "[AIEngine][Single] result created"
            << "size:" << result.size()
            << "format:" << result.format()
            << "isNull:" << result.isNull();
    setProgress(0.95);
    return result;
}

QImage AIEngine::processTiled(const QImage &input, const ModelInfo &model)
{
    // 防御性检查：确保输入有效
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIEngine][Tiled] invalid input image, returning null";
        return QImage();
    }

    int tileSize = model.tileSize;
    int scale = model.scaleFactor;
    int w = input.width();
    int h = input.height();

    qInfo() << "[AIEngine][Tiled] entry"
            << "inputSize:" << w << "x" << h
            << "inputFormat:" << input.format()
            << "tileSize:" << tileSize
            << "scale:" << scale
            << "layerCount:" << model.layerCount;

    if (tileSize <= 0) {
        qWarning() << "[AIEngine][Tiled] tileSize is 0 or negative, falling back to processSingle";
        return processSingle(input, model);
    }

    // ── 动态计算 padding：复杂模型需要更大的重叠区域避免边界伪影 ──────────
    // Real-ESRGAN 等大模型的感受野很大，需要足够的上下文才能产生一致的输出
    int padding = model.tilePadding;
    const int layerCount = model.layerCount;  // 使用缓存的层数，避免直接访问 m_net
    if (layerCount > 500) {
        padding = std::max(padding, 64);  // 超大模型（如 realesrgan_x4plus 999层）
    } else if (layerCount > 200) {
        padding = std::max(padding, 48);  // 大模型（如 realesrgan_x4plus_anime 268层）
    } else if (layerCount > 50) {
        padding = std::max(padding, 24);  // 中等模型
    }
    
    // ── 为边界分块提供完整 padding：使用简单黑色填充 ──────────────────────
    // 注：复杂的镜像填充在某些 QImage 格式上可能导致崩溃，使用简单填充更稳定
    // 转换为 RGB888 格式确保兼容性
    QImage normalizedInput;
    if (input.format() == QImage::Format_RGB888) {
        normalizedInput = input;
    } else {
        normalizedInput = input.convertToFormat(QImage::Format_RGB888);
        qInfo() << "[AIEngine][Tiled] format converted"
                << "from:" << input.format()
                << "to:" << normalizedInput.format()
                << "bytesPerLine:" << normalizedInput.bytesPerLine()
                << "expectedBytesPerLine:" << (w * 3);
    }

    if (normalizedInput.isNull() || normalizedInput.format() != QImage::Format_RGB888) {
        qWarning() << "[AIEngine][Tiled] failed to normalize input format"
                   << "isNull:" << normalizedInput.isNull()
                   << "format:" << normalizedInput.format();
        return processSingle(input, model);
    }

    QImage paddedInput(w + 2 * padding, h + 2 * padding, QImage::Format_RGB888);
    if (paddedInput.isNull()) {
        qWarning() << "[AIEngine][Tiled] failed to create padded image";
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

    QImage output(w * scale, h * scale, QImage::Format_RGB888);
    output.fill(Qt::black);
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    int tileIndex = 0;
    int successfulTiles = 0;
    int consecutiveFailures = 0;  // 连续失败计数，用于快速失败检测
    const int kMaxConsecutiveFailures = 2;  // 连续失败超过此数即认为 GPU 状态损坏

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
                setProgress(0.1 + 0.85 * tileIndex / totalTiles);
                
                // 如果检测到 GPU OOM，立即返回让调用者用更小分块重试
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

            // 由于使用了镜像填充的 paddedInput，所有分块都有完整的 padding
            // 因此裁剪位置始终是 (padding * scale, padding * scale)
            int outPadLeft = padding * scale;
            int outPadTop  = padding * scale;
            int outW = actualTileW * scale;
            int outH = actualTileH * scale;

            if (outPadLeft + outW > tileResult.width() || outPadTop + outH > tileResult.height()) {
                qWarning() << "[AIEngine][Tiled] tile" << tileIndex
                           << "crop region out of bounds, tileResult:" << tileResult.size()
                           << "crop:(" << outPadLeft << "," << outPadTop << ") size:" << outW << "x" << outH;
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            QImage croppedResult = tileResult.copy(outPadLeft, outPadTop, outW, outH);
            painter.drawImage(x0 * scale, y0 * scale, croppedResult);
            tileIndex++;
            successfulTiles++;
            consecutiveFailures = 0;  // 成功时重置连续失败计数
            setProgress(0.1 + 0.85 * tileIndex / totalTiles);
        }
    }
    painter.end();

    // 只有所有分块都成功才返回结果
    if (successfulTiles < totalTiles) {
        int failedTiles = totalTiles - successfulTiles;
        qWarning() << "[AIEngine][Tiled] tiled inference incomplete"
                   << "successful:" << successfulTiles
                   << "failed:" << failedTiles
                   << "total:" << totalTiles;
        emitError(tr("推理未完整完成（%1/%2 分块成功）").arg(successfulTiles).arg(totalTiles));
        return QImage();
    }

    qInfo() << "[AIEngine][Tiled] all" << totalTiles << "tiles processed successfully";
    return output;
}

QImage AIEngine::processWithTTA(const QImage &input, const ModelInfo &model)
{
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

    const int totalSteps = transformed.size();
    QList<QImage> results;

    for (int i = 0; i < totalSteps; ++i) {
        if (m_cancelRequested) {
            qDebug() << "[AIEngine] TTA processing cancelled at step" << (i + 1) << "/" << totalSteps;
            return QImage();
        }
        emit progressTextChanged(tr("TTA 处理中: %1/%2").arg(i + 1).arg(totalSteps));
        setProgress(0.1 + 0.8 * static_cast<double>(i) / totalSteps);

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
    emit progressTextChanged(tr("合并 TTA 结果..."));
    return mergeTTAResults(results);
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

// ========== 轻量级帧推理（供视频管线使用）==========

QImage AIEngine::processFrame(const QImage &input, const ModelInfo &model, int tileSize)
{
    // 防御性检查：确保输入有效
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIEngine][processFrame] invalid input image"
                   << "isNull:" << input.isNull()
                   << "width:" << input.width()
                   << "height:" << input.height();
        return QImage();
    }
    
    // 验证 QImage 内存是否有效
    if (input.constBits() == nullptr) {
        qWarning() << "[AIEngine][processFrame] input has null bits pointer";
        return QImage();
    }
    
    // 创建深拷贝验证内存有效性并确保数据独立（避免隐式共享导致的竞争）
    QImage safeInput = input.copy();
    if (safeInput.isNull()) {
        qWarning() << "[AIEngine][processFrame] failed to create safe input copy";
        return QImage();
    }
    
    // 检查取消状态
    if (m_cancelRequested || m_forceCancelled) {
        return QImage();
    }

    // 自动 tileSize 决策
    if (tileSize <= 0) {
        tileSize = computeAutoTileSizeForModel(input.size(), model);
    }

    // 极大图像安全检查
    if (tileSize == 0 && model.tileSize > 0) {
        const qint64 pixels = static_cast<qint64>(input.width()) * input.height();
        if (pixels > 1024LL * 1024) {
            tileSize = model.tileSize;
        }
    }
    if (tileSize == 0) {
        const qint64 pixels = static_cast<qint64>(input.width()) * input.height();
        if (pixels > 2048LL * 2048 && model.tileSize > 0) {
            tileSize = model.tileSize;
        }
    }

    // GPU OOM 自动降级
    if (m_gpuOomDetected.load() && tileSize == 0) {
        tileSize = (model.tileSize > 0) ? model.tileSize : 200;
    }

    ModelInfo effectiveModel = model;
    if (tileSize > 0) {
        effectiveModel.tileSize = tileSize;
    }

    // 分块处理策略：
    // 1. 视频帧通常较小（<1024x1024），单帧处理更稳定高效
    // 2. 大图像（>1024x1024）需要分块处理避免 GPU OOM
    // 3. 分块处理的 padding/mirroring 在某些边界条件下可能导致内存问题
    //    因此仅在必要时使用，且优先选择较大的 tile size 减少拼接次数
    const qint64 pixelCount = static_cast<qint64>(safeInput.width()) * safeInput.height();
    constexpr qint64 kTileThreshold = 1024LL * 1024;  // 1M 像素阈值
    
    bool needTile = (tileSize > 0) && (pixelCount > kTileThreshold);

    QImage result;
    if (needTile) {
        // 大图像：使用分块处理，但优先使用较大 tile 减少边界问题
        int safeTileSize = std::max(tileSize, 256);
        effectiveModel.tileSize = safeTileSize;
        result = processTiledNoProgress(safeInput, effectiveModel);
    } else {
        // 小图像/视频帧：单帧处理更稳定
        result = processSingleNoProgress(safeInput, effectiveModel);
    }

    // GPU OOM 自动降级重试
    if (result.isNull() && m_gpuOomDetected.load()) {
        const int fallbackTiles[] = {128, 96, 64};
        for (int fb : fallbackTiles) {
            if (!result.isNull()) break;
            m_gpuOomDetected.store(false);
            cleanupGpuMemory();
            effectiveModel.tileSize = fb;
            result = processTiled(safeInput, effectiveModel);
        }
    }

    return result;
}

QImage AIEngine::processTiledNoProgress(const QImage &input, const ModelInfo &model)
{
    // 防御性检查：确保输入有效
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIEngine][TiledNoProgress] invalid input image, returning null";
        return QImage();
    }

    int tileSize = model.tileSize;
    int scale = model.scaleFactor;
    int w = input.width();
    int h = input.height();

    if (tileSize <= 0) {
        return processSingleNoProgress(input, model);
    }

    // ── 动态计算 padding：复杂模型需要更大的重叠区域避免边界伪影 ──────────
    int padding = model.tilePadding;
    const int layerCount = model.layerCount;
    if (layerCount > 500) {
        padding = std::max(padding, 64);
    } else if (layerCount > 200) {
        padding = std::max(padding, 48);
    } else if (layerCount > 50) {
        padding = std::max(padding, 24);
    }
    
    // ── 转换为 RGB888 格式确保兼容性 ────────────────────────────
    QImage normalizedInput;
    if (input.format() == QImage::Format_RGB888) {
        normalizedInput = input;
    } else {
        QImage inputCopy = input.copy();
        normalizedInput = inputCopy.convertToFormat(QImage::Format_RGB888);
    }

    if (normalizedInput.isNull() || normalizedInput.format() != QImage::Format_RGB888) {
        qWarning() << "[AIEngine][TiledNoProgress] failed to normalize input format"
                   << "isNull:" << normalizedInput.isNull()
                   << "format:" << normalizedInput.format();
        return processSingleNoProgress(input, model);
    }

    QImage paddedInput(w + 2 * padding, h + 2 * padding, QImage::Format_RGB888);
    if (paddedInput.isNull()) {
        qWarning() << "[AIEngine][TiledNoProgress] failed to create padded image";
        return processSingleNoProgress(input, model);
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
    
    // 使用扩展后的图像进行分块处理
    const int paddedW = paddedInput.width();
    const int paddedH = paddedInput.height();

    int tilesX = (w + tileSize - 1) / tileSize;
    int tilesY = (h + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;

    QImage output(w * scale, h * scale, QImage::Format_RGB888);
    output.fill(Qt::black);
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    int tileIndex = 0;
    int successfulTiles = 0;
    int consecutiveFailures = 0;  // 连续失败计数，用于快速失败检测
    const int kMaxConsecutiveFailures = 2;  // 连续失败超过此数即认为 GPU 状态损坏

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            if (m_cancelRequested) { painter.end(); return QImage(); }

            // 快速失败：如果连续多个分块失败（可能是 GPU OOM 或状态损坏），立即返回
            if (consecutiveFailures >= kMaxConsecutiveFailures) {
                qWarning() << "[AIEngine][TiledNoProgress] fast-fail: too many consecutive failures"
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
                qWarning() << "[AIEngine][TiledNoProgress] tile" << tileIndex << "qimageToMat failed";
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            ncnn::Mat out = runInference(in, model);
            if (out.empty() || out.w <= 0 || out.h <= 0) {
                qWarning() << "[AIEngine][TiledNoProgress] tile" << tileIndex << "inference failed";
                tileIndex++;
                consecutiveFailures++;
                
                // 如果检测到 GPU OOM，立即返回让调用者用更小分块重试
                if (m_gpuOomDetected.load()) {
                    qWarning() << "[AIEngine][TiledNoProgress] GPU OOM detected, aborting to allow retry with smaller tiles";
                    painter.end();
                    return QImage();
                }
                continue;
            }

            QImage tileResult = matToQimage(out, model);
            if (tileResult.isNull()) {
                qWarning() << "[AIEngine][TiledNoProgress] tile" << tileIndex << "matToQimage failed";
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            // 由于使用了镜像填充的 paddedInput，所有分块都有完整的 padding
            // 因此裁剪位置始终是 (padding * scale, padding * scale)
            int outPadLeft = padding * scale;
            int outPadTop  = padding * scale;
            int outW = actualTileW * scale;
            int outH = actualTileH * scale;

            if (outPadLeft + outW > tileResult.width() || outPadTop + outH > tileResult.height()) {
                qWarning() << "[AIEngine][TiledNoProgress] tile" << tileIndex
                           << "crop region out of bounds, tileResult:" << tileResult.size()
                           << "crop:(" << outPadLeft << "," << outPadTop << ") size:" << outW << "x" << outH;
                tileIndex++;
                consecutiveFailures++;
                continue;
            }

            QImage croppedResult = tileResult.copy(outPadLeft, outPadTop, outW, outH);
            painter.drawImage(x0 * scale, y0 * scale, croppedResult);
            tileIndex++;
            successfulTiles++;
            consecutiveFailures = 0;  // 成功时重置连续失败计数
        }
    }
    painter.end();

    // 只有所有分块都成功才返回结果
    if (successfulTiles < totalTiles) {
        int failedTiles = totalTiles - successfulTiles;
        qWarning() << "[AIEngine][TiledNoProgress] tiled inference incomplete"
                   << "successful:" << successfulTiles
                   << "failed:" << failedTiles
                   << "total:" << totalTiles;
        emitError(tr("推理未完整完成（%1/%2 分块成功）").arg(successfulTiles).arg(totalTiles));
        return QImage();
    }

    return output;
}

QImage AIEngine::processSingleNoProgress(const QImage &input, const ModelInfo &model)
{
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIEngine][SingleNoProgress] invalid input image";
        return QImage();
    }

    if (m_cancelRequested) return QImage();
    
    ncnn::Mat in = qimageToMat(input, model);
    if (m_cancelRequested) return QImage();
    ncnn::Mat out = runInference(in, model);
    if (out.empty()) {
        qWarning() << "[AIEngine][SingleNoProgress] inference returned empty mat";
        return QImage();
    }
    if (m_cancelRequested) return QImage();
    return matToQimage(out, model);
}

void AIEngine::cleanupGpuMemory()
{
#if NCNN_VULKAN
    if (!m_vkdev) return;
    if (m_blobVkAllocator) {
        m_blobVkAllocator->clear();
    }
    if (m_stagingVkAllocator) {
        m_stagingVkAllocator->clear();
    }
#endif
}

// ========== 视频处理管线 ==========

void AIEngine::processVideoInternal(const QString &inputPath, const QString &outputPath)
{
    QElapsedTimer perfTimer;
    perfTimer.start();

    // ── 锁定推理互斥锁：整个视频处理期间持有，避免逐帧锁释放导致状态不一致 ──
    QMutexLocker inferenceLocker(&m_inferenceMutex);

    // ── 快照模型和参数（线程安全，仅在循环前做一次）─────────────────────────
    ModelInfo snapModel;
    QString snapModelId;
    QVariantMap snapParams;
    {
        QMutexLocker locker(&m_mutex);
        snapModel   = m_currentModel;
        snapModelId = m_currentModelId;
    }
    {
        QMutexLocker locker(&m_paramsMutex);
        snapParams = getEffectiveParamsLocked(snapModel);
    }

    // ── 发出警告：图像模型处理视频的潜在问题 ──────────────────────────────
    const bool isVideoNativeModel = snapModelId.contains("video", Qt::CaseInsensitive)
                                 || snapModelId.contains("rife",  Qt::CaseInsensitive);
    if (!isVideoNativeModel) {
        QString warning = tr("当前使用图像模型处理视频，可能存在以下问题：\n"
                             "• 帧间一致性较差，可能出现闪烁\n"
                             "• 处理速度显著慢于视频专用模型\n"
                             "• 输出文件体积可能较大\n"
                             "建议优先使用 AnimeVideo 系列视频模型。");
        qWarning() << "[AIEngine][Video] image model used for video:" << snapModelId << warning;
        QMetaObject::invokeMethod(this, [this, warning]() {
            emit videoProcessingWarning(warning);
        }, Qt::QueuedConnection);
    }

    setProcessing(true);
    setProgress(0.0);
    m_cancelRequested = false;

    // ── 预计算帧 tileSize 和 outscale ────────────────────────────────────
    // 优先使用用户指定的 tileSize，否则使用模型默认值（避免自动计算返回过大值导致 GPU OOM）
    int frameTileSize = snapModel.tileSize;  // 使用模型默认值（通常是 200）
    if (snapParams.contains("tileSize") && snapParams["tileSize"].toInt() > 0) {
        frameTileSize = snapParams["tileSize"].toInt();
    }
    double outscale = snapParams.value("outscale", snapModel.scaleFactor).toDouble();

    // ── FFmpeg 资源（全部集中声明，统一清理）──────────────────────────────
    AVFormatContext *inFmtCtx  = nullptr;
    AVFormatContext *outFmtCtx = nullptr;
    AVCodecContext  *decCtx    = nullptr;
    AVCodecContext  *encCtx    = nullptr;
    SwsContext      *decSwsCtx = nullptr;   // 解码 sws（复用，避免逐帧创建销毁）
    SwsContext      *encSwsCtx = nullptr;   // 编码 sws
    AVFrame *decFrame = nullptr;
    AVFrame *encFrame = nullptr;
    AVPacket *pkt     = nullptr;

    int videoIdx = -1;
    int audioIdx = -1;
    int outVideoIdx = -1;
    int outAudioIdx = -1;
    bool headerWritten = false;
    bool encoderOpened = false;

    // 解码 sws 缓存状态（用于检测格式变化并按需重建）
    int decSwsW = 0, decSwsH = 0;
    AVPixelFormat decSwsFmt = AV_PIX_FMT_NONE;

    auto cleanup = [&]() {
        if (decSwsCtx) sws_freeContext(decSwsCtx);
        if (encSwsCtx) sws_freeContext(encSwsCtx);
        if (encFrame)  av_frame_free(&encFrame);
        if (decFrame)  av_frame_free(&decFrame);
        if (pkt)       av_packet_free(&pkt);
        if (encCtx)    avcodec_free_context(&encCtx);
        if (decCtx)    avcodec_free_context(&decCtx);
        if (outFmtCtx) {
            if (headerWritten && !(outFmtCtx->oformat->flags & AVFMT_NOFILE))
                avio_closep(&outFmtCtx->pb);
            avformat_free_context(outFmtCtx);
        }
        if (inFmtCtx) avformat_close_input(&inFmtCtx);
        setProcessing(false);
    };

    auto fail = [&](const QString &err) {
        qWarning() << "[AIEngine][Video] error:" << err;
        cleanup();
        emit processError(err);
        emit processFileCompleted(false, QString(), err);
    };

    // ── 1. 打开输入文件 ──────────────────────────────────────────────────
    if (avformat_open_input(&inFmtCtx, inputPath.toUtf8().constData(), nullptr, nullptr) < 0) {
        fail(tr("无法打开视频文件: %1").arg(inputPath));
        return;
    }
    if (avformat_find_stream_info(inFmtCtx, nullptr) < 0) {
        fail(tr("无法获取视频流信息"));
        return;
    }

    for (unsigned i = 0; i < inFmtCtx->nb_streams; ++i) {
        if (inFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoIdx < 0)
            videoIdx = static_cast<int>(i);
        else if (inFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioIdx < 0)
            audioIdx = static_cast<int>(i);
    }
    if (videoIdx < 0) { fail(tr("未找到视频流")); return; }

    // ── 2. 初始化解码器 ──────────────────────────────────────────────────
    const AVCodec *dec = avcodec_find_decoder(inFmtCtx->streams[videoIdx]->codecpar->codec_id);
    if (!dec) { fail(tr("未找到合适的视频解码器")); return; }

    decCtx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(decCtx, inFmtCtx->streams[videoIdx]->codecpar);
    if (avcodec_open2(decCtx, dec, nullptr) < 0) {
        fail(tr("无法打开视频解码器"));
        return;
    }

    // 估算总帧数
    int64_t totalFrames = inFmtCtx->streams[videoIdx]->nb_frames;
    if (totalFrames <= 0 && decCtx->framerate.num > 0 && inFmtCtx->duration > 0) {
        totalFrames = inFmtCtx->duration * decCtx->framerate.num
                    / (static_cast<int64_t>(AV_TIME_BASE) * std::max(1, decCtx->framerate.den));
    }
    if (totalFrames <= 0) totalFrames = 1000;

    qInfo() << "[AIEngine][Video] start processing"
            << "input:" << inputPath
            << "decoder:" << dec->name
            << "size:" << decCtx->width << "x" << decCtx->height
            << "estimatedFrames:" << totalFrames
            << "model:" << snapModelId
            << "tileSize:" << frameTileSize
            << "outscale:" << outscale;

    // Note: setProgress removed to avoid cross-thread signal issues in worker thread

    // ── 3. 逐帧解码 → AI 推理 → 编码 ───────────────────────────────────
    decFrame = av_frame_alloc();
    pkt      = av_packet_alloc();
    int64_t frameCount = 0;
    int64_t failedFrames = 0;
    bool firstFrameProcessed = false;

    // 确保输出路径使用 mp4 扩展名
    QString effectiveOutputPath = outputPath;
    {
        QFileInfo outInfo(outputPath);
        if (outInfo.suffix().toLower() != "mp4") {
            effectiveOutputPath = outInfo.absolutePath() + "/" + outInfo.completeBaseName() + ".mp4";
        }
    }

    // GPU 清理间隔：每 N 帧清理一次显存，平衡性能和显存占用
    constexpr int kGpuCleanupInterval = 5;

    // ── 方案3：try-catch 包裹主循环，捕获未预期的异常防止进程残留 ────
    try {
    while (av_read_frame(inFmtCtx, pkt) >= 0) {
        if (m_cancelRequested || m_forceCancelled) {
            av_packet_unref(pkt);
            qInfo() << "[AIEngine][Video] Cancelled at frame" << frameCount << "force:" << m_forceCancelled.load();
            fail(tr("视频处理已取消"));
            return;
        }

        // ── 音频流：直接复制（仅在编码器已打开后） ─────────────────────────
        if (pkt->stream_index == audioIdx && outFmtCtx && outAudioIdx >= 0 && headerWritten) {
            AVPacket *audioPkt = av_packet_clone(pkt);
            audioPkt->stream_index = outAudioIdx;
            av_packet_rescale_ts(audioPkt,
                inFmtCtx->streams[audioIdx]->time_base,
                outFmtCtx->streams[outAudioIdx]->time_base);
            av_interleaved_write_frame(outFmtCtx, audioPkt);
            av_packet_free(&audioPkt);
            av_packet_unref(pkt);
            continue;
        }

        if (pkt->stream_index != videoIdx) { av_packet_unref(pkt); continue; }

        avcodec_send_packet(decCtx, pkt);
        av_packet_unref(pkt);

        while (avcodec_receive_frame(decCtx, decFrame) == 0) {
            if (m_cancelRequested || m_forceCancelled) {
                qInfo() << "[AIEngine][Video] Cancelled at frame receive" << frameCount;
                fail(tr("视频处理已取消"));
                return;
            }

            // ── 解码帧 → QImage（使用 frame->format 而非 decCtx->pix_fmt，
            //    复用 SwsContext，仅在分辨率或格式变化时重建）───────────────
            const AVPixelFormat frameFmt = static_cast<AVPixelFormat>(decFrame->format);
            if (!decSwsCtx || decSwsW != decFrame->width ||
                decSwsH != decFrame->height || decSwsFmt != frameFmt) {
                if (decSwsCtx) sws_freeContext(decSwsCtx);
                decSwsCtx = sws_getContext(
                    decFrame->width, decFrame->height, frameFmt,
                    decFrame->width, decFrame->height, AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                decSwsW   = decFrame->width;
                decSwsH   = decFrame->height;
                decSwsFmt = frameFmt;
                if (!decSwsCtx) {
                    qWarning() << "[AIEngine][Video] frame" << frameCount
                               << "sws_getContext failed for pixel format" << frameFmt;
                    failedFrames++;
                    continue;
                }
            }

            // 修复堆损坏：使用对齐的临时缓冲区进行 sws_scale，避免 QImage bytesPerLine 问题
            const int frameW = decFrame->width;
            const int frameH = decFrame->height;
            const int rgb24LineSize = frameW * 3;  // RGB24 精确行宽，无填充
            
            // 分配临时缓冲区（sws_scale 需要连续内存，无填充）
            std::vector<uint8_t> rgb24Buffer(rgb24LineSize * frameH);
            
            {
                uint8_t *dst[4] = { rgb24Buffer.data(), nullptr, nullptr, nullptr };
                int dstStride[4] = { rgb24LineSize, 0, 0, 0 };

                if (frameCount % 10 == 0 || frameCount < 3) {
                    qInfo() << "[AIEngine][Video] frame" << frameCount
                            << "sws_scale: src linesize:" << decFrame->linesize[0]
                            << "dst lineSize:" << rgb24LineSize
                            << "width:" << frameW
                            << "height:" << frameH;
                    fflush(stdout);
                }

                int swsRet = sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
                                       0, frameH, dst, dstStride);

                if (swsRet < 0 || swsRet != frameH) {
                    qWarning() << "[AIEngine][Video] frame" << frameCount
                               << "sws_scale failed with ret:" << swsRet
                               << "expected:" << frameH;
                    failedFrames++;
                    continue;
                }
            }
            
            // 从临时缓冲区安全创建 QImage
            QImage frameImg(frameW, frameH, QImage::Format_RGB888);
            if (frameImg.isNull()) {
                qWarning() << "[AIEngine][Video] frame" << frameCount
                           << "failed to allocate QImage for frame";
                failedFrames++;
                continue;
            }
            
            // 逐行复制数据，正确处理 QImage 的 bytesPerLine 对齐
            for (int y = 0; y < frameH; ++y) {
                const uint8_t* srcLine = rgb24Buffer.data() + y * rgb24LineSize;
                uchar* dstLine = frameImg.scanLine(y);
                std::memcpy(dstLine, srcLine, rgb24LineSize);
            }
            

            if (frameImg.isNull()) {
                qWarning() << "[AIEngine][Video] frame" << frameCount
                           << "decode to QImage failed, skipping";
                failedFrames++;
                continue;
            }

            // ── GPU 显存帧间清理已禁用 ──────────────────────────────────────
            // 在视频处理过程中清理 GPU 内存会导致 ncnn 的 Vulkan allocator 失效，
            // 从而引发空指针访问崩溃。仅在视频处理完全结束后清理一次。
            // if (frameCount > 0 && (frameCount % kGpuCleanupInterval == 0)) {
            //     cleanupGpuMemory();
            // }

            
            // ── AI 推理（使用轻量级 processFrame，不涉及状态/信号管理）─────
            QImage resultImg = processFrame(frameImg, snapModel, frameTileSize);
            if (resultImg.isNull()) {
                qWarning() << "[AIEngine][Video] frame" << frameCount
                           << "AI inference failed, using original frame";
                resultImg = frameImg;
                failedFrames++;
            }

            // 应用 outscale（如果与模型原生缩放不同）
            if (std::abs(outscale - snapModel.scaleFactor) > 0.01) {
                resultImg = applyOutscale(resultImg, outscale / snapModel.scaleFactor);
            }

            // ── 延迟初始化编码器（需要第一帧推理结果才知道输出尺寸） ──────
            if (!firstFrameProcessed) {
                firstFrameProcessed = true;
                // 方案4：确保输出宽高为偶数（YUV420P 要求，某些编码器对奇数分辨率不兼容）
                const int outW = resultImg.width()  & ~1;
                const int outH = resultImg.height() & ~1;
                if (outW != resultImg.width() || outH != resultImg.height()) {
                    qInfo() << "[AIEngine][Video] output size aligned to even:"
                            << resultImg.width() << "x" << resultImg.height()
                            << "->" << outW << "x" << outH;
                    resultImg = resultImg.scaled(outW, outH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }

                avformat_alloc_output_context2(&outFmtCtx, nullptr, "mp4",
                    effectiveOutputPath.toUtf8().constData());
                if (!outFmtCtx) { fail(tr("无法创建输出格式上下文")); return; }

                // 编码器优先级：优先使用稳定的软件/硬件编码器
                // 注意：h264_mf 在工作线程中不稳定，需要排除
                const AVCodec *enc = nullptr;
                const char* encoderPriority[] = {
                    "libx264",      // GPL 软件编码器（需要 GPL FFmpeg 构建）
                    "h264_nvenc",   // NVIDIA 硬件编码器（RTX/GTX 显卡）
                    "h264_qsv",     // Intel Quick Sync（集成显卡）
                    "h264_amf",     // AMD 硬件编码器
                    "libopenh264",  // Cisco OpenH264（BSD 许可证）
                    nullptr
                };
                
                for (int i = 0; encoderPriority[i] && !enc; ++i) {
                    const AVCodec *candidate = avcodec_find_encoder_by_name(encoderPriority[i]);
                    if (candidate) {
                        // 排除 h264_mf（Windows MF 编码器在工作线程中崩溃）
                        if (QString(candidate->name).contains("mf", Qt::CaseInsensitive)) {
                            qInfo() << "[AIEngine][Video] skipping unstable encoder:" << candidate->name;
                            continue;
                        }
                        enc = candidate;
                        qInfo() << "[AIEngine][Video] selected H.264 encoder:" << enc->name;
                    }
                }
                
                // 回退到 MPEG-4（广泛支持，质量稍低但稳定）
                if (!enc) {
                    enc = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
                    if (enc) {
                        qInfo() << "[AIEngine][Video] using MPEG-4 fallback encoder:" << enc->name;
                    }
                }
                
                if (!enc) { fail(tr("未找到可用的视频编码器")); return; }

                AVStream *outVStream = avformat_new_stream(outFmtCtx, nullptr);
                outVideoIdx = outVStream->index;

                encCtx = avcodec_alloc_context3(enc);
                encCtx->width  = outW;
                encCtx->height = outH;
                encCtx->pix_fmt = AV_PIX_FMT_YUV420P;
                encCtx->time_base = inFmtCtx->streams[videoIdx]->time_base;
                encCtx->framerate = decCtx->framerate;
                encCtx->bit_rate = static_cast<int64_t>(outW) * outH * 4;
                if (outFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
                    encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

                AVDictionary *encOpts = nullptr;
                av_dict_set(&encOpts, "preset", "medium", 0);
                av_dict_set(&encOpts, "crf", "18", 0);
                if (avcodec_open2(encCtx, enc, &encOpts) < 0) {
                    av_dict_free(&encOpts);
                    fail(tr("无法打开视频编码器"));
                    return;
                }
                av_dict_free(&encOpts);
                encoderOpened = true;

                avcodec_parameters_from_context(outVStream->codecpar, encCtx);
                outVStream->time_base = encCtx->time_base;

                if (audioIdx >= 0) {
                    AVStream *outAStream = avformat_new_stream(outFmtCtx, nullptr);
                    outAudioIdx = outAStream->index;
                    avcodec_parameters_copy(outAStream->codecpar, inFmtCtx->streams[audioIdx]->codecpar);
                    outAStream->time_base = inFmtCtx->streams[audioIdx]->time_base;
                }

                if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
                    if (avio_open(&outFmtCtx->pb, effectiveOutputPath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
                        fail(tr("无法打开输出文件: %1").arg(effectiveOutputPath));
                        return;
                    }
                }
                if (avformat_write_header(outFmtCtx, nullptr) < 0) {
                    fail(tr("无法写入视频文件头"));
                    return;
                }
                headerWritten = true;

                encSwsCtx = sws_getContext(outW, outH, AV_PIX_FMT_RGB24,
                    outW, outH, AV_PIX_FMT_YUV420P,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                encFrame = av_frame_alloc();
                encFrame->format = AV_PIX_FMT_YUV420P;
                encFrame->width  = outW;
                encFrame->height = outH;
                av_frame_get_buffer(encFrame, 32);

                qInfo() << "[AIEngine][Video] encoder initialized"
                        << "outputSize:" << outW << "x" << outH
                        << "codec:" << enc->name;
            }

            // ── QImage → YUV → 编码（内联转换，避免中间 AVFrame 分配）────
            {
                // 确保 resultImg 是 RGB888 格式（与 encSwsCtx 匹配）
                if (resultImg.format() != QImage::Format_RGB888) {
                    resultImg = resultImg.convertToFormat(QImage::Format_RGB888);
                }
                uint8_t *src[4] = { resultImg.bits(), nullptr, nullptr, nullptr };
                int srcStride[4] = { static_cast<int>(resultImg.bytesPerLine()), 0, 0, 0 };
                sws_scale(encSwsCtx, src, srcStride, 0, resultImg.height(),
                          encFrame->data, encFrame->linesize);
            }
            encFrame->pts = decFrame->pts;

            int sendRet = avcodec_send_frame(encCtx, encFrame);
            if (sendRet < 0) {
                qWarning() << "[AIEngine][Video] frame" << frameCount
                           << "avcodec_send_frame failed ret:" << sendRet;
                failedFrames++;
            }
            AVPacket *outPkt = av_packet_alloc();
            while (avcodec_receive_packet(encCtx, outPkt) == 0) {
                av_packet_rescale_ts(outPkt, encCtx->time_base,
                    outFmtCtx->streams[outVideoIdx]->time_base);
                outPkt->stream_index = outVideoIdx;
                av_interleaved_write_frame(outFmtCtx, outPkt);
                av_packet_unref(outPkt);
            }
            av_packet_free(&outPkt);

            frameCount++;
            
            // 更新进度（跨线程安全）
            if (totalFrames > 0) {
                const double progress = static_cast<double>(frameCount) / totalFrames;
                setProgress(progress);
            }
            
            // 心跳日志（每30帧输出一次，减少IO开销）
            if (frameCount % 30 == 0) {
                qInfo() << "[AIEngine][Video] progress" << frameCount << "/" << totalFrames
                        << QString::number(100.0 * frameCount / std::max(1LL, totalFrames), 'f', 1) + "%"
                        << "elapsed:" << perfTimer.elapsed() << "ms";
            }
        }
    }

    // ── 4. 刷新编码器 ───────────────────────────────────────────────────
    if (encoderOpened && encCtx) {
        avcodec_send_frame(encCtx, nullptr);
        AVPacket *outPkt = av_packet_alloc();
        while (avcodec_receive_packet(encCtx, outPkt) == 0) {
            av_packet_rescale_ts(outPkt, encCtx->time_base,
                outFmtCtx->streams[outVideoIdx]->time_base);
            outPkt->stream_index = outVideoIdx;
            av_interleaved_write_frame(outFmtCtx, outPkt);
            av_packet_unref(outPkt);
        }
        av_packet_free(&outPkt);
    }

    // ── 5. 写入文件尾 & 最终清理 ────────────────────────────────────────
    if (headerWritten && outFmtCtx) {
        av_write_trailer(outFmtCtx);
    }

    } catch (const std::exception &ex) {
        qCritical() << "[AIEngine][Video] C++ exception caught in video pipeline:"
                    << ex.what() << "at frame" << frameCount;
        fflush(stdout);
        cleanup();
        emit processError(tr("视频处理异常崩溃: %1").arg(QString::fromLocal8Bit(ex.what())));
        emit processFileCompleted(false, QString(), tr("视频处理异常崩溃"));
        return;
    } catch (...) {
        qCritical() << "[AIEngine][Video] Unknown exception caught in video pipeline"
                    << "at frame" << frameCount;
        fflush(stdout);
        cleanup();
        emit processError(tr("视频处理发生未知异常"));
        emit processFileCompleted(false, QString(), tr("视频处理发生未知异常"));
        return;
    }

    // GPU 显存清理已禁用
    // 在视频处理结束后立即清理 Vulkan allocator 会导致 Qt 渲染线程的 GPU 设备丢失
    // (DXGI_ERROR_DEVICE_REMOVED 0x887a0005)。NCNN 的 Vulkan allocator 会在
    // AIEngine 析构时自动清理，无需手动干预。
    // cleanupGpuMemory();

    const qint64 totalCostMs = perfTimer.elapsed();
    qInfo() << "[AIEngine][Video] finished"
            << "frames:" << frameCount
            << "failedFrames:" << failedFrames
            << "totalCost:" << totalCostMs << "ms"
            << "avgPerFrame:" << (frameCount > 0 ? totalCostMs / frameCount : 0) << "ms"
            << "output:" << effectiveOutputPath;

    setProgress(1.0);  // 完成时设置进度为100%
    cleanup();

    if (frameCount > 0) {
        QFileInfo outInfo(effectiveOutputPath);
        if (outInfo.exists() && outInfo.size() > 0) {
            emit processFileCompleted(true, effectiveOutputPath, QString());
        } else {
            emit processFileCompleted(false, QString(),
                tr("视频处理完成但输出文件无效: %1").arg(effectiveOutputPath));
        }
    } else {
        emit processFileCompleted(false, QString(), tr("视频处理失败：未能成功处理任何帧"));
    }
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