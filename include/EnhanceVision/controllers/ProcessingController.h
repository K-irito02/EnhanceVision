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
#include "EnhanceVision/core/TaskStateManager.h"
#include "EnhanceVision/core/ImageProcessor.h"
#include "EnhanceVision/core/video/AIVideoProcessor.h"
#include "EnhanceVision/core/ProgressManager.h"
#include "EnhanceVision/controllers/SessionSyncHelper.h"
#include "EnhanceVision/controllers/ProgressSyncHelper.h"

class QTimer;

namespace EnhanceVision {

class MessageModel;
class TaskCoordinator;
class ResourceManager;
class AIEnginePool;

class TaskSwitchSynchronizer : public QObject {
    Q_OBJECT
    
public:
    static TaskSwitchSynchronizer* instance() {
        static TaskSwitchSynchronizer* s_instance = nullptr;
        if (!s_instance) {
            s_instance = new TaskSwitchSynchronizer();
        }
        return s_instance;
    }
    
    void beginTaskSwitch(const QString& oldTaskId, const QString& newTaskId) {
        QMutexLocker locker(&m_mutex);
        m_currentSwitch = {oldTaskId, newTaskId, QDateTime::currentMSecsSinceEpoch()};
        qInfo() << "[TaskSwitchSynchronizer] Begin switch from" << oldTaskId 
                << "to" << newTaskId;
    }
    
    void endTaskSwitch(const QString& taskId) {
        QMutexLocker locker(&m_mutex);
        if (m_currentSwitch.newTaskId == taskId || m_currentSwitch.oldTaskId == taskId) {
            m_currentSwitch = {};
            m_switchCompleteCv.notify_all();
        }
    }
    
    bool waitForSwitchComplete(int timeoutMs = 5000) {
        std::unique_lock<std::mutex> lock(m_switchMutex);
        return m_switchCompleteCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this]() { return m_currentSwitch.oldTaskId.isEmpty(); });
    }
    
private:
    TaskSwitchSynchronizer() = default;
    
    struct SwitchInfo {
        QString oldTaskId;
        QString newTaskId;
        qint64 timestamp;
    };
    
    mutable QMutex m_mutex;
    std::mutex m_switchMutex;
    std::condition_variable m_switchCompleteCv;
    SwitchInfo m_currentSwitch;
};

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
    bool hasActiveTaskForFile(const QString& messageId, const QString& fileId) const;
    void removeStaleTasksForFile(const QString& messageId, const QString& fileId);
    
    Q_INVOKABLE void preloadModel(const QString& modelId);
    
    Q_INVOKABLE QVariantMap getTaskProgress(const QString& taskId) const;
    Q_INVOKABLE QVariantMap getMessageProgress(const QString& messageId) const;
    
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

    AIEngine* m_aiEngine;  // 已废弃，保持为 nullptr，统一使用 m_aiEnginePool
    AIEnginePool* m_aiEnginePool;
    ModelRegistry* m_modelRegistry;
    QHash<QString, Message> m_taskMessages;
    QHash<QString, int> m_lastReportedTaskProgress;
    QHash<QString, qint64> m_lastTaskProgressUpdateMs;
    QSet<QString> m_preloadedModelIds;
    QSet<QString> m_pendingPreloadModelIds;
    QSet<QString> m_pendingModelLoadTaskIds;
    QHash<QString, QList<QMetaObject::Connection>> m_aiEngineConnections;
    
    SessionSyncHelper* m_sessionSyncHelper;
    ProgressSyncHelper* m_progressSyncHelper;
    
    QHash<QString, QSharedPointer<class VideoProcessor>> m_activeVideoProcessors;
    
    QHash<QString, QSharedPointer<AIVideoProcessor>> m_activeAIVideoProcessors;
    
    QHash<QString, QSharedPointer<ImageProcessor>> m_activeImageProcessors;
    
    std::unique_ptr<ProgressManager> m_progressManager;
    QHash<QString, QString> m_taskToProgressId;
    QHash<QString, ProgressReporter*> m_taskProgressReporters;

    QSet<QString> m_cleaningUpTaskIds;
    QMap<QString, QTimer*> m_orphanTimers;
    QSet<QString> m_pendingCancelKeys;
    
    QSet<QString> m_cancellingTaskIds;  ///< 正在取消的任务ID（防止并发取消）
    mutable QMutex m_cancelMutex;  ///< 取消操作的互斥锁

    enum class TaskLifecycle {
        Active,
        Canceling,
        Draining,
        Cleaning,
        Dead
    };
    QHash<QString, TaskLifecycle> m_taskLifecycle;
    QHash<QString, QSharedPointer<AIVideoProcessor>> m_dyingProcessors;
    QHash<QString, QSharedPointer<VideoProcessor>> m_dyingVideoProcessors;
    

    QString generateTaskId();
    void releaseTaskResources(const QString& taskId);
    void launchAiInference(AIEngine* engine, const QString& taskId, const QString& inputPath, 
                           const QString& outputPath, const Message& message);
    void launchAIVideoProcessor(AIEngine* engine, const QString& taskId,
                                const QString& inputPath, const QString& outputPath,
                                const Message& message, const ModelInfo& modelInfo,
                                const QVariantMap& effectiveParams);
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
    void requestSessionSync();
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
    void handleOrphanedVideoTask(const QString& taskId);
    void cleanupTask(const QString& taskId);
    void terminateTask(const QString& taskId, const QString& reason = QString());
    void connectAiEngineForTask(AIEngine* engine, const QString& taskId);
    void disconnectAiEngineForTask(const QString& taskId);
    
    qint64 estimateMemoryUsage(const QString& filePath, MediaType type) const;
    void registerTaskContext(const QString& taskId, const QString& messageId, 
                             const QString& sessionId, const QString& fileId,
                             qint64 estimatedMemoryMB);
    void validateAndRepairQueueState();

    QString createAndRegisterTask(const Message& message, const MediaFile& file,
                                   const QString& sessionId);
    void cancelAIEngineForTask(const QString& taskId);
    void adjustProcessingCount(int delta);

    enum class CancelMode { PendingOnly, ForceProcessing };
    struct CancelResult {
        int cancelledPending = 0;
        int cancelledProcessing = 0;
    };
    CancelResult cancelAndRemoveTask(int index, CancelMode mode);
    void processDeletionBatch(const QString& messageId);

    struct DeletionBatchItem {
        QString fileId;
        int origIndex = -1;
    };
    QHash<QString, QList<DeletionBatchItem>> m_deletionBatches;
    QTimer* m_batchProcessTimer = nullptr;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGCONTROLLER_H
