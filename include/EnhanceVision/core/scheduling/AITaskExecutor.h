/**
 * @file AITaskExecutor.h
 * @brief AI 任务执行器
 * @author EnhanceVision Team
 * 
 * 负责执行 AI 推理任务，调用处理层完成实际工作。
 */

#ifndef ENHANCEVISION_AITASKEXECUTOR_H
#define ENHANCEVISION_AITASKEXECUTOR_H

#include "AITaskTypes.h"
#include "EnhanceVision/core/inference/IInferenceBackend.h"
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <atomic>
#include <memory>

namespace EnhanceVision {

class AIImageProcessor;
class AIVideoProcessor;
class AIModelManager;
class ModelRegistry;

/**
 * @brief AI 任务执行器
 * 
 * 在独立线程中执行 AI 推理任务。
 * 调用处理层完成实际的图像/视频处理。
 */
class AITaskExecutor : public QObject
{
    Q_OBJECT

public:
    explicit AITaskExecutor(QObject* parent = nullptr);
    ~AITaskExecutor() override;
    
    /**
     * @brief 设置模型注册表
     */
    void setModelRegistry(ModelRegistry* registry);
    
    /**
     * @brief 设置推理后端
     */
    void setBackend(IInferenceBackend* backend);
    
    /**
     * @brief 执行任务
     * @param request 任务请求
     */
    void execute(const AITaskRequest& request);
    
    /**
     * @brief 请求取消当前任务
     */
    void requestCancel(AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 强制取消
     */
    void forceCancel();
    
    /**
     * @brief 检查是否空闲
     */
    bool isIdle() const;
    
    /**
     * @brief 检查是否正在处理
     */
    bool isProcessing() const;
    
    /**
     * @brief 获取当前任务 ID
     */
    QString currentTaskId() const;
    
    /**
     * @brief 获取当前状态
     */
    AITaskState currentState() const;
    
    /**
     * @brief 获取当前进度
     */
    double currentProgress() const;
    
    /**
     * @brief 重置执行器状态
     */
    void reset();
    
    /**
     * @brief 等待当前任务完成
     */
    bool waitForCompletion(int timeoutMs = 30000);

signals:
    void stateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    void progressChanged(const AITaskProgress& progress);
    void taskCompleted(const AITaskResult& result);
    void errorOccurred(const QString& taskId, const QString& error, AIErrorType type);

private:
    AITaskResult processImage(const AITaskRequest& request);
    AITaskResult processVideo(const AITaskRequest& request);
    
    void setState(AITaskState newState);
    void updateProgress(double progress, const QString& stage = QString());
    bool checkCancellation();
    bool ensureModelLoaded(const QString& modelId, QString& errorOut);
    
    ModelRegistry* m_modelRegistry = nullptr;
    IInferenceBackend* m_backend = nullptr;
    
    std::unique_ptr<AIImageProcessor> m_imageProcessor;
    std::unique_ptr<AIVideoProcessor> m_videoProcessor;
    std::unique_ptr<AIModelManager> m_modelManager;
    
    QString m_currentTaskId;
    std::atomic<AITaskState> m_state{AITaskState::Idle};
    std::atomic<double> m_progress{0.0};
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_forceCancel{false};
    AICancelReason m_cancelReason = AICancelReason::None;
    
    QElapsedTimer m_taskTimer;
    mutable QMutex m_mutex;
    QMutex m_completionMutex;
    QWaitCondition m_completionCondition;
};

/**
 * @brief AI 工作线程
 * 
 * 管理 AITaskExecutor 的线程生命周期。
 */
class AIWorkerThread : public QObject
{
    Q_OBJECT

public:
    explicit AIWorkerThread(QObject* parent = nullptr);
    ~AIWorkerThread() override;
    
    void start();
    void stop(int waitMs = 5000);
    
    AITaskExecutor* executor() const;
    bool isRunning() const;

signals:
    void started();
    void stopped();

private:
    QThread* m_thread = nullptr;
    AITaskExecutor* m_executor = nullptr;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AITASKEXECUTOR_H
