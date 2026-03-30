/**
 * @file AIInferenceService.h
 * @brief AI推理服务（单例）
 * @author EnhanceVision Team
 * 
 * 统一的AI推理服务入口，管理：
 * - 任务队列
 * - 工作线程
 * - 资源生命周期
 * - 状态同步
 * 
 * 核心设计原则：
 * 1. 严格单任务串行：同一时间只处理一个媒体文件
 * 2. 线程安全：所有公开方法线程安全
 * 3. 状态一致性：使用状态机保证状态转换正确
 * 4. 资源安全：自动管理GPU资源生命周期
 */

#ifndef ENHANCEVISION_AIINFERENCESERVICE_H
#define ENHANCEVISION_AIINFERENCESERVICE_H

#include "AIInferenceTypes.h"
#include "AITaskQueue.h"
#include "AIInferenceWorker.h"
#include <QObject>
#include <QMutex>
#include <memory>
#include <functional>

namespace EnhanceVision {

class ModelRegistry;

/**
 * @brief AI推理服务配置
 */
struct AIServiceConfig {
    int maxQueueSize = 1000;            ///< 最大队列大小
    int taskTimeoutMs = 3600000;        ///< 任务超时（默认1小时）
    bool autoRetryOnGpuOom = true;      ///< GPU OOM时自动重试
    int maxRetryCount = 3;              ///< 最大重试次数
    bool enableProgressThrottle = true; ///< 启用进度节流
    int progressThrottleMs = 50;        ///< 进度更新最小间隔
};

/**
 * @brief AI推理服务（单例）
 * 
 * 使用方法：
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
    /**
     * @brief 获取单例实例
     */
    static AIInferenceService* instance();
    
    /**
     * @brief 销毁单例（应用退出时调用）
     */
    static void destroyInstance();
    
    /**
     * @brief 初始化服务
     * @param modelRegistry 模型注册表
     * @param config 服务配置
     * @return true 如果初始化成功
     */
    bool initialize(ModelRegistry* modelRegistry, const AIServiceConfig& config = AIServiceConfig());
    
    /**
     * @brief 关闭服务
     * @param waitMs 等待时间
     */
    void shutdown(int waitMs = 5000);
    
    /**
     * @brief 检查服务是否已初始化
     */
    bool isInitialized() const;
    
    // ========== 任务管理 ==========
    
    /**
     * @brief 提交任务
     * @param request 任务请求
     * @return true 如果成功入队
     */
    Q_INVOKABLE bool submitTask(const AITaskRequest& request);
    
    /**
     * @brief 取消指定任务
     * @param taskId 任务ID
     * @param reason 取消原因
     * @return true 如果找到并取消了任务
     */
    Q_INVOKABLE bool cancelTask(const QString& taskId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 取消指定消息的所有任务
     * @param messageId 消息ID
     * @param reason 取消原因
     * @return 取消的任务数量
     */
    Q_INVOKABLE int cancelMessageTasks(const QString& messageId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 取消指定会话的所有任务
     * @param sessionId 会话ID
     * @param reason 取消原因
     * @return 取消的任务数量
     */
    Q_INVOKABLE int cancelSessionTasks(const QString& sessionId, AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 取消所有任务
     * @param reason 取消原因
     * @return 取消的任务数量
     */
    Q_INVOKABLE int cancelAllTasks(AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 强制停止当前任务并清空队列
     */
    Q_INVOKABLE void forceStopAll();
    
    // ========== 状态查询 ==========
    
    /**
     * @brief 检查是否有任务在处理
     */
    bool isProcessing() const;
    
    /**
     * @brief 获取队列大小
     */
    int queueSize() const;
    
    /**
     * @brief 获取当前处理的任务ID
     */
    QString currentTaskId() const;
    
    /**
     * @brief 获取当前进度
     */
    double currentProgress() const;
    
    /**
     * @brief 获取任务状态
     */
    Q_INVOKABLE AITaskState getTaskState(const QString& taskId) const;
    
    /**
     * @brief 检查任务是否存在
     */
    Q_INVOKABLE bool hasTask(const QString& taskId) const;
    
    /**
     * @brief 获取消息的待处理任务数
     */
    Q_INVOKABLE int pendingTasksForMessage(const QString& messageId) const;
    
    // ========== 模型管理 ==========
    
    /**
     * @brief 预加载模型
     * @param modelId 模型ID
     */
    Q_INVOKABLE void preloadModel(const QString& modelId);
    
    /**
     * @brief 卸载当前模型
     */
    Q_INVOKABLE void unloadModel();
    
    /**
     * @brief 获取当前加载的模型ID
     */
    Q_INVOKABLE QString currentModelId() const;
    
    /**
     * @brief 获取模型注册表
     */
    ModelRegistry* modelRegistry() const;

signals:
    // ========== 状态信号 ==========
    void processingChanged(bool processing);
    void queueSizeChanged(int size);
    void currentTaskChanged(const QString& taskId);
    void progressChanged(double progress);
    
    // ========== 任务信号 ==========
    void taskSubmitted(const QString& taskId);
    void taskStarted(const QString& taskId);
    void taskProgress(const QString& taskId, double progress, const QString& stage);
    void taskCompleted(const QString& taskId, const QString& outputPath);
    void taskFailed(const QString& taskId, const QString& error, AIErrorType errorType);
    void taskCancelled(const QString& taskId, AICancelReason reason);
    void taskStateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    
    // ========== 系统信号 ==========
    void serviceInitialized();
    void serviceShutdown();
    void errorOccurred(const QString& error);

private slots:
    void onWorkerStateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    void onWorkerProgressChanged(const AITaskProgress& progress);
    void onWorkerTaskCompleted(const AITaskResult& result);
    void onWorkerError(const QString& taskId, const QString& error, AIErrorType type);
    void processNextTask();

private:
    explicit AIInferenceService(QObject* parent = nullptr);
    ~AIInferenceService() override;
    
    // 禁止复制
    AIInferenceService(const AIInferenceService&) = delete;
    AIInferenceService& operator=(const AIInferenceService&) = delete;
    
    void startWorkerThread();
    void stopWorkerThread();
    void connectWorkerSignals();
    void disconnectWorkerSignals();
    
    // 单例实例
    static AIInferenceService* s_instance;
    static QMutex s_instanceMutex;
    
    // 成员变量
    bool m_initialized;
    AIServiceConfig m_config;
    ModelRegistry* m_modelRegistry;
    
    std::unique_ptr<AITaskQueue> m_taskQueue;
    std::unique_ptr<AIInferenceThread> m_workerThread;
    
    QString m_currentTaskId;
    std::atomic<double> m_currentProgress;
    std::atomic<bool> m_isProcessing;
    
    mutable QMutex m_mutex;
    
    // 进度节流
    qint64 m_lastProgressEmitMs;
    
    // 活动任务跟踪
    std::unordered_map<std::string, AITaskState> m_taskStates;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIINFERENCESERVICE_H
