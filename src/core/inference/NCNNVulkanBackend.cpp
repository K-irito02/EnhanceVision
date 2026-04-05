/**
 * @file NCNNVulkanBackend.cpp
 * @brief NCNN Vulkan GPU 推理后端实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/inference/NCNNVulkanBackend.h"
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
#include <gpu.h>

namespace EnhanceVision {

NCNNVulkanBackend::NCNNVulkanBackend(QObject* parent)
    : IInferenceBackend(parent)
{
    qInfo() << "[NCNNVulkanBackend] Initializing Vulkan GPU inference backend";
    
    m_opt.num_threads = 0;
    m_opt.use_packing_layout = true;
    m_opt.use_vulkan_compute = true;
    m_opt.lightmode = true;
    m_opt.use_local_pool_allocator = true;
    
    qInfo() << "[NCNNVulkanBackend] Backend created (GPU resources initialized on demand)";
}

NCNNVulkanBackend::~NCNNVulkanBackend()
{
    shutdown();
}

bool NCNNVulkanBackend::isVulkanSupported()
{
#ifdef NCNN_VULKAN_AVAILABLE
    try {
        int ret = ncnn::create_gpu_instance();
        if (ret == 0) {
            ncnn::destroy_gpu_instance();
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
#else
    return false;
#endif
}

NCNNVulkanBackend::VulkanDeviceInfo NCNNVulkanBackend::probeDevice()
{
    VulkanDeviceInfo result;

#ifdef NCNN_VULKAN_AVAILABLE
    try {
        int ret = ncnn::create_gpu_instance();
        if (ret != 0) {
            result.errorReason = QStringLiteral("创建 Vulkan 实例失败");
            return result;
        }

        int deviceCount = ncnn::get_gpu_count();
        result.deviceCount = deviceCount;

        if (deviceCount <= 0) {
            ncnn::destroy_gpu_instance();
            result.errorReason = QStringLiteral("未找到支持 Vulkan 的 GPU");
            return result;
        }

        const ncnn::GpuInfo& gpuInfo = ncnn::get_gpu_info(0);
        result.available = true;
        result.deviceName = QString::fromUtf8(gpuInfo.device_name());

        ncnn::destroy_gpu_instance();

        qInfo() << "[NCNNVulkanBackend] Vulkan probe successful:"
                << "device:" << result.deviceName
                << "count:" << deviceCount;
    } catch (const std::exception& e) {
        result.errorReason = QString::fromStdString(e.what());
        qWarning() << "[NCNNVulkanBackend] Vulkan probe exception:" << e.what();
    } catch (...) {
        result.errorReason = QStringLiteral("Vulkan 初始化异常");
    }
#else
    result.errorReason = QStringLiteral("程序未编译 Vulkan 支持 (NCNN_VULKAN_AVAILABLE 未定义)");
#endif

    return result;
}

bool NCNNVulkanBackend::initialize(const BackendConfig& config)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isInitialized.load()) {
        qWarning() << "[NCNNVulkanBackend] Already initialized";
        return true;
    }
    
    qInfo() << "[NCNNVulkanBackend] Initializing with config";
    
    m_config = config;
    m_gpuDeviceId = config.gpuDeviceId;
    
    if (!initGpuResources()) {
        m_isInitialized.store(false);
        return false;
    }
    
    if (config.numThreads > 0) {
        m_opt.num_threads = config.numThreads;
    }
    
    m_isInitialized.store(true);
    qInfo() << "[NCNNVulkanBackend] Initialization complete, GPU device:" << m_gpuDeviceId;
    
    return true;
}

void NCNNVulkanBackend::shutdown()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_isInitialized.load()) {
        return;
    }
    
    qInfo() << "[NCNNVulkanBackend] Shutting down";
    
    unloadModel();
    releaseGpuResources();
    m_isInitialized.store(false);
    
    qInfo() << "[NCNNVulkanBackend] Shutdown complete";
}

bool NCNNVulkanBackend::initGpuResources()
{
#ifdef NCNN_VULKAN_AVAILABLE
    try {
        if (m_vulkanInstanceCreated) {
            qInfo() << "[NCNNVulkanBackend] Vulkan instance already created";
        } else {
            int ret = ncnn::create_gpu_instance();
            if (ret != 0) {
                emitError(tr("Failed to create Vulkan instance, error code: %1").arg(ret));
                return false;
            }
            m_vulkanInstanceCreated = true;
        }

        int deviceCount = ncnn::get_gpu_count();
        if (deviceCount <= 0) {
            emitError(tr("No Vulkan-capable GPU device found"));
            return false;
        }

        if (m_gpuDeviceId >= deviceCount) {
            qWarning() << "[NCNNVulkanBackend] Requested GPU device" << m_gpuDeviceId
                       << "not available, using device 0";
            m_gpuDeviceId = 0;
        }

        const ncnn::GpuInfo& gpuInfo = ncnn::get_gpu_info(m_gpuDeviceId);
        (void)gpuInfo;

        m_gpuInitialized = true;

        qInfo() << "[NCNNVulkanBackend] GPU resources initialized"
                << "device:" << m_gpuDeviceId
                << "name:" << QString::fromUtf8(gpuInfo.device_name());

        return true;
    } catch (const std::exception& e) {
        qWarning() << "[NCNNVulkanBackend] GPU init exception:" << e.what();
        emitError(tr("GPU initialization failed: %1").arg(e.what()));
        releaseGpuResources();
        return false;
    } catch (...) {
        qWarning() << "[NCNNVulkanBackend] GPU init unknown exception";
        emitError(tr("GPU initialization failed: unknown exception"));
        releaseGpuResources();
        return false;
    }
#else
    emitError(tr("Application not compiled with Vulkan support"));
    return false;
#endif
}

void NCNNVulkanBackend::releaseGpuResources()
{
#ifdef NCNN_VULKAN_AVAILABLE
    if (m_vulkanInstanceCreated) {
        try {
            ncnn::destroy_gpu_instance();
        } catch (...) {
            qWarning() << "[NCNNVulkanBackend] Exception during GPU resource release";
        }
        m_vulkanInstanceCreated = false;
    }
#endif
    m_gpuInitialized = false;
    qInfo() << "[NCNNVulkanBackend] GPU resources released";
}

bool NCNNVulkanBackend::loadModel(const ModelInfo& model)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isInferenceRunning.load()) {
        qWarning() << "[NCNNVulkanBackend] Cannot load model while inference is running";
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
        emitError(tr("Model file unavailable: %1").arg(model.id));
        return false;
    }
    
    if (m_currentModelId == model.id && m_modelLoaded) {
        return true;
    }
    
    unloadModel();
    
    m_net.opt = m_opt;

#ifdef NCNN_VULKAN_AVAILABLE
    if (m_gpuInitialized && m_vulkanInstanceCreated) {
        m_net.set_vulkan_device(m_gpuDeviceId);
        qInfo() << "[NCNNVulkanBackend] Model will use GPU device:" << m_gpuDeviceId;
    }
#endif
    
    int ret = m_net.load_param(model.paramPath.toStdString().c_str());
    if (ret != 0) {
        emitError(tr("Failed to load model parameters: %1").arg(model.paramPath));
        return false;
    }
    
    ret = m_net.load_model(model.binPath.toStdString().c_str());
    if (ret != 0) {
        m_net.clear();
        emitError(tr("Failed to load model weights: %1").arg(model.binPath));
        return false;
    }
    
    m_currentModelId = model.id;
    m_currentModel = model;
    m_currentModel.isLoaded = true;
    m_currentModel.layerCount = static_cast<int>(m_net.layers().size());
    m_modelLoaded = true;
    
    qInfo() << "[NCNNVulkanBackend] Model loaded on GPU:" << model.id
            << "layers:" << m_currentModel.layerCount;
    
    emit modelLoaded(true, model.id);
    return true;
}

void NCNNVulkanBackend::unloadModel()
{
    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_modelLoaded = false;
        m_currentModelId.clear();
        m_currentModel = ModelInfo();
    }
}

bool NCNNVulkanBackend::isModelLoaded() const
{
    return m_modelLoaded;
}

QString NCNNVulkanBackend::currentModelId() const
{
    return m_currentModelId;
}

ModelInfo NCNNVulkanBackend::currentModel() const
{
    return m_currentModel;
}

InferenceResult NCNNVulkanBackend::inference(const InferenceRequest& request)
{
    QMutexLocker inferenceLocker(&m_inferenceMutex);
    
    if (!m_isInitialized.load()) {
        return InferenceResult::makeFailure(tr("Backend not initialized"), 
                                            static_cast<int>(InferenceError::BackendNotInitialized));
    }
    
    if (!m_modelLoaded) {
        return InferenceResult::makeFailure(tr("No model loaded"), 
                                            static_cast<int>(InferenceError::ModelNotLoaded));
    }
    
    if (request.input.isNull() || request.input.bits() == nullptr) {
        return InferenceResult::makeFailure(tr("Invalid input image"), 
                                            static_cast<int>(InferenceError::InvalidInput));
    }

#ifdef NCNN_VULKAN_AVAILABLE
    try {
#endif
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

#ifdef NCNN_VULKAN_AVAILABLE
    } catch (const std::bad_alloc&) {
        qWarning() << "[NCNNVulkanBackend] GPU out of memory during inference";
        setInferenceRunning(false);
        setProgress(1.0);
        emitError(tr("GPU memory insufficient, please switch to CPU mode or reduce tile size"));
        return InferenceResult::makeFailure(
            tr("GPU memory insufficient, recommend switching to CPU mode"),
            static_cast<int>(InferenceError::OutOfMemory));
    } catch (const std::exception& e) {
        qWarning() << "[NCNNVulkanBackend] Inference exception:" << e.what();
        setInferenceRunning(false);
        setProgress(1.0);
        emitError(tr("GPU inference error: %1").arg(e.what()));
        return InferenceResult::makeFailure(
            tr("GPU inference failed: %1").arg(e.what()),
            static_cast<int>(InferenceError::InferenceFailed));
    } catch (...) {
        qWarning() << "[NCNNVulkanBackend] Unknown inference exception";
        setInferenceRunning(false);
        setProgress(1.0);
        emitError(tr("GPU inference unknown error"));
        return InferenceResult::makeFailure(
            tr("GPU inference unknown error"),
            static_cast<int>(InferenceError::Unknown));
    }
#endif
}

BackendCapabilities NCNNVulkanBackend::capabilities() const
{
    BackendCapabilities caps;
    caps.supportsGPU = true;
    caps.supportsHalfPrecision = true;
    caps.supportsInt8 = false;
    caps.supportsDynamicBatch = false;
    caps.supportsDynamicInput = true;
    caps.maxBatchSize = 1;
    caps.maxMemoryMB = 4096;
    caps.deviceName = m_gpuInitialized ? QStringLiteral("Vulkan GPU") : QStringLiteral("N/A");
    return caps;
}

bool NCNNVulkanBackend::isInferenceRunning() const
{
    return m_isInferenceRunning.load();
}

InferenceResult NCNNVulkanBackend::processSingle(const QImage& input)
{
    setProgress(0.05);
    
    ncnn::Mat in = ImagePreprocessor::qimageToMat(input, m_currentModel);
    if (in.empty()) {
        return InferenceResult::makeFailure(tr("Image preprocessing failed"), 
                                            static_cast<int>(InferenceError::InvalidInput));
    }
    
    if (isCancelRequested()) {
        return InferenceResult::makeFailure(tr("Cancelled"), 
                                            static_cast<int>(InferenceError::Cancelled));
    }
    
    setProgress(0.15);
    
    ncnn::Mat out = runInference(in);
    
    if (out.empty() || out.w <= 0 || out.h <= 0) {
        return InferenceResult::makeFailure(tr("Inference failed"), 
                                            static_cast<int>(InferenceError::InferenceFailed));
    }
    
    if (isCancelRequested()) {
        return InferenceResult::makeFailure(tr("Cancelled"), 
                                            static_cast<int>(InferenceError::Cancelled));
    }
    
    setProgress(0.85);
    
    QImage result = ImagePostprocessor::matToQimage(out, m_currentModel);
    
    setProgress(0.95);
    
    return InferenceResult::makeSuccess(result);
}

InferenceResult NCNNVulkanBackend::processTiled(const QImage& input, int tileSize)
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
            return InferenceResult::makeFailure(tr("Cancelled"), 
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
        
        int dstW = tileInfo.width * scale;
        int dstH = tileInfo.height * scale;
        
        if (croppedResult.width() != dstW || croppedResult.height() != dstH) {
            croppedResult = croppedResult.scaled(dstW, dstH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
            tr("Tile processing incomplete (%1/%2)").arg(successfulTiles).arg(totalTiles), 
            static_cast<int>(InferenceError::InferenceFailed));
    }
    
    setProgress(0.95);
    
    return InferenceResult::makeSuccess(output);
}

InferenceResult NCNNVulkanBackend::processWithTTA(const QImage& input)
{
    setProgress(0.02);
    
    auto transforms = ImagePreprocessor::generateTTATransforms(input);
    const int totalSteps = static_cast<int>(transforms.size());
    
    std::vector<QImage> results;
    
    for (int i = 0; i < totalSteps; ++i) {
        if (isCancelRequested()) {
            return InferenceResult::makeFailure(tr("Cancelled"), 
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
        return InferenceResult::makeFailure(tr("TTA processing failed"), 
                                            static_cast<int>(InferenceError::InferenceFailed));
    }
    
    setProgress(0.92);
    
    QImage merged = ImagePostprocessor::mergeTTAResults(results);
    
    setProgress(0.95);
    
    return InferenceResult::makeSuccess(merged);
}

ncnn::Mat NCNNVulkanBackend::runInference(const ncnn::Mat& input)
{
    if (input.empty() || input.w <= 0 || input.h <= 0) {
        qWarning() << "[NCNNVulkanBackend] Empty input mat";
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
    ex.set_light_mode(true);
    
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
        emitError(tr("Inference input failed, model input node mismatch"));
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
        emitError(tr("Inference output failed, model output node mismatch"));
        return ncnn::Mat();
    }
    
    return output;
}

void NCNNVulkanBackend::setProgress(double value)
{
    m_progress.store(value);
    emit progressChanged(value);
}

void NCNNVulkanBackend::resetCancelFlag()
{
    m_cancelRequested.store(false);
}

bool NCNNVulkanBackend::isCancelRequested() const
{
    return m_cancelRequested.load();
}

void NCNNVulkanBackend::setInferenceRunning(bool running)
{
    m_isInferenceRunning.store(running);
}

void NCNNVulkanBackend::emitError(const QString& error)
{
    qWarning() << "[NCNNVulkanBackend] Error:" << error;
}

} // namespace EnhanceVision
