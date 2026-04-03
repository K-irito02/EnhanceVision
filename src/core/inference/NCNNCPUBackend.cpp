/**
 * @file NCNNCPUBackend.cpp
 * @brief NCNN CPU 推理后端实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/inference/NCNNCPUBackend.h"
#include "EnhanceVision/core/inference/ImagePreprocessor.h"
#include "EnhanceVision/core/inference/ImagePostprocessor.h"
#include "EnhanceVision/core/inference/AutoParamCalculator.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <net.h>

namespace EnhanceVision {

NCNNCPUBackend::NCNNCPUBackend(QObject* parent)
    : IInferenceBackend(parent)
{
    qInfo() << "[NCNNCPUBackend] Initializing CPU-only inference backend";
    
    m_opt.num_threads = std::max(1, QThread::idealThreadCount() - 1);
    m_opt.use_packing_layout = true;
    m_opt.use_vulkan_compute = false;
    
    qInfo() << "[NCNNCPUBackend] CPU threads:" << m_opt.num_threads;
}

NCNNCPUBackend::~NCNNCPUBackend()
{
    shutdown();
}

bool NCNNCPUBackend::initialize(const BackendConfig& config)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isInitialized.load()) {
        qWarning() << "[NCNNCPUBackend] Already initialized";
        return true;
    }
    
    qInfo() << "[NCNNCPUBackend] Initializing with config";
    
    m_config = config;
    
    if (config.numThreads > 0) {
        m_opt.num_threads = config.numThreads;
    }
    
    m_isInitialized.store(true);
    qInfo() << "[NCNNCPUBackend] Initialization complete";
    
    return true;
}

void NCNNCPUBackend::shutdown()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_isInitialized.load()) {
        return;
    }
    
    qInfo() << "[NCNNCPUBackend] Shutting down";
    
    unloadModel();
    m_isInitialized.store(false);
    
    qInfo() << "[NCNNCPUBackend] Shutdown complete";
}

bool NCNNCPUBackend::loadModel(const ModelInfo& model)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isInferenceRunning.load()) {
        qWarning() << "[NCNNCPUBackend] Cannot load model while inference is running";
        return false;
    }
    
    if (model.paramPath.isEmpty() && model.binPath.isEmpty()) {
        m_currentModelId = model.id;
        m_currentModel = model;
        m_modelLoaded = true;
        emit modelLoaded(true, model.id);
        return true;
    }
    
    if (!model.isAvailable) {
        emitError(tr("模型文件不可用: %1").arg(model.id));
        return false;
    }
    
    if (m_currentModelId == model.id && m_modelLoaded) {
        return true;
    }
    
    unloadModel();
    
    m_net.opt = m_opt;
    
    int ret = m_net.load_param(model.paramPath.toStdString().c_str());
    if (ret != 0) {
        emitError(tr("加载模型参数失败: %1").arg(model.paramPath));
        return false;
    }
    
    ret = m_net.load_model(model.binPath.toStdString().c_str());
    if (ret != 0) {
        m_net.clear();
        emitError(tr("加载模型权重失败: %1").arg(model.binPath));
        return false;
    }
    
    m_currentModelId = model.id;
    m_currentModel = model;
    m_currentModel.isLoaded = true;
    m_currentModel.layerCount = static_cast<int>(m_net.layers().size());
    m_modelLoaded = true;
    
    qInfo() << "[NCNNCPUBackend] Model loaded:" << model.id
            << "layers:" << m_currentModel.layerCount;
    
    emit modelLoaded(true, model.id);
    return true;
}

void NCNNCPUBackend::unloadModel()
{
    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_modelLoaded = false;
        m_currentModelId.clear();
        m_currentModel = ModelInfo();
    }
}

bool NCNNCPUBackend::isModelLoaded() const
{
    return m_modelLoaded;
}

QString NCNNCPUBackend::currentModelId() const
{
    return m_currentModelId;
}

ModelInfo NCNNCPUBackend::currentModel() const
{
    return m_currentModel;
}

InferenceResult NCNNCPUBackend::inference(const InferenceRequest& request)
{
    QMutexLocker inferenceLocker(&m_inferenceMutex);
    
    if (!m_isInitialized.load()) {
        return InferenceResult::makeFailure(tr("后端未初始化"), 
                                            static_cast<int>(InferenceError::BackendNotInitialized));
    }
    
    if (!m_modelLoaded) {
        return InferenceResult::makeFailure(tr("模型未加载"), 
                                            static_cast<int>(InferenceError::ModelNotLoaded));
    }
    
    if (request.input.isNull() || request.input.bits() == nullptr) {
        return InferenceResult::makeFailure(tr("输入图像无效"), 
                                            static_cast<int>(InferenceError::InvalidInput));
    }
    
    setInferenceRunning(true);
    resetCancelFlag();
    setProgress(0.0);
    
    QElapsedTimer timer;
    timer.start();
    
    InferenceResult result;
    
    int tileSize = request.tileSize;
    if (tileSize == 0 && m_currentModel.tileSize > 0) {
        tileSize = AutoParamCalculator::computeTileSize(request.input.size(), m_currentModel);
    }
    
    bool needTile = tileSize > 0 && 
                    (request.input.width() > tileSize || request.input.height() > tileSize);
    
    if (request.ttaMode && !needTile) {
        result = processWithTTA(request.input);
    } else if (needTile) {
        result = processTiled(request.input, tileSize);
    } else {
        result = processSingle(request.input);
    }
    
    if (result.success && std::abs(request.outscale - 1.0) > 0.01) {
        result.output = ImagePostprocessor::applyOutscale(result.output, request.outscale);
    }
    
    setProgress(1.0);
    setInferenceRunning(false);
    
    result.inferenceTimeMs = timer.elapsed();
    
    emit inferenceCompleted(result);
    return result;
}

BackendCapabilities NCNNCPUBackend::capabilities() const
{
    BackendCapabilities caps;
    caps.supportsGPU = false;
    caps.supportsHalfPrecision = false;
    caps.supportsInt8 = false;
    caps.supportsDynamicBatch = false;
    caps.supportsDynamicInput = true;
    caps.maxBatchSize = 1;
    caps.maxMemoryMB = 4096;
    caps.deviceName = QStringLiteral("CPU");
    return caps;
}

bool NCNNCPUBackend::isInferenceRunning() const
{
    return m_isInferenceRunning.load();
}

InferenceResult NCNNCPUBackend::processSingle(const QImage& input)
{
    setProgress(0.05);
    
    ncnn::Mat in = ImagePreprocessor::qimageToMat(input, m_currentModel);
    if (in.empty()) {
        return InferenceResult::makeFailure(tr("图像预处理失败"), 
                                            static_cast<int>(InferenceError::InvalidInput));
    }
    
    if (isCancelRequested()) {
        return InferenceResult::makeFailure(tr("已取消"), 
                                            static_cast<int>(InferenceError::Cancelled));
    }
    
    setProgress(0.15);
    
    ncnn::Mat out = runInference(in);
    
    if (out.empty() || out.w <= 0 || out.h <= 0) {
        return InferenceResult::makeFailure(tr("推理失败"), 
                                            static_cast<int>(InferenceError::InferenceFailed));
    }
    
    if (isCancelRequested()) {
        return InferenceResult::makeFailure(tr("已取消"), 
                                            static_cast<int>(InferenceError::Cancelled));
    }
    
    setProgress(0.85);
    
    QImage result = ImagePostprocessor::matToQimage(out, m_currentModel);
    
    setProgress(0.95);
    
    return InferenceResult::makeSuccess(result);
}

InferenceResult NCNNCPUBackend::processTiled(const QImage& input, int tileSize)
{
    setProgress(0.01);
    
    int scale = m_currentModel.scaleFactor;
    int w = input.width();
    int h = input.height();
    
    int padding = m_currentModel.tilePadding;
    const int layerCount = m_currentModel.layerCount;
    if (layerCount > 500) {
        padding = std::max(padding, 64);
    } else if (layerCount > 200) {
        padding = std::max(padding, 48);
    } else if (layerCount > 50) {
        padding = std::max(padding, 24);
    }
    
    QImage paddedInput = ImagePreprocessor::createPaddedImage(input, padding);
    
    auto tiles = ImagePreprocessor::computeTiles(input.size(), tileSize);
    int totalTiles = static_cast<int>(tiles.size());
    
    setProgress(0.10);
    
    QImage output(w * scale, h * scale, QImage::Format_RGB888);
    output.fill(Qt::black);
    QPainter painter(&output);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    
    setProgress(0.15);
    
    int tileIndex = 0;
    int successfulTiles = 0;
    
    for (const auto& tileInfo : tiles) {
        if (isCancelRequested()) {
            painter.end();
            return InferenceResult::makeFailure(tr("已取消"), 
                                                static_cast<int>(InferenceError::Cancelled));
        }
        
        QImage tile = ImagePreprocessor::extractTile(paddedInput, tileInfo, padding);
        ncnn::Mat in = ImagePreprocessor::qimageToMat(tile, m_currentModel);
        if (in.empty()) {
            tileIndex++;
            continue;
        }
        
        ncnn::Mat out = runInference(in);
        if (out.empty()) {
            tileIndex++;
            continue;
        }
        
        QImage tileResult = ImagePostprocessor::matToQimage(out, m_currentModel);
        if (tileResult.isNull()) {
            tileIndex++;
            continue;
        }
        
        int outPadLeft = padding * scale;
        int outPadTop = padding * scale;
        int outW = tileInfo.width * scale;
        int outH = tileInfo.height * scale;
        
        QImage croppedResult = tileResult.copy(outPadLeft, outPadTop, outW, outH);
        
        int dstX = tileInfo.x * scale;
        int dstY = tileInfo.y * scale;
        
        if (croppedResult.width() != outW || croppedResult.height() != outH) {
            croppedResult = croppedResult.scaled(outW, outH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        
        painter.drawImage(dstX, dstY, croppedResult);
        tileIndex++;
        successfulTiles++;
        
        double tileProgress = 0.15 + 0.70 * static_cast<double>(tileIndex) / totalTiles;
        setProgress(tileProgress);
    }
    
    painter.end();
    
    setProgress(0.85);
    
    if (successfulTiles < totalTiles) {
        return InferenceResult::makeFailure(
            tr("分块处理未完成 (%1/%2)").arg(successfulTiles).arg(totalTiles), 
            static_cast<int>(InferenceError::InferenceFailed));
    }
    
    setProgress(0.95);
    
    return InferenceResult::makeSuccess(output);
}

InferenceResult NCNNCPUBackend::processWithTTA(const QImage& input)
{
    setProgress(0.02);
    
    auto transforms = ImagePreprocessor::generateTTATransforms(input);
    const int totalSteps = static_cast<int>(transforms.size());
    
    std::vector<QImage> results;
    
    for (int i = 0; i < totalSteps; ++i) {
        if (isCancelRequested()) {
            return InferenceResult::makeFailure(tr("已取消"), 
                                                static_cast<int>(InferenceError::Cancelled));
        }
        
        double stepProgress = 0.05 + 0.85 * static_cast<double>(i) / totalSteps;
        setProgress(stepProgress);
        
        InferenceResult result = processSingle(transforms[i]);
        if (!result.success) {
            continue;
        }
        
        QImage transformed = ImagePostprocessor::applyInverseTTATransform(result.output, i);
        results.push_back(transformed);
    }
    
    if (results.empty()) {
        return InferenceResult::makeFailure(tr("TTA 处理失败"), 
                                            static_cast<int>(InferenceError::InferenceFailed));
    }
    
    setProgress(0.92);
    
    QImage merged = ImagePostprocessor::mergeTTAResults(results);
    
    setProgress(0.95);
    
    return InferenceResult::makeSuccess(merged);
}

ncnn::Mat NCNNCPUBackend::runInference(const ncnn::Mat& input)
{
    if (input.empty() || input.w <= 0 || input.h <= 0) {
        qWarning() << "[NCNNCPUBackend] Empty input mat";
        return ncnn::Mat();
    }
    
    QStringList inputCandidates;
    inputCandidates << m_currentModel.inputBlobName << "input" << "data" << "in0" << "Input1";
    inputCandidates.removeAll(QString());
    inputCandidates.removeDuplicates();
    
    QStringList outputCandidates;
    outputCandidates << m_currentModel.outputBlobName << "output" << "out0" << "prob";
    outputCandidates.removeAll(QString());
    outputCandidates.removeDuplicates();
    
    QMutexLocker netLocker(&m_mutex);
    
    ncnn::Extractor ex = m_net.create_extractor();
    ex.set_light_mode(false);
    
    QString selectedInputBlob;
    int inputRet = -1;
    for (const QString& candidate : inputCandidates) {
        inputRet = ex.input(candidate.toStdString().c_str(), input);
        if (inputRet == 0) {
            selectedInputBlob = candidate;
            break;
        }
    }
    
    if (inputRet != 0) {
        emitError(tr("推理输入失败，模型输入节点不匹配"));
        return ncnn::Mat();
    }
    
    ncnn::Mat output;
    int extractRet = -1;
    
    for (const QString& candidate : outputCandidates) {
        ncnn::Mat tmp;
        extractRet = ex.extract(candidate.toStdString().c_str(), tmp);
        if (extractRet == 0 && !tmp.empty()) {
            output = tmp.clone();
            break;
        }
    }
    
    if (extractRet != 0 || output.empty()) {
        emitError(tr("推理输出失败，模型输出节点不匹配"));
        return ncnn::Mat();
    }
    
    return output;
}

void NCNNCPUBackend::setProgress(double value)
{
    m_progress.store(value);
    emit progressChanged(value);
}

void NCNNCPUBackend::emitError(const QString& error, int code)
{
    Q_UNUSED(code);
    qWarning() << "[NCNNCPUBackend] Error:" << error;
}

void NCNNCPUBackend::resetCancelFlag()
{
    m_cancelRequested.store(false);
}

bool NCNNCPUBackend::isCancelRequested() const
{
    return m_cancelRequested.load();
}

void NCNNCPUBackend::setInferenceRunning(bool running)
{
    m_isInferenceRunning.store(running);
}

} // namespace EnhanceVision
