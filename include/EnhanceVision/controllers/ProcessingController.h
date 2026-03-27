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
#include <QSet>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <functional>
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/models/ProcessingModel.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"

class QTimer;

namespace EnhanceVision {

class MessageModel;
class TaskCoordinator;
class ResourceManager;
class AIEnginePool;

struct PendingExport {
    QString taskId;
    QString messageId;
    QString sessionId;
    QString fileId;
    QString originalPath;
    QString outputPath;
    QVariantMap shaderParams;
    bool isVideoThumbnail = false;
    QString tempFramePath;
    qint64 estimatedMemoryMB = 0;
    qint64 estimatedGpuMemoryMB = 0;
};

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
    Q_PROPERTY(int resourcePressure READ resourcePressure NOTIFY resourcePressureChanged)
    Q_PROPERTY(EnhanceVision::ModelRegistry* modelRegistry READ modelRegistry CONSTANT)
    Q_PROPERTY(EnhanceVision::AIEngine* aiEngine READ aiEngine CONSTANT)

public:
    explicit ProcessingController(QObject* parent = nullptr);
    ~ProcessingController() override;

    void setFileController(class FileController* fileController);
    void setMessageModel(MessageModel* messageModel);
    void setSessionController(class SessionController* sessionController);

    ProcessingModel* processingModel() const;
    QueueStatus queueStatus() const;
    int queueSize() const;
    int currentProcessingCount() const;
    int resourcePressure() const;
    ModelRegistry* modelRegistry() const;
    AIEngine* aiEngine() const;

    Q_INVOKABLE void pauseQueue();
    Q_INVOKABLE void resumeQueue();
    Q_INVOKABLE void cancelTask(const QString& taskId);
    Q_INVOKABLE void cancelAllTasks();
    Q_INVOKABLE void forceCancelAllTasks();
    Q_INVOKABLE QString addTask(const Message& message);
    Q_INVOKABLE QString sendToProcessing(int mode, const QVariantMap& params);
    Q_INVOKABLE void reorderTask(const QString& taskId, int newPosition);
    Q_INVOKABLE QueueTask* getTask(const QString& taskId);
    Q_INVOKABLE QList<QueueTask> getAllTasks() const;
    Q_INVOKABLE void clearCompletedTasks();
    Q_INVOKABLE void retryMessage(const QString& messageId);
    Q_INVOKABLE void retryFailedFiles(const QString& messageId);
    Q_INVOKABLE void retrySingleFailedFile(const QString& messageId, int fileIndex);
    void autoRetryInterruptedFiles(const QString& messageId, const QString& sessionId);
    bool hasTasksForMessage(const QString& messageId) const;
    
    Q_INVOKABLE void preloadModel(const QString& modelId);
    
    void cancelMessageTasks(const QString& messageId);
    void cancelMessageFileTasks(const QString& messageId, const QString& fileId);
    void cancelSessionTasks(const QString& sessionId);
    
    void pauseSessionTasks(const QString& sessionId);
    void resumeSessionTasks(const QString& sessionId);
    
    void cancelVideoProcessing(const QString& taskId);
    
    void onSessionChanging(const QString& oldSessionId, const QString& newSessionId);

signals:
    void queueStatusChanged();
    void queueSizeChanged();
    void currentProcessingCountChanged();
    void resourcePressureChanged();
    void taskAdded(const QString& taskId);
    void taskStarted(const QString& taskId);
    void taskProgress(const QString& taskId, int progress);
    void taskCompleted(const QString& taskId, const QString& resultPath);
    void taskFailed(const QString& taskId, const QString& error);
    void taskCancelled(const QString& taskId);
    void queuePaused();
    void queueResumed();
    
    void messageTasksCancelled(const QString& messageId);
    void sessionTasksCancelled(const QString& sessionId);
    
    void requestShaderExport(
        const QString& exportId,
        const QString& imagePath,
        const QVariantMap& shaderParams,
        const QString& outputPath
    );
    
    void resourceExhausted();

public slots:
    void onShaderExportCompleted(const QString& exportId, bool success, const QString& outputPath, const QString& error);
    void onTaskOrphaned(const QString& taskId);
    void onResourcePressureChanged(int pressureLevel);

private slots:
    void processNextTask();

private:
    ProcessingModel* m_processingModel;
    QueueStatus m_queueStatus;
    QList<QueueTask> m_tasks;
    QThreadPool* m_threadPool;
    int m_currentProcessingCount;
    int m_taskCounter;
    class FileController* m_fileController;
    MessageModel* m_messageModel;
    class SessionController* m_sessionController;
    QHash<QString, QString> m_taskToMessage;
    QHash<QString, PendingExport> m_pendingExports;
    
