/**
 * @file AIInferenceService.cpp
 * @brief AI推理服务实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/ai/AIInferenceService.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>

namespace EnhanceVision {

// 静态成员初始化
AIInferenceService* AIInferenceService::s_instance = nullptr;
QMutex AIInferenceService::s_instanceMutex;

AIInferenceService* AIInferenceService::instance()
{
    QMutexLocker locker(&s_instanceMutex);
    
    if (!s_instance) {
        s_instance = new AIInferenceService();
    }
    
    return s_instance;
}

void AIInferenceService::destroyInstance()
{
    QMutexLocker locker(&s_instanceMutex);
    
    if (s_instance) {
        s_instance->shutdown();
        delete s_instance;
        s_instance = nullptr;
    }
}

AIInferenceService::AIInferenceService(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_modelRegistry(nullptr)
    , m_currentProgress(0.0)
    , m_isProcessing(false)
    , m_lastProgressEmitMs(0)
{
    qInfo() << "[AIInferenceService] Instance created";
}

AIInferenceService::~AIInferenceService()
{
    shutdown();
    qInfo() << "[AIInferenceService] Instance destroyed";
}

bool AIInferenceService::initialize(ModelRegistry* modelRegistry, const AIServiceConfig& config)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        qWarning() << "[AIInferenceService] Already initialized";
        return true;
    }
    
    qInfo() << "[AIInferenceService] Initializing service...";
    
    m_modelRegistry = modelRegistry;
    m_config = config;
    
    // 创建任务队列
    m_taskQueue = std::make_unique<AITaskQueue>(this);
    
    // 连接队列信号
    connect(m_taskQueue.get(), &AITaskQueue::taskEnqueued,
            this, [this](const QString& taskId) {
                emit taskSubmitted(taskId);
                processNextTask();
            });
    
    connect(m_taskQueue.get(), &AITaskQueue::taskCancelled,
            this, [this](const QString& taskId, AICancelReason reason) {
                emit taskCancelled(taskId, reason);
            });
    
    connect(m_taskQueue.get(), &AITaskQueue::taskStateChanged,
            this, [this](const QString& taskId, AITaskState oldState, AITaskState newState) {
                emit taskStateChanged(taskId, oldState, newState);
            });
    
    connect(m_taskQueue.get(), &AITaskQueue::queueSizeChanged,
            this, [this](int size) {
                emit queueSizeChanged(size);
            });
    
    // 创建并启动工作线程
    m_workerThread = std::make_unique<AIInferenceThread>(this);
    
    if (m_workerThread->worker()) {
        m_workerThread->worker()->setModelRegistry(m_modelRegistry);
        connectWorkerSignals();
    }
    
    m_workerThread->start();
    
    m_initialized = true;
    
    qInfo() << "[AIInferenceService] Service initialized successfully";
    emit serviceInitialized();
    
    return true;
}

void AIInferenceService::shutdown(int waitMs)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    qInfo() << "[AIInferenceService] Shutting down service...";
    
    // 取消所有任务
    if (m_taskQueue) {
        m_taskQueue->cancelAll(AICancelReason::SystemShutdown);
        m_taskQueue->shutdown();
    }
    
    // 停止工作线程
    if (m_workerThread) {
        disconnectWorkerSignals();
        m_workerThread->stop(waitMs);
    }
    
    m_initialized = false;
    m_isProcessing.store(false);
    m_currentProgress.store(0.0);
    m_currentTaskId.clear();
    m_taskStates.clear();
    
    locker.unlock();
    
    emit serviceShutdown();
    
    qInfo() << "[AIInferenceService] Service shutdown complete";
}

bool AIInferenceService::isInitialized() const
{
    QMutexLocker locker(&m_mutex);
    return m_initialized;
}

bool AIInferenceService::submitTask(const AITaskRequest& request)
{
    if (!m_initialized) {
        qWarning() << "[AIInferenceService] Cannot submit task: service not initialized";
        return false;
    }
    
    if (request.taskId.isEmpty()) {
        qWarning() << "[AIInferenceService] Cannot submit task: empty taskId";
        return false;
    }
    
    if (request.inputPath.isEmpty()) {
        qWarning() << "[AIInferenceService] Cannot submit task: empty inputPath";
        return false;
    }
    
    qInfo() << "[AIInferenceService] Submitting task:" << request.taskId
            << "input:" << request.inputPath
            << "model:" << request.modelId;
    
    // 记录任务状态
    {
        QMutexLocker locker(&m_mutex);
        m_taskStates[request.taskId.toStdString()] = AITaskState::Queued;
    }
    
    // 入队
    bool success = m_taskQueue->enqueue(request);
    
    if (success) {
        qInfo() << "[AIInferenceService] Task submitted successfully:" << request.taskId;
    } else {
        QMutexLocker locker(&m_mutex);
        m_taskStates.erase(request.taskId.toStdString());
    }
    
    return success;
}

bool AIInferenceService::cancelTask(const QString& taskId, AICancelReason reason)
{
    qInfo() << "[AIInferenceService] Cancelling task:" << taskId;
    
    // 检查是否是当前正在处理的任务
    {
        QMutexLocker locker(&m_mutex);
        if (m_currentTaskId == taskId && m_workerThread && m_workerThread->worker()) {
            m_workerThread->worker()->requestCancel(reason);
            return true;
        }
    }
    
    // 尝试从队列中取消
    return m_taskQueue ? m_taskQueue->cancelByTaskId(taskId, reason) : false;
}

int AIInferenceService::cancelMessageTasks(const QString& messageId, AICancelReason reason)
{
    qInfo() << "[AIInferenceService] Cancelling tasks for message:" << messageId;
    
    int count = 0;
    
    // 取消当前任务（如果属于该消息）
    {
        QMutexLocker locker(&m_mutex);
        // 需要从队列中查找当前任务的messageId
    }
    
    // 取消队列中的任务
    if (m_taskQueue) {
        count = m_taskQueue->cancelByMessageId(messageId, reason);
    }
    
    return count;
}

int AIInferenceService::cancelSessionTasks(const QString& sessionId, AICancelReason reason)
{
    qInfo() << "[AIInferenceService] Cancelling tasks for session:" << sessionId;
    
    int count = 0;
    
    if (m_taskQueue) {
        count = m_taskQueue->cancelBySessionId(sessionId, reason);
    }
    
    return count;
}

int AIInferenceService::cancelAllTasks(AICancelReason reason)
{
    qInfo() << "[AIInferenceService] Cancelling all tasks";
    
    int count = 0;
    
    // 取消当前任务
    {
        QMutexLocker locker(&m_mutex);
        if (!m_currentTaskId.isEmpty() && m_workerThread && m_workerThread->worker()) {
            m_workerThread->worker()->requestCancel(reason);
            count++;
        }
    }
    
    // 取消队列中的任务
    if (m_taskQueue) {
        count += m_taskQueue->cancelAll(reason);
    }
    
    return count;
}

void AIInferenceService::forceStopAll()
{
    qWarning() << "[AIInferenceService] Force stopping all tasks";
    
    // 强制取消当前任务
    {
        QMutexLocker locker(&m_mutex);
        if (m_workerThread && m_workerThread->worker()) {
            m_workerThread->worker()->forceCancel();
        }
    }
    
    // 清空队列
    if (m_taskQueue) {
        m_taskQueue->cancelAll(AICancelReason::UserRequest);
    }
    
    // 重置状态
    m_isProcessing.store(false);
    m_currentProgress.store(0.0);
    
    {
        QMutexLocker locker(&m_mutex);
        m_currentTaskId.clear();
        m_taskStates.clear();
    }
    
    emit processingChanged(false);
    emit currentTaskChanged(QString());
}

bool AIInferenceService::isProcessing() const
{
    return m_isProcessing.load();
}

int AIInferenceService::queueSize() const
{
    return m_taskQueue ? m_taskQueue->size() : 0;
}

QString AIInferenceService::currentTaskId() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentTaskId;
}

double AIInferenceService::currentProgress() const
{
    return m_currentProgress.load();
}

AITaskState AIInferenceService::getTaskState(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_taskStates.find(taskId.toStdString());
    if (it != m_taskStates.end()) {
        return it->second;
    }
    
    // 检查队列
    if (m_taskQueue) {
        auto state = m_taskQueue->getTaskState(taskId);
        if (state.has_value()) {
            return state.value();
        }
    }
    
    return AITaskState::Idle;
}

bool AIInferenceService::hasTask(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentTaskId == taskId) {
        return true;
    }
    
    if (m_taskStates.find(taskId.toStdString()) != m_taskStates.end()) {
        return true;
    }
    
    return m_taskQueue ? m_taskQueue->contains(taskId) : false;
}

int AIInferenceService::pendingTasksForMessage(const QString& messageId) const
{
    return m_taskQueue ? m_taskQueue->pendingCountForMessage(messageId) : 0;
}

void AIInferenceService::preloadModel(const QString& modelId)
{
    qInfo() << "[AIInferenceService] Preloading model:" << modelId;
    
    // 在工作线程中预加载模型
    if (m_workerThread && m_workerThread->worker() && !isProcessing()) {
        // TODO: 实现异步模型预加载
    }
}

void AIInferenceService::unloadModel()
{
    qInfo() << "[AIInferenceService] Unloading model";
    
    if (isProcessing()) {
        qWarning() << "[AIInferenceService] Cannot unload model while processing";
        return;
    }
    
    // TODO: 实现模型卸载
}

QString AIInferenceService::currentModelId() const
{
    if (m_workerThread && m_workerThread->worker()) {
        // TODO: 获取当前模型ID
    }
    return QString();
}

ModelRegistry* AIInferenceService::modelRegistry() const
{
    return m_modelRegistry;
}

void AIInferenceService::onWorkerStateChanged(const QString& taskId, AITaskState oldState, AITaskState newState)
{
    qInfo() << "[AIInferenceService] Worker state changed:" << taskId
            << aiTaskStateToString(oldState) << "->" << aiTaskStateToString(newState);
    
    // 更新任务状态
    {
        QMutexLocker locker(&m_mutex);
        m_taskStates[taskId.toStdString()] = newState;
    }
    
    // 更新处理状态
    bool isNowProcessing = (newState == AITaskState::Preparing || newState == AITaskState::Processing);
    bool wasProcessing = m_isProcessing.exchange(isNowProcessing);
    
    if (wasProcessing != isNowProcessing) {
        emit processingChanged(isNowProcessing);
    }
    
    // 发出状态变化信号
    emit taskStateChanged(taskId, oldState, newState);
    
    // 任务开始信号
    if (oldState == AITaskState::Queued && newState == AITaskState::Preparing) {
        emit taskStarted(taskId);
    }
}

void AIInferenceService::onWorkerProgressChanged(const AITaskProgress& progress)
{
    // 进度节流
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_config.enableProgressThrottle && 
        (nowMs - m_lastProgressEmitMs) < m_config.progressThrottleMs &&
        progress.progress < 0.99) {
        return;
    }
    m_lastProgressEmitMs = nowMs;
    
    // 更新进度
    m_currentProgress.store(progress.progress);
    
    emit progressChanged(progress.progress);
    emit taskProgress(progress.taskId, progress.progress, progress.stage);
}

void AIInferenceService::onWorkerTaskCompleted(const AITaskResult& result)
{
    qInfo() << "[AIInferenceService] Worker task completed:" << result.taskId
            << "success:" << result.success
            << "elapsed:" << result.processingTimeMs << "ms";
    
    // 更新状态
    {
        QMutexLocker locker(&m_mutex);
        m_currentTaskId.clear();
        
        if (result.success) {
            m_taskStates[result.taskId.toStdString()] = AITaskState::Completed;
        } else if (result.errorType == AIErrorType::Cancelled) {
            m_taskStates[result.taskId.toStdString()] = AITaskState::Cancelled;
        } else {
            m_taskStates[result.taskId.toStdString()] = AITaskState::Failed;
        }
    }
    
    // 更新处理状态
    m_isProcessing.store(false);
    m_currentProgress.store(0.0);
    
    emit processingChanged(false);
    emit currentTaskChanged(QString());
    emit progressChanged(0.0);
    
    // 发出完成信号
    if (result.success) {
        emit taskCompleted(result.taskId, result.outputPath);
    } else if (result.errorType == AIErrorType::Cancelled) {
        emit taskCancelled(result.taskId, AICancelReason::UserRequest);
    } else {
        emit taskFailed(result.taskId, result.errorMessage, result.errorType);
    }
    
    // 重置worker状态
    if (m_workerThread && m_workerThread->worker()) {
        m_workerThread->worker()->reset();
    }
    
    // 处理下一个任务
    QMetaObject::invokeMethod(this, "processNextTask", Qt::QueuedConnection);
}

void AIInferenceService::onWorkerError(const QString& taskId, const QString& error, AIErrorType type)
{
    qWarning() << "[AIInferenceService] Worker error:" << taskId
               << "error:" << error
               << "type:" << aiErrorTypeToString(type);
    
    emit errorOccurred(error);
}

void AIInferenceService::processNextTask()
{
    // 检查是否正在处理
    if (m_isProcessing.load()) {
        qInfo() << "[AIInferenceService] Already processing, skipping processNextTask";
        return;
    }
    
    // 检查队列
    if (!m_taskQueue || m_taskQueue->isEmpty()) {
        qInfo() << "[AIInferenceService] No tasks in queue";
        return;
    }
    
    // 检查worker
    if (!m_workerThread || !m_workerThread->worker() || !m_workerThread->isRunning()) {
        qWarning() << "[AIInferenceService] Worker thread not available";
        return;
    }
    
    // 取出下一个任务
    auto taskOpt = m_taskQueue->dequeue(0);  // 非阻塞
    if (!taskOpt.has_value()) {
        qInfo() << "[AIInferenceService] No available task to process";
        return;
    }
    
    AITaskItem task = std::move(taskOpt.value());
    
    // 更新状态
    {
        QMutexLocker locker(&m_mutex);
        m_currentTaskId = task.request.taskId;
        m_taskStates[task.request.taskId.toStdString()] = AITaskState::Preparing;
    }
    
    m_isProcessing.store(true);
    m_currentProgress.store(0.0);
    
    emit processingChanged(true);
    emit currentTaskChanged(task.request.taskId);
    emit taskStarted(task.request.taskId);
    
    qInfo() << "[AIInferenceService] Starting task:" << task.request.taskId;
    
    // 在工作线程中启动任务
    QMetaObject::invokeMethod(m_workerThread->worker(), "startTask",
                              Qt::QueuedConnection,
                              Q_ARG(EnhanceVision::AITaskRequest, task.request));
}

void AIInferenceService::startWorkerThread()
{
    if (m_workerThread && !m_workerThread->isRunning()) {
        m_workerThread->start();
    }
}

void AIInferenceService::stopWorkerThread()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->stop();
    }
}

void AIInferenceService::connectWorkerSignals()
{
    if (!m_workerThread || !m_workerThread->worker()) {
        return;
    }
    
    AIInferenceWorker* worker = m_workerThread->worker();
    
    connect(worker, &AIInferenceWorker::stateChanged,
            this, &AIInferenceService::onWorkerStateChanged,
            Qt::QueuedConnection);
    
    connect(worker, &AIInferenceWorker::progressChanged,
            this, &AIInferenceService::onWorkerProgressChanged,
            Qt::QueuedConnection);
    
    connect(worker, &AIInferenceWorker::taskCompleted,
            this, &AIInferenceService::onWorkerTaskCompleted,
            Qt::QueuedConnection);
    
    connect(worker, &AIInferenceWorker::errorOccurred,
            this, &AIInferenceService::onWorkerError,
            Qt::QueuedConnection);
}

void AIInferenceService::disconnectWorkerSignals()
{
    if (!m_workerThread || !m_workerThread->worker()) {
        return;
    }
    
    AIInferenceWorker* worker = m_workerThread->worker();
    
    disconnect(worker, &AIInferenceWorker::stateChanged,
               this, &AIInferenceService::onWorkerStateChanged);
    
    disconnect(worker, &AIInferenceWorker::progressChanged,
               this, &AIInferenceService::onWorkerProgressChanged);
    
    disconnect(worker, &AIInferenceWorker::taskCompleted,
               this, &AIInferenceService::onWorkerTaskCompleted);
    
    disconnect(worker, &AIInferenceWorker::errorOccurred,
               this, &AIInferenceService::onWorkerError);
}

} // namespace EnhanceVision
