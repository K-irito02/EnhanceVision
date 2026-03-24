/**
 * @file AIEngine.cpp
 * @brief NCNN AI 推理引擎实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QtConcurrent/QtConcurrent>
#include <QDateTime>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

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
        qWarning() << "[AIEngine] Please ensure Vulkan drivers are installed";
        qWarning() << "[AIEngine] Download from: https://vulkan.lunarg.com/sdk/home";
        emit gpuAvailableChanged(false);
    }
#else
    m_gpuAvailable = false;
    qWarning() << "[AIEngine] NCNN built without Vulkan support, using CPU mode";
    qWarning() << "[AIEngine] To enable GPU acceleration:";
    qWarning() << "[AIEngine] 1. Install Vulkan SDK from https://vulkan.lunarg.com/sdk/home";
    qWarning() << "[AIEngine] 2. Rebuild NCNN with NCNN_VULKAN=ON";
    emit gpuAvailableChanged(false);
#endif

    updateOptions();
}

void AIEngine::destroyVulkan()
{
#if NCNN_VULKAN
    // m_vkdev is managed by ncnn globally, do not delete
    m_vkdev = nullptr;
    ncnn::destroy_gpu_instance();
#endif
}

void AIEngine::updateOptions()
{
    m_opt.num_threads = std::max(1, QThread::idealThreadCount() - 1);
    m_opt.use_packing_layout = true;

#if NCNN_VULKAN
    if (m_gpuAvailable && m_useGpu && m_vkdev) {
        m_opt.use_vulkan_compute = true;
        m_opt.blob_vkallocator = m_vkdev->acquire_blob_allocator();
        m_opt.workspace_vkallocator = m_vkdev->acquire_staging_allocator();
    } else {
        m_opt.use_vulkan_compute = false;
        m_opt.blob_vkallocator = nullptr;
        m_opt.workspace_vkallocator = nullptr;
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

    // 卸载旧模型
    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_currentModel.isLoaded = false;
    }

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

    qInfo() << "[AIEngine][loadModel] model loaded successfully"
            << "layers:" << m_net.layers().size();

    m_currentModelId = modelId;
    m_currentModel = info;
    m_currentModel.isLoaded = true;

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
        emit modelUnloaded();
        emit modelChanged();
    }
}

// ========== 推理接口 ==========

QImage AIEngine::process(const QImage &input)
{
    if (m_currentModelId.isEmpty() || !m_currentModel.isLoaded) {
        emit processError(tr("未加载模型"));
        return QImage();
    }

    if (input.isNull()) {
        emit processError(tr("输入图像为空"));
        return QImage();
    }

    setProcessing(true);
    setProgress(0.0);
    m_cancelRequested = false;

    QImage result;

    QVariantMap effectiveParams = getEffectiveParams();

    // ── 分块大小决策 ──────────────────────────────────────────
    // 优先级：用户手动设置 > 自动计算 > 模型默认
    int tileSize = 0;
    if (effectiveParams.contains("tileSize") && effectiveParams["tileSize"].toInt() > 0) {
        // 用户手动指定了分块大小
        tileSize = effectiveParams["tileSize"].toInt();
    } else {
        // 自动模式：根据图像尺寸和模型配置自动决定
        tileSize = computeAutoTileSize(input.size());
        // 将自动计算结果通知 UI（非阻塞）
        emit autoParamsComputed(tileSize);
        // 发出全参数自动推荐结果（isVideo 在此上下文为 false，图像推理）
        QVariantMap allAuto = computeAutoParams(input.size(), false);
        emit allAutoParamsComputed(allAuto);
    }

    double outscale = effectiveParams.value("outscale", m_currentModel.scaleFactor).toDouble();
    bool ttaMode = effectiveParams.value("tta_mode", false).toBool();

    qInfo() << "[AIEngine] process start"
            << "model:" << m_currentModelId
            << "input:" << input.size()
            << "gpuEnabled:" << (m_gpuAvailable && m_useGpu)
            << "tileSize:" << tileSize
            << "modelScale:" << m_currentModel.scaleFactor
            << "outscale:" << outscale
            << "tta:" << ttaMode;

    bool needTile = (tileSize > 0) &&
                    (input.width() > tileSize || input.height() > tileSize);

    if (ttaMode && !needTile) {
        result = processWithTTA(input, m_currentModel);
    } else if (needTile) {
        result = processTiled(input, m_currentModel);
    } else {
        result = processSingle(input, m_currentModel);
    }

    if (!result.isNull() && std::abs(outscale - m_currentModel.scaleFactor) > 0.01) {
        result = applyOutscale(result, outscale / m_currentModel.scaleFactor);
    }

    setProgress(1.0);
    setProcessing(false);

    if (!result.isNull()) {
        emit processCompleted(result);
    } else {
        qWarning() << "[AIEngine] Processing failed";
    }

    return result;
}

void AIEngine::processAsync(const QString &inputPath, const QString &outputPath)
{
    if (m_isProcessing) {
        emit processError(tr("已有推理任务正在进行"));
        emit processFileCompleted(false, QString(), tr("已有推理任务正在进行"));
        return;
    }

    QtConcurrent::run([this, inputPath, outputPath]() {
        QElapsedTimer perfTimer;
        perfTimer.start();

        QImage inputImage(inputPath);
        if (inputImage.isNull()) {
            QString error = tr("无法读取图像: %1").arg(inputPath);
            emit processError(error);
            emit processFileCompleted(false, QString(), error);
            return;
        }

        const qint64 loadInputCostMs = perfTimer.elapsed();

        m_lastError.clear();
        QMetaObject::Connection errorConn = connect(this, &AIEngine::processError,
            [this](const QString& err) { m_lastError = err; });

        QImage result = process(inputImage);

        const qint64 processCostMs = perfTimer.elapsed() - loadInputCostMs;

        disconnect(errorConn);

        if (m_cancelRequested) {
            emit processFileCompleted(false, QString(), tr("推理已取消"));
            return;
        }

        if (result.isNull()) {
            QString error = m_lastError.isEmpty() ? tr("推理失败") : m_lastError;
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

    // 简单的基于周围像素平均的修复算法
    // 多次迭代，逐步从边缘向内修复
    int maxIter = radius * 3;
    QImage working = result.copy();

    for (int iter = 0; iter < maxIter; ++iter) {
        bool anyChanged = false;

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // 检查是否是需要修复的像素
                if (*(msk.constScanLine(y) + x) == 0) continue;

                int sumR = 0, sumG = 0, sumB = 0;
                int count = 0;

                // 采样周围像素（仅使用非掩码区域或已修复的像素）
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;

                        // 使用非掩码区域的像素
                        if (*(msk.constScanLine(ny) + nx) == 0) {
                            QRgb pixel = working.pixel(nx, ny);
                            sumR += qRed(pixel);
                            sumG += qGreen(pixel);
                            sumB += qBlue(pixel);
                            count++;
                        }
                    }
                }

                if (count > 0) {
                    result.setPixel(x, y, qRgb(sumR / count, sumG / count, sumB / count));
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
    if (m_isProcessing) {
        emit processError(tr("已有任务正在进行"));
        return;
    }

    QtConcurrent::run([this, inputPath, maskPath, outputPath, radius, method]() {
        setProcessing(true);
        setProgress(0.0);

        QImage input(inputPath);
        QImage mask(maskPath);

        if (input.isNull() || mask.isNull()) {
            setProcessing(false);
            emit processFileCompleted(false, QString(), tr("无法读取图像"));
            return;
        }

        setProgress(0.2);
        QImage result = inpaint(input, mask, radius, method);
        setProgress(0.8);

        if (result.isNull()) {
            setProcessing(false);
            emit processFileCompleted(false, QString(), tr("修复失败"));
            return;
        }

        if (!result.save(outputPath)) {
            setProcessing(false);
            emit processFileCompleted(false, QString(), tr("保存失败"));
            return;
        }

        setProgress(1.0);
        setProcessing(false);
        emit processFileCompleted(true, outputPath, QString());
    });
}

// ========== 参数设置 ==========

void AIEngine::setParameter(const QString &name, const QVariant &value)
{
    m_parameters[name] = value;
}

QVariant AIEngine::getParameter(const QString &name) const
{
    return m_parameters.value(name);
}

void AIEngine::clearParameters()
{
    m_parameters.clear();
}

QVariantMap AIEngine::mergeWithDefaultParams() const
{
    QVariantMap merged = m_parameters;

    if (!m_currentModelId.isEmpty() && !m_currentModel.supportedParams.isEmpty()) {
        for (auto it = m_currentModel.supportedParams.begin(); it != m_currentModel.supportedParams.end(); ++it) {
            const QString &paramKey = it.key();
            const QVariantMap &paramMeta = it.value().toMap();

            if (!merged.contains(paramKey)) {
                if (paramMeta.contains("default")) {
                    merged[paramKey] = paramMeta["default"];
                }
            } else {
                QVariant &val = merged[paramKey];
                QString type = paramMeta["type"].toString();

                if (type == "int" || type == "float") {
                    double minVal = paramMeta["min"].toDouble();
                    double maxVal = paramMeta["max"].toDouble();
                    double currentVal = val.toDouble();
                    val = std::clamp(currentVal, minVal, maxVal);
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
    return mergeWithDefaultParams();
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

        // 如果有模型已加载，需要重新加载
        if (!m_currentModelId.isEmpty() && m_currentModel.isLoaded) {
            QString modelId = m_currentModelId;
            unloadModel();
            loadModel(modelId);
        }

        emit useGpuChanged(m_useGpu);
    }
}

// ========== 内部方法 ==========

void AIEngine::setProgress(double value, bool forceEmit)
{
    constexpr double kProgressEmitDelta = 0.01;
    constexpr qint64 kProgressEmitIntervalMs = 66;

    const double clampedValue = std::clamp(value, 0.0, 1.0);
    const double previous = m_progress.exchange(clampedValue);

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastEmit = m_lastProgressEmitMs.load();

    const bool firstEmit = (lastEmit == 0);
    const bool reachedTerminal = (clampedValue >= 1.0);
    const bool progressedEnough = (std::abs(clampedValue - previous) >= kProgressEmitDelta);
    const bool timeoutReached = (nowMs - lastEmit) >= kProgressEmitIntervalMs;

    if (forceEmit || firstEmit || reachedTerminal || progressedEnough || timeoutReached) {
        m_lastProgressEmitMs.store(nowMs);
        emit progressChanged(clampedValue);
    }
}

void AIEngine::setProcessing(bool processing)
{
    m_isProcessing = processing;
    emit processingChanged(processing);
}

ncnn::Mat AIEngine::qimageToMat(const QImage &image, const ModelInfo &model)
{
    QImage img = image.convertToFormat(QImage::Format_RGB888);
    int w = img.width();
    int h = img.height();

    qInfo() << "[AIEngine][qimageToMat] input:" << image.size()
            << "converted:" << img.size()
            << "bytesPerLine:" << img.bytesPerLine()
            << "normMean:" << model.normMean[0] << model.normMean[1] << model.normMean[2]
            << "normScale:" << model.normScale[0] << model.normScale[1] << model.normScale[2];

    ncnn::Mat in = ncnn::Mat::from_pixels(
        img.constBits(),
        ncnn::Mat::PIXEL_RGB,
        w, h
    );

    in.substract_mean_normalize(model.normMean, model.normScale);

    qInfo() << "[AIEngine][qimageToMat] output mat:" << "w:" << in.w << "h:" << in.h << "c:" << in.c;

    return in;
}

QImage AIEngine::matToQimage(const ncnn::Mat &mat, const ModelInfo &model)
{
    int w = mat.w;
    int h = mat.h;

    qInfo() << "[AIEngine][matToQimage] input mat:" << "w:" << w << "h:" << h << "c:" << mat.c
            << "denormMean:" << model.denormMean[0] << model.denormMean[1] << model.denormMean[2]
            << "denormScale:" << model.denormScale[0] << model.denormScale[1] << model.denormScale[2];

    ncnn::Mat out = mat.clone();
    out.substract_mean_normalize(model.denormMean, model.denormScale);

    float *data = (float *)out.data;
    int total = w * h * out.c;

    float minVal = data[0], maxVal = data[0];
    for (int i = 0; i < total; ++i) {
        data[i] = std::clamp(data[i], 0.f, 255.f);
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }

    qInfo() << "[AIEngine][matToQimage] after clamp min:" << minVal << "max:" << maxVal << "total:" << total;

    QImage result(w, h, QImage::Format_RGB888);
    out.to_pixels(result.bits(), ncnn::Mat::PIXEL_RGB);

    qInfo() << "[AIEngine][matToQimage] output image:" << result.size() << "isNull:" << result.isNull();

    return result;
}

ncnn::Mat AIEngine::runInference(const ncnn::Mat &input, const ModelInfo &model)
{
    ncnn::Extractor ex = m_net.create_extractor();

    std::string inputBlob = model.inputBlobName.toStdString();
    std::string outputBlob = model.outputBlobName.toStdString();

    qInfo() << "[AIEngine][Inference] start"
            << "inputBlob:" << model.inputBlobName
            << "outputBlob:" << model.outputBlobName
            << "inputMat:" << "w:" << input.w << "h:" << input.h << "c:" << input.c
            << "netLoaded:" << m_currentModel.isLoaded;

    int inputRet = ex.input(inputBlob.c_str(), input);
    qInfo() << "[AIEngine][Inference] input result:" << inputRet;

    if (inputRet != 0) {
        qWarning() << "[AIEngine][Inference] Failed to set input blob:" << model.inputBlobName;
    }

    ncnn::Mat output;
    int extractRet = ex.extract(outputBlob.c_str(), output);

    qInfo() << "[AIEngine][Inference] extract result:" << extractRet
            << "outputMat:" << "w:" << output.w << "h:" << output.h << "c:" << output.c;

    if (extractRet != 0 || output.empty()) {
        qWarning() << "[AIEngine][Inference] Failed to extract output blob:" << model.outputBlobName
                   << "extractRet:" << extractRet << "empty:" << output.empty();
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

    qInfo() << "[AIEngine][Single] input mat created"
            << "w:" << in.w << "h:" << in.h << "c:" << in.c;

    if (m_cancelRequested) return QImage();

    setProgress(0.3);

    ncnn::Mat out = runInference(in, model);

    qInfo() << "[AIEngine][Single] inference done"
            << "output mat:" << "w:" << out.w << "h:" << out.h << "c:" << out.c;

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
    int tileSize = model.tileSize;
    int padding = model.tilePadding;
    int scale = model.scaleFactor;
    int w = input.width();
    int h = input.height();

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
            << "outputSize:" << (w * scale) << "x" << (h * scale);

    QImage output(w * scale, h * scale, QImage::Format_RGB888);
    output.fill(Qt::black);

    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    int tileIndex = 0;
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            if (m_cancelRequested) {
                painter.end();
                return QImage();
            }

            int x0 = tx * tileSize;
            int y0 = ty * tileSize;
            int x1 = std::min(x0 + tileSize, w);
            int y1 = std::min(y0 + tileSize, h);

            int px0 = std::max(x0 - padding, 0);
            int py0 = std::max(y0 - padding, 0);
            int px1 = std::min(x1 + padding, w);
            int py1 = std::min(y1 + padding, h);

            int tileW = px1 - px0;
            int tileH = py1 - py0;

            qInfo() << "[AIEngine][Tiled] tile" << tileIndex
                    << "pos:(" << tx << "," << ty << ")"
                    << "region:(" << x0 << "," << y0 << ")-(" << x1 << "," << y1 << ")"
                    << "padded:(" << px0 << "," << py0 << ")-(" << px1 << "," << py1 << ")"
                    << "tileSize:" << tileW << "x" << tileH;

            QImage tile = input.copy(px0, py0, tileW, tileH);

            ncnn::Mat in = qimageToMat(tile, model);
            ncnn::Mat out = runInference(in, model);
            QImage tileResult = matToQimage(out, model);

            qInfo() << "[AIEngine][Tiled] tile" << tileIndex
                    << "input:" << tile.size()
                    << "output:" << tileResult.size();

            int outPadLeft = (x0 - px0) * scale;
            int outPadTop = (y0 - py0) * scale;
            int outW = (x1 - x0) * scale;
            int outH = (y1 - y0) * scale;

            qInfo() << "[AIEngine][Tiled] tile" << tileIndex
                    << "crop:(" << outPadLeft << "," << outPadTop << ")"
                    << "size:" << outW << "x" << outH
                    << "drawAt:(" << (x0 * scale) << "," << (y0 * scale) << ")";

            QImage croppedResult = tileResult.copy(outPadLeft, outPadTop, outW, outH);

            painter.drawImage(x0 * scale, y0 * scale, croppedResult);

            tileIndex++;
            setProgress(0.1 + 0.85 * tileIndex / totalTiles);
        }
    }

    painter.end();
    return output;
}

QImage AIEngine::processWithTTA(const QImage &input, const ModelInfo &model)
{
    QList<QImage> transformed;
    QList<QImage> results;

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

    for (int i = 0; i < totalSteps; ++i) {
        if (m_cancelRequested) {
            qDebug() << "[AIEngine] TTA processing cancelled at step" << (i + 1) << "/" << totalSteps;
            return QImage();
        }

        emit progressTextChanged(tr("TTA 处理中: %1/%2").arg(i + 1).arg(totalSteps));
        setProgress(0.1 + 0.8 * static_cast<double>(i) / totalSteps);

        QImage result = processSingle(transformed[i], model);
        if (result.isNull()) {
            qWarning() << "[AIEngine] TTA step" << (i + 1) << "failed";
            return QImage();
        }

        if (i >= 4) {
            QTransform rot270;
            rot270.rotate(270);
            result = result.transformed(rot270);

            if (i == 5) result = result.mirrored(true, false);
            else if (i == 6) result = result.mirrored(false, true);
            else if (i == 7) result = result.mirrored(true, true);
        } else {
            if (i == 1) result = result.mirrored(true, false);
            else if (i == 2) result = result.mirrored(false, true);
            else if (i == 3) result = result.mirrored(true, true);
        }

        results.append(result);
    }

    emit progressTextChanged(tr("合并 TTA 结果..."));
    return mergeTTAResults(results);
}

QImage AIEngine::mergeTTAResults(const QList<QImage> &results)
{
    if (results.isEmpty()) {
        return QImage();
    }

    int w = results[0].width();
    int h = results[0].height();

    QImage merged(w, h, QImage::Format_RGB888);
    merged.fill(Qt::black);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sumR = 0, sumG = 0, sumB = 0;
            int count = 0;

            for (const auto &img : results) {
                if (x < img.width() && y < img.height()) {
                    QRgb pixel = img.pixel(x, y);
                    sumR += qRed(pixel);
                    sumG += qGreen(pixel);
                    sumB += qBlue(pixel);
                    count++;
                }
            }

            if (count > 0) {
                merged.setPixel(x, y, qRgb(sumR / count, sumG / count, sumB / count));
            }
        }
    }

    return merged;
}

QImage AIEngine::applyOutscale(const QImage &input, double scale)
{
    if (input.isNull() || scale <= 0) {
        return input;
    }

    if (std::abs(scale - 1.0) < 0.001) {
        return input;
    }

    int newWidth = static_cast<int>(input.width() * scale);
    int newHeight = static_cast<int>(input.height() * scale);

    return input.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// ========== 自动参数计算 ==========

QVariantMap AIEngine::computeAutoParams(const QSize &mediaSize, bool isVideo) const
{
    QVariantMap result;

    if (!m_currentModel.isLoaded && m_currentModel.paramPath.isEmpty()) {
        // OpenCV 引擎或模型未加载 — 仅填充通用字段
        return result;
    }

    const int w = mediaSize.width();
    const int h = mediaSize.height();
    const int maxDim = std::max(w, h);
    Q_UNUSED(std::min(w, h));  // minDim reserved for future per-model use
    const qint64 pixels = static_cast<qint64>(w) * h;

    // 宽高比（> 1 表示横向，< 1 表示纵向）
    const double aspectRatio = (h > 0) ? static_cast<double>(w) / h : 1.0;
    // 是否超宽（21:9 及以上）
    const bool isUltraWide = (aspectRatio >= 2.0);
    // 是否竖向（9:16 及以下，移动端竖屏）
    const bool isPortrait = (aspectRatio <= 0.7);

    // ── 1. 自动分块大小（所有需要分块的模型通用） ─────────────────
    const int autoTile = computeAutoTileSize(mediaSize);
    result["tileSize"] = autoTile;

    // ── 2. 按模型类别进行参数自动推荐 ──────────────────────────────
    // 规则原则：
    //   a) 遍历 supportedParams 中定义的每一个参数键，确保完整覆盖
    //   b) 图像与视频分支分别优化，视频优先稳定性（避免闪烁/拖影），图像优先质量
    //   c) 参数值始终限制在模型元数据中声明的 [min, max] 范围内

    auto clampParam = [&](const QString &key, double rawVal) -> double {
        if (!m_currentModel.supportedParams.contains(key)) return rawVal;
        const QVariantMap &meta = m_currentModel.supportedParams[key].toMap();
        double lo = meta.contains("min") ? meta["min"].toDouble() : -1e9;
        double hi = meta.contains("max") ? meta["max"].toDouble() : 1e9;
        return std::clamp(rawVal, lo, hi);
    };

    switch (m_currentModel.category) {

    // ── 超分辨率 ──────────────────────────────────────────────────
    case ModelCategory::SuperResolution: {
        const double modelScale = static_cast<double>(m_currentModel.scaleFactor);

        // outscale：根据输入分辨率与媒体类型决定合理倍数
        //   - 视频：优先保守，减少显存/时间开销；1080p+ 使用 1x，720p~1080p 使用 0.75x 倍数
        //   - 图像：最大化质量；仅在 1080p+ 时限制到 0.5x 倍数
        if (m_currentModel.supportedParams.contains("outscale")) {
            double recommendedScale = modelScale;
            if (isVideo) {
                if (pixels > 2073600)      recommendedScale = std::max(1.0, modelScale * 0.5);  // 1080p+
                else if (pixels > 921600)  recommendedScale = std::max(1.0, modelScale * 0.75); // 720p-1080p
                else if (pixels > 409600)  recommendedScale = std::max(1.0, modelScale * 1.0);  // 480p-720p
                // 480p 以下：使用完整倍数
            } else {
                if (pixels > 2073600)      recommendedScale = std::max(1.0, modelScale * 0.5);  // 1080p+
                else if (pixels > 921600)  recommendedScale = std::max(1.0, modelScale * 0.75); // 720p-1080p
                // 720p 以下：使用完整倍数
            }
            // 四舍五入到 0.5 步长
            recommendedScale = std::round(recommendedScale * 2.0) / 2.0;
            result["outscale"] = clampParam("outscale", recommendedScale);
        }

        // tta_mode：仅对小图像启用，视频始终关闭（性能/时间原因）
        //   阈值：512×512 以下（262144 像素）
        if (m_currentModel.supportedParams.contains("tta_mode")) {
            result["tta_mode"] = (!isVideo && pixels <= 262144);
        }

        // face_enhance：图像默认开启（有助于人像超分），视频关闭（避免闪烁）
        if (m_currentModel.supportedParams.contains("face_enhance")) {
            result["face_enhance"] = !isVideo;
        }

        // uhd_mode：4K 及以上自动开启（减少显存峰值）
        //   4K = 3840×2160 = 8294400 px；视频和图像均适用
        if (m_currentModel.supportedParams.contains("uhd_mode")) {
            result["uhd_mode"] = (pixels >= 8294400);
        }

        // fp32（半精度/全精度选择）：大图或视频优先 fp16 以省显存
        if (m_currentModel.supportedParams.contains("fp32")) {
            // fp32=true 表示使用全精度，大图/视频用 fp16(false) 更稳定
            result["fp32"] = (pixels <= 262144 && !isVideo);
        }

        // denoise（后处理降噪强度，部分超分模型支持）
        if (m_currentModel.supportedParams.contains("denoise")) {
            // 视频：轻微降噪防止闪烁；图像：根据尺寸适度降噪
            float dn = isVideo ? 0.3f : (pixels < 65536 ? 0.5f : 0.2f);
            result["denoise"] = static_cast<double>(clampParam("denoise", dn));
        }

        break;
    }

    // ── 去噪 ──────────────────────────────────────────────────────
    case ModelCategory::Denoising: {
        // noise_threshold：像素越少（压缩更严重）噪点越多，使用较高阈值
        //   视频额外 +10，因为视频压缩引入更多块状噪声
        if (m_currentModel.supportedParams.contains("noise_threshold")) {
            float threshold = 50.0f;
            if      (pixels < 65536)   threshold = 70.0f;  // 256×256 以下
            else if (pixels < 262144)  threshold = 60.0f;  // ~512×512
            else if (pixels > 2073600) threshold = 35.0f;  // 1080p 以上
            if (isVideo) threshold = std::min(threshold + 10.0f, 80.0f);
            result["noise_threshold"] = static_cast<double>(clampParam("noise_threshold", threshold));
        }
        // noise_level（离散等级版本）：0=无,1=轻,2=中,3=重
        if (m_currentModel.supportedParams.contains("noise_level")) {
            int level = isVideo ? 2 : 3;
            if      (pixels < 65536)   level = isVideo ? 3 : 4;
            else if (pixels > 2073600) level = isVideo ? 1 : 2;
            result["noise_level"] = static_cast<int>(clampParam("noise_level", static_cast<double>(level)));
        }
        // color_denoise：彩色去噪，视频建议关（保持肤色/色彩稳定）
        if (m_currentModel.supportedParams.contains("color_denoise")) {
            result["color_denoise"] = !isVideo;
        }
        // sharpness_preserve：去噪后保锐度，大图效果更明显
        if (m_currentModel.supportedParams.contains("sharpness_preserve")) {
            result["sharpness_preserve"] = (pixels >= 409600); // 640×640 以上
        }
        break;
    }

    // ── 去模糊 ────────────────────────────────────────────────────
    case ModelCategory::Deblurring: {
        // deblur_strength：小图/扫描件来源模糊更严重，建议强度稍高
        //   视频运动模糊通常是轻度，不宜过强以免产生振铃
        if (m_currentModel.supportedParams.contains("deblur_strength")) {
            float strength = 1.0f;
            if (pixels < 65536) {
                strength = isVideo ? 1.0f : 1.3f;
            } else if (pixels > 2073600) {
                strength = isVideo ? 0.6f : 0.8f;
            } else if (isVideo) {
                strength = 0.8f;
            }
            result["deblur_strength"] = static_cast<double>(clampParam("deblur_strength", strength));
        }
        // iterations（多轮去模糊迭代）：图像可多轮，视频为避免拖影限1轮
        if (m_currentModel.supportedParams.contains("iterations")) {
            result["iterations"] = static_cast<int>(clampParam("iterations", isVideo ? 1.0 : 2.0));
        }
        // motion_blur（是否针对运动模糊优化）：视频优先开启
        if (m_currentModel.supportedParams.contains("motion_blur")) {
            result["motion_blur"] = isVideo;
        }
        break;
    }

    // ── 去雾 ──────────────────────────────────────────────────────
    case ModelCategory::Dehazing: {
        // dehaze_strength：全量去雾（1.0）是基准；
        //   超宽/竖屏构图的视频（通常是户外/行车记录仪）可适度加强
        if (m_currentModel.supportedParams.contains("dehaze_strength")) {
            float strength = 1.0f;
            if (isVideo && (isUltraWide || isPortrait)) strength = 1.1f;
            result["dehaze_strength"] = static_cast<double>(clampParam("dehaze_strength", strength));
        }
        // sky_protect：保护天空区域不过曝，大尺寸图/视频更容易出现此问题
        if (m_currentModel.supportedParams.contains("sky_protect")) {
            result["sky_protect"] = (pixels >= 409600);
        }
        // color_correct：去雾后色偏校正，默认开启
        if (m_currentModel.supportedParams.contains("color_correct")) {
            result["color_correct"] = true;
        }
        break;
    }

    // ── 上色 ──────────────────────────────────────────────────────
    case ModelCategory::Colorization: {
        // render_factor：值越大，网格分辨率越高，上色越细腻（但更慢）
        //   高分辨率图/视频需要更高的 render_factor 维持细节密度
        if (m_currentModel.supportedParams.contains("render_factor")) {
            int rf = 35;
            if      (maxDim >= 1920) rf = isVideo ? 38 : 45;
            else if (maxDim >= 1280) rf = isVideo ? 35 : 40;
            else if (maxDim <= 256)  rf = 20;
            result["render_factor"] = static_cast<int>(clampParam("render_factor", static_cast<double>(rf)));
        }
        // artistic_mode：艺术感色调；图像默认开，视频关（减少帧间色彩抖动）
        if (m_currentModel.supportedParams.contains("artistic_mode")) {
            result["artistic_mode"] = !isVideo;
        }
        // temporal_consistency（视频帧间色彩一致性）：视频专属，默认开启
        if (m_currentModel.supportedParams.contains("temporal_consistency")) {
            result["temporal_consistency"] = isVideo;
        }
        // saturation_boost：饱和度增强；大分辨率图/视频适度降低以免过饱和
        if (m_currentModel.supportedParams.contains("saturation_boost")) {
            float sat = (pixels > 921600) ? 0.8f : 1.0f;
            result["saturation_boost"] = static_cast<double>(clampParam("saturation_boost", sat));
        }
        break;
    }

    // ── 低光增强 ──────────────────────────────────────────────────
    case ModelCategory::LowLight: {
        // enhancement_strength：超低分辨率图（手机夜间截图）噪点更多，强度适度提高
        //   视频夜拍帧间亮度波动大，略微降低强度以稳定输出
        if (m_currentModel.supportedParams.contains("enhancement_strength")) {
            float es = 1.0f;
            if      (pixels < 65536)   es = isVideo ? 1.1f : 1.3f;
            else if (pixels > 2073600) es = isVideo ? 0.8f : 0.9f;
            else if (isVideo)          es = 0.9f;
            result["enhancement_strength"] = static_cast<double>(clampParam("enhancement_strength", es));
        }
        // exposure_correction：自动曝光补偿，默认开启
        if (m_currentModel.supportedParams.contains("exposure_correction")) {
            result["exposure_correction"] = true;
        }
        // noise_suppression：低光增强后降噪；视频必须开，图像可按尺寸决定
        if (m_currentModel.supportedParams.contains("noise_suppression")) {
            result["noise_suppression"] = isVideo || (pixels < 262144);
        }
        // gamma_correction：Gamma 矫正；宽屏/超宽视频保持稳定
        if (m_currentModel.supportedParams.contains("gamma_correction")) {
            result["gamma_correction"] = true;
        }
        break;
    }

    // ── 视频插帧 ──────────────────────────────────────────────────
    case ModelCategory::FrameInterpolation: {
        // time_step：中间帧时间步长，0.5 = 正中间插 1 帧（2x 帧率）
        if (m_currentModel.supportedParams.contains("time_step")) {
            result["time_step"] = static_cast<double>(clampParam("time_step", 0.5));
        }
        // uhd_mode：4K 视频自动开启（减少峰值显存）
        if (m_currentModel.supportedParams.contains("uhd_mode")) {
            result["uhd_mode"] = (pixels >= 8294400);
        }
        // tta_spatial：空间 TTA，提升质量但 4x 慢，视频默认关
        if (m_currentModel.supportedParams.contains("tta_spatial")) {
            result["tta_spatial"] = false;
        }
        // tta_temporal：时序 TTA，改善帧间一致性，性能开销高，默认关
        if (m_currentModel.supportedParams.contains("tta_temporal")) {
            result["tta_temporal"] = false;
        }
        // scale（插帧模型的超分倍数，部分模型同时支持超分+插帧）
        if (m_currentModel.supportedParams.contains("scale")) {
            // 4K 视频不建议叠加超分，其余默认 1x
            result["scale"] = static_cast<int>(clampParam("scale", pixels >= 8294400 ? 1.0 : 1.0));
        }
        // scene_detection：场景切换检测，避免在硬切处插出错误帧
        if (m_currentModel.supportedParams.contains("scene_detection")) {
            result["scene_detection"] = true;
        }
        break;
    }

    // ── 图像修复 ──────────────────────────────────────────────────
    case ModelCategory::Inpainting: {
        // inpaint_radius：修复半径随分辨率增大而增大，保持视觉效果一致
        if (m_currentModel.supportedParams.contains("inpaint_radius")) {
            int radius = 3;
            if      (maxDim >= 1920) radius = 5;
            else if (maxDim >= 1280) radius = 4;
            else if (maxDim <= 256)  radius = 2;
            result["inpaint_radius"] = static_cast<int>(clampParam("inpaint_radius", static_cast<double>(radius)));
        }
        // inpaint_method：0=Telea(快速), 1=Navier-Stokes(高质量)
        //   大图/图像首选高质量，视频/小图用快速
        if (m_currentModel.supportedParams.contains("inpaint_method")) {
            result["inpaint_method"] = static_cast<int>(clampParam(
                "inpaint_method",
                (!isVideo && pixels >= 262144) ? 1.0 : 0.0
            ));
        }
        // feather_edge（羽化边缘）：大图默认开启，减少修复边界感
        if (m_currentModel.supportedParams.contains("feather_edge")) {
            result["feather_edge"] = (pixels >= 262144);
        }
        break;
    }

    default:
        break;
    }

    qInfo() << "[AIEngine][AutoParams]"
             << "model:" << m_currentModelId
             << "mediaSize:" << w << "x" << h
             << "isVideo:" << isVideo
             << "aspectRatio:" << aspectRatio
             << "isUltraWide:" << isUltraWide
             << "isPortrait:" << isPortrait
             << "category:" << static_cast<int>(m_currentModel.category)
             << "result:" << result;

    return result;
}

int AIEngine::computeAutoTileSize(const QSize &inputSize) const
{
    if (!m_currentModel.isLoaded && m_currentModel.paramPath.isEmpty()) {
        // 模型未加载（如 OpenCV 类），不分块
        return 0;
    }

    // 若模型配置声明 tileSize==0，说明该模型不需要/不支持分块
    if (m_currentModel.tileSize == 0) {
        return 0;
    }

    const int w = inputSize.width();
    const int h = inputSize.height();
    const int scale = std::max(1, m_currentModel.scaleFactor);
    const int channels = m_currentModel.outputChannels > 0 ? m_currentModel.outputChannels : 3;

    // ── 策略参数 ──────────────────────────────────────────────
    // GPU 推理时每个像素约占 sizeof(float)×channels 字节；
    // 加上 scale² 倍的输出、中间激活层（约 kFactor 倍），
    // kFactor = 6 是一个保守估计（输入+输出+激活层≈3-8×）。
    constexpr double kFactor = 6.0;
    constexpr qint64 kBytesPerMB = 1024LL * 1024;

    // 可用显存/内存估算
    qint64 availableMB = 0;
    if (m_gpuAvailable && m_useGpu) {
#if NCNN_VULKAN
        if (m_vkdev) {
            // NCNN 没有直接暴露剩余显存接口，使用保守固定值：
            // 常见低端 GPU 4 GB，保留 1 GB 给系统，留 512 MB 给其他进程
            availableMB = 2048; // 2 GB 作为默认保守值
        }
#endif
        if (availableMB <= 0) availableMB = 1024;
    } else {
        // CPU 模式：受限于内存，更保守
        availableMB = 512;
    }

    // 每个分块的显存消耗（MB）
    // tile_pixels × channels × sizeof(float) × kFactor × scale²
    // 其中 scale² 因为输出尺寸是输入的 scale 倍（超分辨率）
    auto memForTile = [&](int tile) -> double {
        const qint64 pixels = static_cast<qint64>(tile) * tile;
        const qint64 bytes = pixels * channels * static_cast<qint64>(sizeof(float))
                             * static_cast<qint64>(scale * scale);
        return static_cast<double>(bytes) * kFactor / kBytesPerMB;
    };

    // ── 搜索最佳分块大小 ─────────────────────────────────────
    // 范围：[64, 1024]，步长 64
    // 同时不能超过图像本身的尺寸（超过则无需分块）
    const int maxPossibleTile = std::max(w, h);
    const int minTile = 64;
    const int maxTile = std::min(1024, maxPossibleTile);
    const int step = 64;

    int bestTile = minTile;
    for (int t = maxTile; t >= minTile; t -= step) {
        if (memForTile(t) <= static_cast<double>(availableMB)) {
            bestTile = t;
            break;
        }
    }

    // 若图像尺寸本身小于等于 bestTile，整图可以一次推理，不需要分块
    if (w <= bestTile && h <= bestTile) {
        return 0;
    }

    qInfo() << "[AIEngine][AutoTile]"
            << "input:" << w << "x" << h
            << "scale:" << scale
            << "availableMB:" << availableMB
            << "bestTile:" << bestTile
            << "memForBestTile(MB):" << memForTile(bestTile);

    return bestTile;
}

} // namespace EnhanceVision
