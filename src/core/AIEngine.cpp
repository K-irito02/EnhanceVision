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
        qDebug() << "[AIEngine] Vulkan GPU available, device index:" << gpuIndex;
    } else {
        m_gpuAvailable = false;
        qDebug() << "[AIEngine] No Vulkan GPU found, using CPU mode";
    }
#else
    m_gpuAvailable = false;
    qDebug() << "[AIEngine] NCNN built without Vulkan support, using CPU mode";
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

    m_currentModelId = modelId;
    m_currentModel = info;
    m_currentModel.isLoaded = true;

    qDebug() << "[AIEngine] Model loaded:" << info.name
             << "GPU:" << (m_opt.use_vulkan_compute ? "ON" : "OFF");

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

    // 判断是否需要分块处理
    int tileSize = m_currentModel.tileSize;
    if (m_parameters.contains("tileSize") && m_parameters["tileSize"].toInt() > 0) {
        tileSize = m_parameters["tileSize"].toInt();
    }

    bool needTile = (tileSize > 0) &&
                    (input.width() > tileSize || input.height() > tileSize);

    if (needTile) {
        result = processTiled(input, m_currentModel);
    } else {
        result = processSingle(input, m_currentModel);
    }

    setProgress(1.0);
    setProcessing(false);

    if (!result.isNull()) {
        emit processCompleted(result);
    }

    return result;
}

void AIEngine::processAsync(const QString &inputPath, const QString &outputPath)
{
    if (m_isProcessing) {
        emit processError(tr("已有推理任务正在进行"));
        return;
    }

    QtConcurrent::run([this, inputPath, outputPath]() {
        QImage inputImage(inputPath);
        if (inputImage.isNull()) {
            emit processError(tr("无法读取图像: %1").arg(inputPath));
            emit processFileCompleted(false, QString(), tr("无法读取图像"));
            return;
        }

        QImage result = process(inputImage);

        if (m_cancelRequested) {
            emit processFileCompleted(false, QString(), tr("推理已取消"));
            return;
        }

        if (result.isNull()) {
            emit processFileCompleted(false, QString(), tr("推理失败"));
            return;
        }

        if (!result.save(outputPath)) {
            emit processError(tr("无法保存结果: %1").arg(outputPath));
            emit processFileCompleted(false, QString(), tr("保存失败"));
            return;
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

void AIEngine::setProgress(double value)
{
    m_progress = value;
    emit progressChanged(value);
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

    ncnn::Mat in = ncnn::Mat::from_pixels(
        img.constBits(),
        ncnn::Mat::PIXEL_RGB,
        w, h
    );

    in.substract_mean_normalize(model.normMean, model.normScale);

    return in;
}

QImage AIEngine::matToQimage(const ncnn::Mat &mat, const ModelInfo &model)
{
    int w = mat.w;
    int h = mat.h;

    ncnn::Mat out = mat.clone();
    out.substract_mean_normalize(model.denormMean, model.denormScale);

    // 裁剪到 [0, 255]
    float *data = (float *)out.data;
    int total = w * h * out.c;
    for (int i = 0; i < total; ++i) {
        data[i] = std::clamp(data[i], 0.f, 255.f);
    }

    QImage result(w, h, QImage::Format_RGB888);
    out.to_pixels(result.bits(), ncnn::Mat::PIXEL_RGB);

    return result;
}

ncnn::Mat AIEngine::runInference(const ncnn::Mat &input, const ModelInfo &model)
{
    ncnn::Extractor ex = m_net.create_extractor();

#if NCNN_VULKAN
    if (m_opt.use_vulkan_compute) {
        ex.set_vulkan_compute(true);
    }
#endif

    ex.input(model.inputBlobName.toStdString().c_str(), input);

    ncnn::Mat output;
    ex.extract(model.outputBlobName.toStdString().c_str(), output);

    return output;
}

QImage AIEngine::processSingle(const QImage &input, const ModelInfo &model)
{
    setProgress(0.1);

    ncnn::Mat in = qimageToMat(input, model);

    if (m_cancelRequested) return QImage();

    setProgress(0.3);

    ncnn::Mat out = runInference(in, model);

    if (m_cancelRequested) return QImage();

    setProgress(0.8);

    QImage result = matToQimage(out, model);

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

    // 计算分块数
    int tilesX = (w + tileSize - 1) / tileSize;
    int tilesY = (h + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;

    // 输出图像
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

            // 计算分块区域（含 padding）
            int x0 = tx * tileSize;
            int y0 = ty * tileSize;
            int x1 = std::min(x0 + tileSize, w);
            int y1 = std::min(y0 + tileSize, h);

            int px0 = std::max(x0 - padding, 0);
            int py0 = std::max(y0 - padding, 0);
            int px1 = std::min(x1 + padding, w);
            int py1 = std::min(y1 + padding, h);

            // 裁剪出带 padding 的分块
            QImage tile = input.copy(px0, py0, px1 - px0, py1 - py0);

            // 推理
            ncnn::Mat in = qimageToMat(tile, model);
            ncnn::Mat out = runInference(in, model);
            QImage tileResult = matToQimage(out, model);

            // 计算输出区域（去掉 padding）
            int outPadLeft = (x0 - px0) * scale;
            int outPadTop = (y0 - py0) * scale;
            int outW = (x1 - x0) * scale;
            int outH = (y1 - y0) * scale;

            // 裁剪掉 padding 部分
            QImage croppedResult = tileResult.copy(outPadLeft, outPadTop, outW, outH);

            // 绘制到输出图像
            painter.drawImage(x0 * scale, y0 * scale, croppedResult);

            tileIndex++;
            setProgress(0.1 + 0.85 * tileIndex / totalTiles);
        }
    }

    painter.end();
    return output;
}

} // namespace EnhanceVision
