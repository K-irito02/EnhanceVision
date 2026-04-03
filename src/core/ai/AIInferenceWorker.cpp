/**
 * @file AIInferenceWorker.cpp
 * @brief AI推理工作器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/ai/AIInferenceWorker.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include "EnhanceVision/core/video/AIVideoProcessor.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QDebug>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

namespace EnhanceVision {

// ========== AIInferenceWorker ==========

AIInferenceWorker::AIInferenceWorker(QObject* parent)
    : QObject(parent)
    , m_modelRegistry(nullptr)
    , m_state(AITaskState::Idle)
    , m_progress(0.0)
    , m_cancelRequested(false)
    , m_forceCancel(false)
    , m_cancelReason(AICancelReason::None)
{
    // 创建引擎实例
    m_engine = std::make_unique<AIEngine>(nullptr);
}

AIInferenceWorker::~AIInferenceWorker()
{
    forceCancel();
    
    waitForCompletion(2000);
    
    m_videoProcessor.reset();
    m_engine.reset();
}

void AIInferenceWorker::setModelRegistry(ModelRegistry* registry)
{
    QMutexLocker locker(&m_mutex);
    m_modelRegistry = registry;
    if (m_engine) {
        m_engine->setModelRegistry(registry);
    }
}

bool AIInferenceWorker::isIdle() const
{
    return m_state.load() == AITaskState::Idle;
}

bool AIInferenceWorker::isProcessing() const
{
    AITaskState state = m_state.load();
    return state == AITaskState::Preparing || state == AITaskState::Processing;
}

QString AIInferenceWorker::currentTaskId() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentTaskId;
}

AITaskState AIInferenceWorker::currentState() const
{
    return m_state.load();
}

double AIInferenceWorker::currentProgress() const
{
    return m_progress.load();
}

void AIInferenceWorker::requestCancel(AICancelReason reason)
{
    qInfo() << "[AIInferenceWorker] Cancel requested for task:" << m_currentTaskId
            << "reason:" << static_cast<int>(reason);
    
    m_cancelReason = reason;
    m_cancelRequested.store(true);

    if (m_videoProcessor) {
        m_videoProcessor->cancel();
    }
}

void AIInferenceWorker::forceCancel()
{
    qInfo() << "[AIInferenceWorker] Force cancel requested for task:" << m_currentTaskId;
    
    m_forceCancel.store(true);
    m_cancelRequested.store(true);
    m_cancelReason = AICancelReason::UserRequest;
    
    if (m_engine) {
        m_engine->forceCancel();
    }
    
    if (m_videoProcessor) {
        m_videoProcessor->cancel();
    }
}

bool AIInferenceWorker::isCancelRequested() const
{
    return m_cancelRequested.load();
}

void AIInferenceWorker::reset()
{
    QMutexLocker locker(&m_mutex);
    
    m_currentTaskId.clear();
    m_state.store(AITaskState::Idle);
    m_progress.store(0.0);
    m_cancelRequested.store(false);
    m_forceCancel.store(false);
    m_cancelReason = AICancelReason::None;
    
    // 重置引擎状态
    if (m_engine) {
        m_engine->resetState();
    }
    
    // 通知等待的线程
    m_completionCondition.wakeAll();
    
    qInfo() << "[AIInferenceWorker] Worker reset";
}

bool AIInferenceWorker::waitForCompletion(int timeoutMs)
{
    QMutexLocker locker(&m_completionMutex);
    
    if (!isProcessing()) {
        return true;
    }
    
    return m_completionCondition.wait(&m_completionMutex, timeoutMs);
}

void AIInferenceWorker::startTask(const AITaskRequest& request)
{
    qInfo() << "[AIInferenceWorker] Starting task:" << request.taskId
            << "input:" << request.inputPath
            << "output:" << request.outputPath
            << "model:" << request.modelId;
    
    // 设置当前任务
    {
        QMutexLocker locker(&m_mutex);
        m_currentTaskId = request.taskId;
        m_cancelRequested.store(false);
        m_forceCancel.store(false);
        m_cancelReason = AICancelReason::None;
    }
    
    // 启动计时器
    m_taskTimer.start();
    
    // 设置状态为准备中
    setState(AITaskState::Preparing);
    updateProgress(0.0, tr("准备中..."));
    
    // 检查取消
    if (checkCancellation()) {
        return;
    }
    
    // 执行处理
    AITaskResult result;
    
    if (request.mediaType == AIMediaType::Video) {
        result = processVideo(request);
    } else {
        result = processImage(request);
    }
    
    // 通知完成
    {
        QMutexLocker locker(&m_completionMutex);
        m_completionCondition.wakeAll();
    }
    
    emit taskCompleted(result);
}

void AIInferenceWorker::setState(AITaskState newState)
{
    AITaskState oldState = m_state.exchange(newState);
    
    if (oldState != newState) {
        qInfo() << "[AIInferenceWorker] State changed:" 
                << aiTaskStateToString(oldState) << "->" << aiTaskStateToString(newState)
                << "task:" << m_currentTaskId;
        
        emit stateChanged(m_currentTaskId, oldState, newState);
    }
}

void AIInferenceWorker::updateProgress(double progress, const QString& stage)
{
    m_progress.store(progress);
    
    AITaskProgress progressInfo;
    progressInfo.taskId = m_currentTaskId;
    progressInfo.progress = progress;
    progressInfo.stage = stage;
    progressInfo.elapsedMs = m_taskTimer.elapsed();
    
    // 估算剩余时间
    if (progress > 0.01 && progress < 0.99) {
        double estimatedTotal = progressInfo.elapsedMs / progress;
        progressInfo.estimatedRemainingMs = static_cast<qint64>(estimatedTotal - progressInfo.elapsedMs);
    }
    
    emit progressChanged(progressInfo);
}

AITaskResult AIInferenceWorker::processImage(const AITaskRequest& request)
{
    qInfo() << "[AIInferenceWorker] Processing image:" << request.inputPath;
    
    // 检查取消
    if (checkCancellation()) {
        return AITaskResult::makeCancelled(request.taskId);
    }
    
    // 加载输入图像
    updateProgress(0.05, tr("加载图像..."));
    
    QImage inputImage(request.inputPath);
    if (inputImage.isNull()) {
        QString error = tr("无法读取图像: %1").arg(request.inputPath);
        qWarning() << "[AIInferenceWorker]" << error;
        setState(AITaskState::Failed);
        return AITaskResult::makeFailure(request.taskId, error, AIErrorType::FileReadError);
    }
    
    // 检查取消
    if (checkCancellation()) {
        return AITaskResult::makeCancelled(request.taskId);
    }
    
    // 加载模型
    updateProgress(0.10, tr("加载模型..."));
    
    QString modelError;
    if (!ensureModelLoaded(request.modelId, modelError)) {
        setState(AITaskState::Failed);
        return AITaskResult::makeFailure(request.taskId, modelError, AIErrorType::ModelLoadFailed);
    }
    
    // 检查取消
    if (checkCancellation()) {
        return AITaskResult::makeCancelled(request.taskId);
    }
    
    // 设置模型参数
    if (m_engine) {
        m_engine->clearParameters();
        for (auto it = request.params.begin(); it != request.params.end(); ++it) {
            m_engine->setParameter(it.key(), it.value());
        }
    }
    
    // 开始处理
    setState(AITaskState::Processing);
    updateProgress(0.15, tr("AI推理中..."));
    
    // 连接进度信号
    QMetaObject::Connection progressConn;
    if (m_engine) {
        progressConn = connect(m_engine.get(), &AIEngine::progressChanged, 
            this, [this](double p) {
                // 将引擎进度映射到 0.15 - 0.90 范围
                double mappedProgress = 0.15 + p * 0.75;
                updateProgress(mappedProgress, tr("AI推理中..."));
            }, Qt::DirectConnection);
    }
    
    // 执行推理
    QImage resultImage = m_engine ? m_engine->process(inputImage) : QImage();
    
    // 断开进度连接
    if (progressConn) {
        disconnect(progressConn);
    }
    
    // 检查取消
    if (checkCancellation()) {
        return AITaskResult::makeCancelled(request.taskId);
    }
    
    // 检查结果
    if (resultImage.isNull()) {
        QString error = tr("AI推理失败，请检查模型兼容性");
        
        // 检查是否是GPU OOM
        if (m_engine && m_engine->isForceCancelled()) {
            setState(AITaskState::Cancelled);
            return AITaskResult::makeCancelled(request.taskId);
        }
        
        qWarning() << "[AIInferenceWorker]" << error;
        setState(AITaskState::Failed);
        return AITaskResult::makeFailure(request.taskId, error, AIErrorType::InferenceFailed);
    }
    
    // 保存结果
    updateProgress(0.92, tr("保存结果..."));
    
    // 确保输出目录存在
    QFileInfo outputInfo(request.outputPath);
    QDir().mkpath(outputInfo.absolutePath());
    
    if (!resultImage.save(request.outputPath)) {
        QString error = tr("无法保存结果: %1").arg(request.outputPath);
        qWarning() << "[AIInferenceWorker]" << error;
        setState(AITaskState::Failed);
        return AITaskResult::makeFailure(request.taskId, error, AIErrorType::FileWriteError);
    }
    
    // 完成
    updateProgress(1.0, tr("完成"));
    setState(AITaskState::Completed);
    
    qint64 elapsedMs = m_taskTimer.elapsed();
    qInfo() << "[AIInferenceWorker] Image processing completed:" << request.taskId
            << "elapsed:" << elapsedMs << "ms";
    
    return AITaskResult::makeSuccess(request.taskId, request.outputPath, elapsedMs);
}

AITaskResult AIInferenceWorker::processVideo(const AITaskRequest& request)
{
    qInfo() << "[AIInferenceWorker] Processing video:" << request.inputPath;
    
    // 检查取消
    if (checkCancellation()) {
        return AITaskResult::makeCancelled(request.taskId);
    }
    
    // 加载模型
    updateProgress(0.05, tr("加载模型..."));
    
    QString modelError;
    if (!ensureModelLoaded(request.modelId, modelError)) {
        setState(AITaskState::Failed);
        return AITaskResult::makeFailure(request.taskId, modelError, AIErrorType::ModelLoadFailed);
    }
    
    // 检查取消
    if (checkCancellation()) {
        return AITaskResult::makeCancelled(request.taskId);
    }
    
    // 设置模型参数
    if (m_engine) {
        m_engine->clearParameters();
        for (auto it = request.params.begin(); it != request.params.end(); ++it) {
            m_engine->setParameter(it.key(), it.value());
        }
    }
    
    // 创建视频处理器
    if (!m_videoProcessor) {
        m_videoProcessor = std::make_unique<AIVideoProcessor>(nullptr);
    }
    
    // 配置视频处理器
    m_videoProcessor->setAIEngine(m_engine.get());
    
    if (m_modelRegistry && m_modelRegistry->hasModel(request.modelId)) {
        ModelInfo modelInfo = m_modelRegistry->getModelInfo(request.modelId);
        m_videoProcessor->setModelInfo(modelInfo);
    }
    
    // 开始处理
    setState(AITaskState::Processing);
    updateProgress(0.10, tr("处理视频中..."));
    
    // 创建结果跟踪变量
    bool videoSuccess = false;
    QString videoError;
    QString videoOutputPath;
    
    // 连接信号
    QMetaObject::Connection progressConn = connect(m_videoProcessor.get(), &AIVideoProcessor::progressChanged,
        this, [this](double p) {
            double mappedProgress = 0.10 + p * 0.85;
            updateProgress(mappedProgress, tr("处理视频中..."));
        }, Qt::DirectConnection);
    
    QMetaObject::Connection completedConn = connect(m_videoProcessor.get(), &AIVideoProcessor::completed,
        this, [&videoSuccess, &videoOutputPath, &videoError](bool success, const QString& resultPath, const QString& error) {
            videoSuccess = success;
            videoOutputPath = resultPath;
            videoError = error;
        }, Qt::DirectConnection);
    
    // 启动异步处理
    m_videoProcessor->processAsync(request.inputPath, request.outputPath);
    
    // 等待完成
    m_videoProcessor->waitForFinished(3600000); // 1小时超时
    
    // 断开信号
    disconnect(progressConn);
    disconnect(completedConn);
    
    // 检查取消
    if (checkCancellation()) {
        return AITaskResult::makeCancelled(request.taskId);
    }
    
    // 检查结果
    if (!videoSuccess) {
        qWarning() << "[AIInferenceWorker] Video processing failed:" << videoError;
        setState(AITaskState::Failed);
        return AITaskResult::makeFailure(request.taskId, 
            videoError.isEmpty() ? tr("视频处理失败") : videoError, 
            AIErrorType::InferenceFailed);
    }
    
    // 完成
    updateProgress(1.0, tr("完成"));
    setState(AITaskState::Completed);
    
    qint64 elapsedMs = m_taskTimer.elapsed();
    qInfo() << "[AIInferenceWorker] Video processing completed:" << request.taskId
            << "elapsed:" << elapsedMs << "ms";
    
    return AITaskResult::makeSuccess(request.taskId, videoOutputPath.isEmpty() ? request.outputPath : videoOutputPath, elapsedMs);
}

bool AIInferenceWorker::ensureModelLoaded(const QString& modelId, QString& errorOut)
{
    if (!m_engine) {
        errorOut = tr("AI引擎未初始化");
        return false;
    }
    
    if (!m_modelRegistry) {
        errorOut = tr("模型注册表未初始化");
        return false;
    }
    
    // 检查模型是否已加载
    if (m_engine->currentModelId() == modelId) {
        return true;
    }
    
    // 加载模型
    qInfo() << "[AIInferenceWorker] Loading model:" << modelId;
    
    if (!m_engine->loadModel(modelId)) {
        errorOut = tr("模型加载失败: %1").arg(modelId);
        qWarning() << "[AIInferenceWorker]" << errorOut;
        return false;
    }
    
    qInfo() << "[AIInferenceWorker] Model loaded successfully:" << modelId;
    return true;
}

void AIInferenceWorker::cleanupResources()
{
    qInfo() << "[AIInferenceWorker] Cleaning up resources";
    
    if (m_engine) {
        m_engine->safeCleanup();
    }
}

bool AIInferenceWorker::checkCancellation()
{
    if (m_cancelRequested.load() || m_forceCancel.load()) {
        qInfo() << "[AIInferenceWorker] Cancellation detected for task:" << m_currentTaskId;
        setState(AITaskState::Cancelled);
        updateProgress(m_progress.load(), tr("已取消"));
        
        cleanupResources();
        
        return true;
    }
    return false;
}

// ========== AIInferenceThread ==========

AIInferenceThread::AIInferenceThread(QObject* parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_worker(new AIInferenceWorker())
{
    m_worker->moveToThread(m_thread);
    
    // 线程启动/停止信号
    connect(m_thread, &QThread::started, this, &AIInferenceThread::started);
    connect(m_thread, &QThread::finished, this, &AIInferenceThread::stopped);
    
    // 线程结束时清理worker
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
}

AIInferenceThread::~AIInferenceThread()
{
    stop();
}

void AIInferenceThread::start()
{
    if (!m_thread->isRunning()) {
        qInfo() << "[AIInferenceThread] Starting worker thread";
        m_thread->start();
    }
}

void AIInferenceThread::stop(int waitMs)
{
    if (m_thread->isRunning()) {
        qInfo() << "[AIInferenceThread] Stopping worker thread";
        
        // 请求取消当前任务
        if (m_worker) {
            m_worker->forceCancel();
        }
        
        // 请求线程退出
        m_thread->quit();
        
        // 等待线程结束
        if (!m_thread->wait(waitMs)) {
            qWarning() << "[AIInferenceThread] Thread did not stop in time, terminating";
            m_thread->terminate();
            m_thread->wait();
        }
        
        qInfo() << "[AIInferenceThread] Worker thread stopped";
    }
}

AIInferenceWorker* AIInferenceThread::worker() const
{
    return m_worker;
}

bool AIInferenceThread::isRunning() const
{
    return m_thread->isRunning();
}

} // namespace EnhanceVision
