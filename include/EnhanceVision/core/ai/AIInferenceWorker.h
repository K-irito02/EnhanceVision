/**
 * @file AIInferenceWorker.h
 * @brief AI推理工作器（工作线程）
 * @author EnhanceVision Team
 * 
 * 在独立线程中执行AI推理任务。
 * 特性：
 * - 独立工作线程，不阻塞UI
 * - 支持优雅取消和强制取消
 * - 自动资源管理
 * - 错误恢复能力
 */

#ifndef ENHANCEVISION_AIINFERENCEWORKER_H
#define ENHANCEVISION_AIINFERENCEWORKER_H

#include "AIInferenceTypes.h"
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <atomic>
#include <memory>

namespace EnhanceVision {

class AIEngine;
class AIVideoProcessor;
class ModelRegistry;
class ModelInfo;

/**
 * @brief AI推理工作器
 * 
 * 负责在工作线程中执行单个AI推理任务。
 * 使用状态机模式管理任务生命周期。
 */
class AIInferenceWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit AIInferenceWorker(QObject* parent = nullptr);
    ~AIInferenceWorker() override;
    
    /**
     * @brief 设置模型注册表
     */
    void setModelRegistry(ModelRegistry* registry);
    
    /**
     * @brief 检查工作器是否空闲
     */
    bool isIdle() const;
    
    /**
     * @brief 检查是否正在处理
     */
    bool isProcessing() const;
    
    /**
     * @brief 获取当前任务ID
     */
    QString currentTaskId() const;
    
    /**
     * @brief 获取当前任务状态
     */
    AITaskState currentState() const;
    
    /**
     * @brief 获取当前进度
     */
    double currentProgress() const;
    
    /**
     * @brief 请求取消当前任务
     * @param reason 取消原因
     */
    void requestCancel(AICancelReason reason = AICancelReason::UserRequest);
    
    /**
     * @brief 强制取消当前任务（立即停止）
     */
    void forceCancel();
    
    /**
     * @brief 检查是否有取消请求
     */
    bool isCancelRequested() const;
    
    /**
     * @brief 重置工作器状态（任务完成后调用）
     */
    void reset();
    
    /**
     * @brief 等待当前任务完成
     * @param timeoutMs 超时时间
     * @return true 如果任务完成，false 如果超时
     */
    bool waitForCompletion(int timeoutMs = 30000);

public slots:
    /**
     * @brief 开始处理任务（在工作线程中调用）
     * @param request 任务请求
     */
    void startTask(const AITaskRequest& request);

signals:
    /**
     * @brief 任务状态变化信号
     */
    void stateChanged(const QString& taskId, AITaskState oldState, AITaskState newState);
    
    /**
     * @brief 进度更新信号
     */
    void progressChanged(const AITaskProgress& progress);
    
    /**
     * @brief 任务完成信号
     */
    void taskCompleted(const AITaskResult& result);
    
    /**
     * @brief 错误信号
     */
    void errorOccurred(const QString& taskId, const QString& error, AIErrorType type);

private:
    void setState(AITaskState newState);
    void updateProgress(double progress, const QString& stage = QString());
    
    // 图像处理
    AITaskResult processImage(const AITaskRequest& request);
    
    // 视频处理
    AITaskResult processVideo(const AITaskRequest& request);
    
    // 模型加载
    bool ensureModelLoaded(const QString& modelId, QString& errorOut);
    
    // GPU资源管理
    void cleanupGpuResources();
    void ensureGpuReady();
    
    // 检查取消点
    bool checkCancellation();
    
    // 成员变量
    ModelRegistry* m_modelRegistry;
    std::unique_ptr<AIEngine> m_engine;
    std::unique_ptr<AIVideoProcessor> m_videoProcessor;
    
    QString m_currentTaskId;
    std::atomic<AITaskState> m_state;
    std::atomic<double> m_progress;
    std::atomic<bool> m_cancelRequested;
    std::atomic<bool> m_forceCancel;
    AICancelReason m_cancelReason;
    
    QElapsedTimer m_taskTimer;
    mutable QMutex m_mutex;
    QMutex m_completionMutex;
    QWaitCondition m_completionCondition;
};

/**
 * @brief AI推理工作线程
 * 
 * 管理AIInferenceWorker的生命周期和线程。
 */
class AIInferenceThread : public QObject
{
    Q_OBJECT
    
public:
    explicit AIInferenceThread(QObject* parent = nullptr);
    ~AIInferenceThread() override;
    
    /**
     * @brief 启动工作线程
     */
    void start();
    
    /**
     * @brief 停止工作线程
     * @param waitMs 等待时间
     */
    void stop(int waitMs = 5000);
    
    /**
     * @brief 获取工作器指针
     */
    AIInferenceWorker* worker() const;
    
    /**
     * @brief 检查线程是否运行中
     */
    bool isRunning() const;

signals:
    void started();
    void stopped();

private:
    QThread* m_thread;
    AIInferenceWorker* m_worker;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIINFERENCEWORKER_H
