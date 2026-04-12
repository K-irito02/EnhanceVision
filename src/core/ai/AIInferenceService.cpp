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
}

AIInferenceService::~AIInferenceService()
{
    shutdown();
}

bool AIInferenceService::initialize(ModelRegistry* modelRegistry, const AIServiceConfig& config)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        qWarning() << "[AIInferenceService] Already initialized";
        return true;
    }
    
    m_modelRegistry = modelRegistry;
    m_config = config;
    
    m_taskQueue = std::make_unique<AITaskQueue>(this);
    
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
    
    m_workerThread = std::make_unique<AIInferenceThread>(this);
    
    if (m_workerThread->worker()) {
        m_workerThread->worker()->setModelRegistry(m_modelRegistry);
        connectWorkerSignals();
    }
    
    m_workerThread->start();
    
    m_initialized = true;
    
    emit serviceInitialized();
    
    return true;
}

void AIInferenceService::shutdown(int waitMs)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    if (m_taskQueue) {
        m_taskQueue->cancelAll(AICancelReason::SystemShutdown);
        m_taskQueue->shutdown();
    }
    
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
    
    {
        QMutexLocker locker(&m_mutex);
        m_taskStates[request.taskId.toStdString()] = AITaskState::Queued;
    }
    
    bool success = m_taskQueue->enqueue(request);
    
    if (!success) {
        QMutexLocker locker(&m_mutex);
        m_taskStates.erase(request.taskId.toStdString());
    }
    
    return success;
}

bool AIInferenceService::cancelTask(const QString& taskId, AICancelReason reason)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_currentTaskId == taskId && m_workerThread && m_workerThread->worker()) {
            m_workerThread->worker()->requestCancel(reason);
            return true;
        }
    }
    
    return m_taskQueue ? m_taskQueue->cancelByTaskId(taskId, reason) : false;
}

int AIInferenceService::cancelMessageTasks(const QString& messageId, AICancelReason reason)
{
    int count = 0;
    
    {
        QMutexLocker locker(&m_mutex);
    }
    
    if (m_taskQueue) {
        count = m_taskQueue->cancelByMessageId(messageId, reason);
    }
    
    return count;
}

int AIInferenceService::cancelSessionTasks(const QString& sessionId, AICancelReason reason)
{
    int count = 0;
    
    if (m_taskQueue) {
        count = m_taskQueue->cancelBySessionId(sessionId, reason);
    }
    
    return count;
}

int AIInferenceService::cancelAllTasks(AICancelReason reason)
{
    int count = 0;
    
    {
        QMutexLocker locker(&m_mutex);
        if (!m_currentTaskId.isEmpty() && m_workerThread && m_workerThread->worker()) {
            m_workerThread->worker()->requestCancel(reason);
            count++;
        }
    }
    
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
    if (m_workerThread && m_workerThread->worker() && !isProcessing()) {
    }
}

void AIInferenceService::unloadModel()
{
    if (isProcessing()) {
        qWarning() << "[AIInferenceService] Cannot unload model while processing";
        return;
    }
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
    {
        QMutexLocker locker(&m_mutex);
        m_taskStates[taskId.toStdString()] = newState;
    }
    
    bool isNowProcessing = (newState == AITaskState::Preparing || newState == AITaskState::Processing);
    bool wasProcessing = m_isProcessing.exchange(isNowProcessing);
    
    if (wasProcessing != isNowProcessing) {
        emit processingChanged(isNowProcessing);
    }
    
    emit taskStateChanged(taskId, oldState, newState);
    
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
    
    m_isProcessing.store(false);
    m_currentProgress.store(0.0);
    
    emit processingChanged(false);
    emit currentTaskChanged(QString());
    emit progressChanged(0.0);
    
    if (result.success) {
        emit taskCompleted(result.taskId, result.outputPath);
    } else if (result.errorType == AIErrorType::Cancelled) {
        emit taskCancelled(result.taskId, AICancelReason::UserRequest);
    } else {
        emit taskFailed(result.taskId, result.errorMessage, result.errorType);
    }
    
    if (m_workerThread && m_workerThread->worker()) {
        m_workerThread->worker()->reset();
    }
    
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
    if (m_isProcessing.load()) {
        return;
    }
    
    if (!m_taskQueue || m_taskQueue->isEmpty()) {
        return;
    }
    
    if (!m_workerThread || !m_workerThread->worker() || !m_workerThread->isRunning()) {
        qWarning() << "[AIInferenceService] Worker thread not available";
        return;
    }
    
    auto taskOpt = m_taskQueue->dequeue(0);
    if (!taskOpt.has_value()) {
        return;
    }
    
    AITaskItem task = std::move(taskOpt.value());
    
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
