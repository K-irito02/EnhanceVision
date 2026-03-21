/**
 * @file ProcessingEngine.h
 * @brief 处理引擎调度器
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PROCESSINGENGINE_H
#define ENHANCEVISION_PROCESSINGENGINE_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/core/ImageProcessor.h"
#include "EnhanceVision/core/VideoProcessor.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"

namespace EnhanceVision {

/**
 * @brief 处理引擎调度器类
 * 负责处理任务调度、队列管理
 */
class ProcessingEngine : public QObject
{
    Q_OBJECT

public:
    explicit ProcessingEngine(QObject *parent = nullptr);
    ~ProcessingEngine() override;

    /**
     * @brief 获取单例实例
     */
    static ProcessingEngine* instance();

    /**
     * @brief 添加任务到队列
     * @param task 队列任务
     * @param message 关联的消息
     */
    void addTask(const QueueTask& task, const Message& message);

    /**
     * @brief 取消任务
     * @param taskId 任务 ID
     */
    void cancelTask(const QString& taskId);

    /**
     * @brief 暂停队列
     */
    void pauseQueue();

    /**
     * @brief 恢复队列
     */
    void resumeQueue();

    /**
     * @brief 获取队列状态
     * @return 队列状态
     */
    QueueStatus getQueueStatus() const;

    /**
     * @brief 获取队列中的任务列表
     * @return 任务列表
     */
    QList<QueueTask> getQueuedTasks() const;

    /**
     * @brief 获取当前处理的任务
     * @return 当前任务
     */
    QueueTask getCurrentTask() const;

    /**
     * @brief 获取最大并发任务数
     * @return 最大并发数
     */
    int getMaxConcurrentTasks() const;

    /**
     * @brief 设置最大并发任务数
     * @param count 最大并发数
     */
    void setMaxConcurrentTasks(int count);

signals:
    /**
     * @brief 队列状态变化信号
     * @param status 新状态
     */
    void queueStatusChanged(QueueStatus status);

    /**
     * @brief 任务添加信号
     * @param task 任务
     */
    void taskAdded(const QueueTask& task);

    /**
     * @brief 任务开始信号
     * @param task 任务
     */
    void taskStarted(const QueueTask& task);

    /**
     * @brief 任务进度更新信号
     * @param taskId 任务 ID
     * @param progress 进度 (0-100)
     * @param status 状态信息
     */
    void taskProgressChanged(const QString& taskId, int progress, const QString& status);

    /**
     * @brief 任务完成信号
     * @param task 任务
     * @param success 是否成功
     * @param resultPath 结果路径
     * @param error 错误信息
     */
    void taskFinished(const QueueTask& task, bool success, const QString& resultPath, const QString& error);

    /**
     * @brief 任务取消信号
     * @param task 任务
     */
    void taskCancelled(const QueueTask& task);

private slots:
    /**
     * @brief 处理下一个任务
     */
    void processNextTask();

    /**
     * @brief 图像处理进度回调
     */
    void onImageProgressChanged(int progress, const QString& status);

    /**
     * @brief 图像处理完成回调
     */
    void onImageFinished(bool success, const QString& resultPath, const QString& error);

    /**
     * @brief 视频处理进度回调
     */
    void onVideoProgressChanged(int progress, const QString& status);

    /**
     * @brief 视频处理完成回调
     */
    void onVideoFinished(bool success, const QString& resultPath, const QString& error);

    /**
     * @brief AI 推理进度回调
     */
    void onAIProgressChanged(double progress);

    /**
     * @brief AI 推理完成回调
     */
    void onAIFinished(bool success, const QString& resultPath, const QString& error);

private:
    /**
     * @brief 开始处理任务
     * @param task 任务
     * @param message 消息
     */
    void startTask(const QueueTask& task, const Message& message);

    static ProcessingEngine* s_instance;

    QQueue<QPair<QueueTask, Message>> m_taskQueue;
    QList<QPair<QueueTask, Message>> m_processingTasks;
    QueueStatus m_queueStatus;
    int m_maxConcurrentTasks;
    mutable QMutex m_mutex;

    ImageProcessor* m_imageProcessor;
    VideoProcessor* m_videoProcessor;
    AIEngine* m_aiEngine;
    ModelRegistry* m_modelRegistry;
    QString m_currentTaskId;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGENGINE_H
