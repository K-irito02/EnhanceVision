/**
 * @file ProcessingController.h
 * @brief 处理控制器 - 管理处理队列、任务暂停/恢复/取消等
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PROCESSINGCONTROLLER_H
#define ENHANCEVISION_PROCESSINGCONTROLLER_H

#include <QObject>
#include <QString>
#include <QQueue>
#include <QThreadPool>
#include <QUuid>
#include <QHash>
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/models/ProcessingModel.h"

namespace EnhanceVision {

class MessageModel;

// QueueStatus 已在 DataTypes.h 中定义
// QueueTask 定义在 DataTypes.h 中

/**
 * @brief 处理控制器
 */
class ProcessingController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ProcessingModel* processingModel READ processingModel CONSTANT)
    Q_PROPERTY(QueueStatus queueStatus READ queueStatus NOTIFY queueStatusChanged)
    Q_PROPERTY(int queueSize READ queueSize NOTIFY queueSizeChanged)
    Q_PROPERTY(int currentProcessingCount READ currentProcessingCount NOTIFY currentProcessingCountChanged)
    Q_PROPERTY(int maxConcurrentTasks READ maxConcurrentTasks NOTIFY maxConcurrentTasksChanged)

public:
    explicit ProcessingController(QObject* parent = nullptr);
    ~ProcessingController() override;

    // 设置关联控制器/模型
    void setFileController(class FileController* fileController);
    void setMessageModel(MessageModel* messageModel);
    void setSessionController(class SessionController* sessionController);

    // 属性访问器
    ProcessingModel* processingModel() const;
    QueueStatus queueStatus() const;
    int queueSize() const;
    int currentProcessingCount() const;
    int maxConcurrentTasks() const;

    // Q_INVOKABLE 方法
    Q_INVOKABLE void pauseQueue();
    Q_INVOKABLE void resumeQueue();
    Q_INVOKABLE void cancelTask(const QString& taskId);
    Q_INVOKABLE void cancelAllTasks();
    Q_INVOKABLE QString addTask(const Message& message);
    Q_INVOKABLE QString sendToProcessing(int mode, const QVariantMap& params);
    Q_INVOKABLE void reorderTask(const QString& taskId, int newPosition);
    Q_INVOKABLE QueueTask* getTask(const QString& taskId);
    Q_INVOKABLE QList<QueueTask> getAllTasks() const;
    Q_INVOKABLE void clearCompletedTasks();

signals:
    void queueStatusChanged();
    void queueSizeChanged();
    void currentProcessingCountChanged();
    void maxConcurrentTasksChanged();
    void taskAdded(const QString& taskId);
    void taskStarted(const QString& taskId);
    void taskProgress(const QString& taskId, int progress);
    void taskCompleted(const QString& taskId, const QString& resultPath);
    void taskFailed(const QString& taskId, const QString& error);
    void taskCancelled(const QString& taskId);
    void queuePaused();
    void queueResumed();

private slots:
    void processNextTask();

private:
    ProcessingModel* m_processingModel;
    QueueStatus m_queueStatus;
    QList<QueueTask> m_tasks;
    QThreadPool* m_threadPool;
    int m_currentProcessingCount;
    int m_maxConcurrentTasks;
    int m_taskCounter;
    class FileController* m_fileController;
    MessageModel* m_messageModel;
    class SessionController* m_sessionController;
    QHash<QString, QString> m_taskToMessage;  ///< taskId -> messageId 映射

    QString generateTaskId();
    void syncMessageProgress(const QString& messageId);
    void syncMessageStatus(const QString& messageId);
    void updateQueuePositions();
    void startTask(QueueTask& task);
    void updateTaskProgress(const QString& taskId, int progress);
    void completeTask(const QString& taskId, const QString& resultPath);
    void failTask(const QString& taskId, const QString& error);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGCONTROLLER_H
