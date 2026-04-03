/**
 * @file AIInferenceService.h
 * @brief AI 推理服务门面（重构版）
 * @author EnhanceVision Team
 * 
 * 统一的 AI 推理服务入口点。
 * 作为服务门面，协调调度层、处理层和推理层。
 */

#ifndef ENHANCEVISION_AIINFERENCESERVICE_V2_H
#define ENHANCEVISION_AIINFERENCESERVICE_V2_H

#include "EnhanceVision/core/scheduling/AITaskTypes.h"
#include "EnhanceVision/core/scheduling/AITaskQueue.h"
#include "EnhanceVision/core/scheduling/AITaskExecutor.h"
#include "EnhanceVision/core/inference/IInferenceBackend.h"
#include <QObject>
#include <QMutex>
#include <memory>
#include <functional>

namespace EnhanceVision {

class ModelRegistry;

/**
 * @brief 服务配置
 */
struct AIServiceConfig {
    int maxQueueSize = 1000;
    int taskTimeoutMs = 3600000;
    bool autoRetryOnGpuOom = true;
    int maxRetryCount = 3;
    bool enableProgressThrottle = true;
    int progressThrottleMs = 50;
};

/**
 * @brief AI 推理服务（单例）
 * 
 * 作为服务门面，提供：
 * - 统一入口点
 * - 状态查询
 * - 信号转发
 * - 配置管理
 * 
 * 使用示例：
 * @code
 * auto* service = AIInferenceService::instance();
 * service->initialize(modelRegistry);
 * 
 * AITaskRequest request;
 * request.taskId = QUuid::createUuid().toString();
 * request.inputPath = "/path/to/image.jpg";
 * request.outputPath = "/path/to/output.png";
 * request.modelId = "realesrgan-x4plus";
 * 
 * service->submitTask(request);
 * @endcode
 */
class AIInferenceService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(int queueSize READ queueSize NOTIFY queueSizeChanged)
    Q_PROPERTY(QString currentTaskId READ currentTaskId NOTIFY currentTaskChanged)
    Q_PROPERTY(double currentProgress READ currentProgress NOTIFY progressChanged)

public:
    static AIInferenceService* instance();
    static void destroyInstance();
    
    /**
     * @brief 初始化服务
     */
    bool initialize(ModelRegistry* modelRegistry, const AIServiceConfig& config = AIServiceConfig());
    
    /**
     * @brief 关闭服务
     */
    void shutdown(int waitMs = 5000);
    
    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const;
    
    // ========== 任务管理 ==========
    
    Q_INVOKABLE bool submitTask(const AITaskRequest& request);
    Q_INVOKABLE bool cancelTask(const QString& taskId, AICancelReason reason = AICancelReason::UserRequest);
    Q_INVOKABLE int cancelMessageTasks(const QString& messageId, AICancelReason reason = AICancelReason::UserRequest);
    Q_INVOKABLE int cancelSessionTasks(const QString& sessionId, AICancelReason reason = AICancelReason::UserRequest);
    Q_INVOKABLE int cancelAllTasks(AICancelReason reason = AICancelReason::UserRequest);
    Q_INVOKABLE void forceStopAll();
    
    // ========== 状态查询 ==========
    
    bool isProcessing() const;
    int queueSize() const;
    QString currentTaskId() const;
    double currentProgress() const;
    Q_INVOKABLE AITaskState getTaskState(const QString& taskId) const;
    Q_INVOKABLE bool hasTask(const QString& taskId) const;
    Q_INVOKABLE int pendingTasksForMessage(const QString& messageId) const;
    
    // ========== 模型管理 ==========
    
    Q_INVOKABLE void preloadModel(const QString& modelId);
    Q_INVOKABLE void unloadModel();
    Q_INVOKABLE QString currentModelId() const;
    ModelRegistry* modelRegistry() const;

signals:
    void processingChanged(bool processing);
    void queueSizeChanged(int size);
    void currentTaskChanged(const QString& taskId);
    void progressChanged(double progress);
    
    void taskSubmitted(const QString& taskId);
    void taskStarted(const QString& taskId);
    void taskProgress(const QString& taskId, double progress, const QString& stage);
    void taskCompleted(const QString& taskId, const QString& outputPath);
    void taskFailed(const QString& taskId, const QString& error, AIErrorType errorType);
    void taskCancelled(const QString& taskId, AICancelReason reason);
    void taskStateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    
    void serviceInitialized();
    void serviceShutdown();
    void errorOccurred(const QString& error);

private slots:
    void onExecutorStateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    void onExecutorProgressChanged(const AITaskProgress& progress);
    void onExecutorTaskCompleted(const AITaskResult& result);
    void onExecutorError(const QString& taskId, const QString& error, AIErrorType type);
    void processNextTask();

private:
    explicit AIInferenceService(QObject* parent = nullptr);
    ~AIInferenceService() override;
    
    AIInferenceService(const AIInferenceService&) = delete;
    AIInferenceService& operator=(const AIInferenceService&) = delete;
    
    void startWorkerThread();
    void stopWorkerThread();
    void connectExecutorSignals();
    void disconnectExecutorSignals();
    
    static AIInferenceService* s_instance;
    static QMutex s_instanceMutex;
    
    bool m_initialized = false;
    AIServiceConfig m_config;
    ModelRegistry* m_modelRegistry = nullptr;
    
    std::unique_ptr<AITaskQueue> m_taskQueue;
    std::unique_ptr<AIWorkerThread> m_workerThread;
    
    QString m_currentTaskId;
    std::atomic<double> m_currentProgress{0.0};
    std::atomic<bool> m_isProcessing{false};
    
    mutable QMutex m_mutex;
    qint64 m_lastProgressEmitMs = 0;
    
    std::unordered_map<std::string, AITaskState> m_taskStates;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIINFERENCESERVICE_V2_H