    TaskCoordinator* m_taskCoordinator;
    ResourceManager* m_resourceManager;
    QHash<QString, TaskContext> m_taskContexts;
    int m_resourcePressure;

    AIEngine* m_aiEngine;
    AIEnginePool* m_aiEnginePool;
    ModelRegistry* m_modelRegistry;
    QHash<QString, Message> m_taskMessages;
    QHash<QString, int> m_lastReportedTaskProgress;
    QHash<QString, qint64> m_lastTaskProgressUpdateMs;
    QHash<QString, qint64> m_lastMessageProgressSyncMs;
    QHash<QString, qint64> m_lastMessageStatusSyncMs;
    QHash<QString, qint64> m_lastSessionMemorySyncMs;
    QSet<QString> m_pendingMemorySyncMessageIds;
    QTimer* m_memorySyncTimer = nullptr;
    QSet<QString> m_preloadedModelIds;
    QSet<QString> m_pendingPreloadModelIds;
    QSet<QString> m_pendingModelLoadTaskIds;
    QHash<QString, QList<QMetaObject::Connection>> m_aiEngineConnections;
    QTimer* m_sessionSyncTimer;
    
    QHash<QString, QSharedPointer<class VideoProcessor>> m_activeVideoProcessors;
    

    QString generateTaskId();
    void launchAiInference(AIEngine* engine, const QString& taskId, const QString& inputPath, 
                           const QString& outputPath, const Message& message);
    void requestSessionSync();
    void requestSessionMemorySync(const QString& messageId = QString());
    void flushSessionMemorySync();
    void syncVisibleMessageIfNeeded(const QString& sessionId, const QString& messageId,
                                    const std::function<void()>& syncFn);
    void syncModelTasks();
    void syncModelTask(const QueueTask& task);
    void syncModelTaskById(const QString& taskId);
    void syncMessageProgress(const QString& messageId, const QString& sessionId = QString());
    void syncMessageStatus(const QString& messageId, const QString& sessionId = QString());
    QString resolveSessionIdForMessage(const QString& messageId, const QString& fallbackSessionId = QString()) const;
    Message messageForSession(const QString& sessionId, const QString& messageId) const;
    void updateFileStatusForSessionMessage(const QString& sessionId, const QString& messageId,
                                           const QString& fileId, ProcessingStatus status,
                                           const QString& resultPath);
    void updateErrorForSessionMessage(const QString& sessionId, const QString& messageId,
                                      const QString& errorMessage);
    void updateProgressForSessionMessage(const QString& sessionId, const QString& messageId, int progress);
    void updateStatusForSessionMessage(const QString& sessionId, const QString& messageId, ProcessingStatus status);
    void updateQueuePositionForSessionMessage(const QString& sessionId, const QString& messageId, int queuePosition);
    void updateQueuePositions();
    bool tryStartTask(QueueTask& task);
    void startTask(QueueTask& task);
    void updateTaskProgress(const QString& taskId, int progress);
    void completeTask(const QString& taskId, const QString& resultPath);
    void failTask(const QString& taskId, const QString& error);
    void processShaderVideoThumbnailAsync(const QString& taskId,
                                          const QString& messageId,
                                          const QString& fileId,
                                          const QString& filePath,
                                          const ShaderParams& shaderParams);
    QVariantMap shaderParamsToVariantMap(const ShaderParams& params);
    
    void finalizeTask(const QString& taskId, const QString& sessionId, const QString& messageId);
    void gracefulCancel(const QString& taskId, int timeoutMs = 5000);
    void handleOrphanedTask(const QString& taskId);
    void cleanupTask(const QString& taskId);
    void connectAiEngineForTask(AIEngine* engine, const QString& taskId);
    void disconnectAiEngineForTask(const QString& taskId);
    
    qint64 estimateMemoryUsage(const QString& filePath, MediaType type) const;
    void registerTaskContext(const QString& taskId, const QString& messageId, 
                             const QString& sessionId, const QString& fileId,
                             qint64 estimatedMemoryMB);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGCONTROLLER_H
