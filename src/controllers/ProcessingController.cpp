/**
 * @file ProcessingController.cpp
 * @brief 处理控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/FileController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/models/MessageModel.h"
#include "EnhanceVision/services/ImageExportService.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUuid>
#include <QDebug>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

namespace EnhanceVision {

ProcessingController::ProcessingController(QObject* parent)
    : QObject(parent)
    , m_processingModel(new ProcessingModel(this))
    , m_queueStatus(QueueStatus::Running)
    , m_threadPool(new QThreadPool(this))
    , m_currentProcessingCount(0)
    , m_taskCounter(0)
    , m_fileController(nullptr)
    , m_messageModel(nullptr)
    , m_sessionController(nullptr)
{
    // 从设置中获取最大并发数
    m_maxConcurrentTasks = SettingsController::instance()->maxConcurrentTasks();
    m_threadPool->setMaxThreadCount(m_maxConcurrentTasks);

    connect(SettingsController::instance(), &SettingsController::maxConcurrentTasksChanged,
            this, [this]() {
        m_maxConcurrentTasks = SettingsController::instance()->maxConcurrentTasks();
        m_threadPool->setMaxThreadCount(m_maxConcurrentTasks);
        emit maxConcurrentTasksChanged();
    });
}

ProcessingController::~ProcessingController()
{
    m_threadPool->waitForDone();
}

ProcessingModel* ProcessingController::processingModel() const
{
    return m_processingModel;
}

QueueStatus ProcessingController::queueStatus() const
{
    return m_queueStatus;
}

int ProcessingController::queueSize() const
{
    return m_tasks.size();
}

int ProcessingController::currentProcessingCount() const
{
    return m_currentProcessingCount;
}

int ProcessingController::maxConcurrentTasks() const
{
    return m_maxConcurrentTasks;
}

void ProcessingController::pauseQueue()
{
    if (m_queueStatus != QueueStatus::Paused) {
        m_queueStatus = QueueStatus::Paused;
        emit queueStatusChanged();
        emit queuePaused();
    }
}

void ProcessingController::resumeQueue()
{
    if (m_queueStatus != QueueStatus::Running) {
        m_queueStatus = QueueStatus::Running;
        emit queueStatusChanged();
        emit queueResumed();
        processNextTask();
    }
}

void ProcessingController::setMessageModel(MessageModel* messageModel)
{
    m_messageModel = messageModel;
}

void ProcessingController::setSessionController(SessionController* sessionController)
{
    m_sessionController = sessionController;
}

void ProcessingController::cancelTask(const QString& taskId)
{
    // 支持按 taskId 或 messageId 取消
    // 先检查是否是 messageId（QML 传的是 message.id）
    bool isMessageId = false;
    for (const auto& task : m_tasks) {
        if (task.messageId == taskId) {
            isMessageId = true;
            break;
        }
    }

    if (isMessageId) {
        // 取消该消息下的所有任务
        for (int i = m_tasks.size() - 1; i >= 0; --i) {
            if (m_tasks[i].messageId == taskId) {
                if (m_tasks[i].status == ProcessingStatus::Pending) {
                    m_tasks[i].status = ProcessingStatus::Cancelled;
                    emit taskCancelled(m_tasks[i].taskId);
                    m_tasks.removeAt(i);
                } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                    m_tasks[i].status = ProcessingStatus::Cancelled;
                    m_currentProcessingCount--;
                    emit currentProcessingCountChanged();
                    emit taskCancelled(m_tasks[i].taskId);
                }
            }
        }
        updateQueuePositions();
        m_processingModel->updateTasks(m_tasks);
        emit queueSizeChanged();

        // 更新消息状态为已取消
        if (m_messageModel) {
            m_messageModel->updateStatus(taskId, static_cast<int>(ProcessingStatus::Cancelled));
        }
        processNextTask();
        return;
    }

    // 按 taskId 取消单个任务
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            QString messageId = m_tasks[i].messageId;
            if (m_tasks[i].status == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                m_tasks.removeAt(i);
                updateQueuePositions();
                emit taskCancelled(taskId);
                emit queueSizeChanged();
            } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                m_currentProcessingCount--;
                emit currentProcessingCountChanged();
                emit taskCancelled(taskId);
            }
            m_processingModel->updateTasks(m_tasks);
            syncMessageProgress(messageId);
            syncMessageStatus(messageId);
            processNextTask();
            break;
        }
    }
}

void ProcessingController::cancelAllTasks()
{
    for (auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Pending) {
            task.status = ProcessingStatus::Cancelled;
            emit taskCancelled(task.taskId);
        } else if (task.status == ProcessingStatus::Processing) {
            task.status = ProcessingStatus::Cancelled;
            emit taskCancelled(task.taskId);
        }
    }

    // 移除已取消的待处理任务
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [](const QueueTask& task) {
                return task.status == ProcessingStatus::Cancelled &&
                       task.startedAt.isNull();
            }),
        m_tasks.end()
    );

    updateQueuePositions();
    m_processingModel->updateTasks(m_tasks);
    emit queueSizeChanged();
}

QString ProcessingController::addTask(const Message& message)
{
    // 为每个媒体文件创建一个任务
    for (const auto& mediaFile : message.mediaFiles) {
        QueueTask task;
        task.taskId = generateTaskId();
        task.messageId = message.id;
        task.fileId = mediaFile.id;
        task.position = m_tasks.size();
        task.progress = 0;
        task.queuedAt = QDateTime::currentDateTime();
        task.status = ProcessingStatus::Pending;

        m_tasks.append(task);
        m_processingModel->addTask(task);
        m_taskToMessage[task.taskId] = message.id;

        emit taskAdded(task.taskId);
    }

    updateQueuePositions();
    emit queueSizeChanged();

    // 开始处理
    processNextTask();

    return message.id;
}

void ProcessingController::reorderTask(const QString& taskId, int newPosition)
{
    int oldIndex = -1;
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            oldIndex = i;
            break;
        }
    }

    if (oldIndex == -1 || newPosition < 0 || newPosition >= m_tasks.size()) {
        return;
    }

    // 只允许重新排列待处理的任务
    if (m_tasks[oldIndex].status != ProcessingStatus::Pending) {
        return;
    }

    QueueTask task = m_tasks.takeAt(oldIndex);
    m_tasks.insert(newPosition, task);

    updateQueuePositions();
    m_processingModel->updateTasks(m_tasks);
}

QueueTask* ProcessingController::getTask(const QString& taskId)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            return &task;
        }
    }
    return nullptr;
}

QList<QueueTask> ProcessingController::getAllTasks() const
{
    return m_tasks;
}

void ProcessingController::clearCompletedTasks()
{
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [](const QueueTask& task) {
                return task.status == ProcessingStatus::Completed ||
                       task.status == ProcessingStatus::Failed ||
                       task.status == ProcessingStatus::Cancelled;
            }),
        m_tasks.end()
    );

    updateQueuePositions();
    m_processingModel->updateTasks(m_tasks);
    emit queueSizeChanged();
}

void ProcessingController::processNextTask()
{
    if (m_queueStatus != QueueStatus::Running) {
        return;
    }

    while (m_currentProcessingCount < m_maxConcurrentTasks) {
        // 查找下一个待处理的任务
        QueueTask* nextTask = nullptr;
        for (auto& task : m_tasks) {
            if (task.status == ProcessingStatus::Pending) {
                nextTask = &task;
                break;
            }
        }

        if (!nextTask) {
            break; // 没有更多待处理任务
        }

        startTask(*nextTask);
    }
}

QString ProcessingController::generateTaskId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ProcessingController::updateQueuePositions()
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        m_tasks[i].position = i;
    }
}

void ProcessingController::startTask(QueueTask& task)
{
    task.status = ProcessingStatus::Processing;
    task.startedAt = QDateTime::currentDateTime();
    m_currentProcessingCount++;

    emit taskStarted(task.taskId);
    emit currentProcessingCountChanged();
    m_processingModel->updateTasks(m_tasks);

    // 查找该任务所属的消息，判断处理模式
    ProcessingMode mode = ProcessingMode::Shader;
    for (const auto& t : m_tasks) {
        if (t.taskId == task.taskId) {
            break;
        }
    }

    // Shader 模式：快速完成（模拟 GPU 实时处理）
    // AI 模式：较慢（模拟 AI 推理）
    int delayMs = (mode == ProcessingMode::Shader) ? 50 : 200;
    int progressStep = (mode == ProcessingMode::Shader) ? 25 : 10;
    int progressInterval = (mode == ProcessingMode::Shader) ? 20 : 200;

    qDebug() << "[ProcessingController] Starting task:" << task.taskId
             << "mode:" << (mode == ProcessingMode::Shader ? "Shader" : "AI")
             << "delay:" << delayMs << "ms";

    QString taskId = task.taskId;

    // 模拟任务处理 - 使用值捕获避免悬垂引用
    QTimer::singleShot(delayMs, this, [this, taskId, progressStep, progressInterval]() {
        QTimer* progressTimer = new QTimer(this);
        // 使用指针存储进度，避免引用悬垂
        int* progress = new int(0);

        connect(progressTimer, &QTimer::timeout, this, [this, progressTimer, taskId, progressStep, progress]() {
            *progress += progressStep;
            if (*progress >= 100) {
                progressTimer->stop();
                progressTimer->deleteLater();
                delete progress;
                completeTask(taskId, "");
            } else {
                updateTaskProgress(taskId, *progress);
            }
        });

        progressTimer->start(progressInterval);
    });
}

void ProcessingController::updateTaskProgress(const QString& taskId, int progress)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.progress = progress;
            emit taskProgress(taskId, progress);
            m_processingModel->updateTasks(m_tasks);

            // 同步更新所属消息的整体进度
            syncMessageProgress(task.messageId);
            break;
        }
    }
}

void ProcessingController::completeTask(const QString& taskId, const QString& resultPath)
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            QString messageId = m_tasks[i].messageId;
            QString fileId = m_tasks[i].fileId;

            if (m_tasks[i].status == ProcessingStatus::Cancelled) {
                emit taskCancelled(taskId);
            } else {
                m_tasks[i].status = ProcessingStatus::Completed;
                m_tasks[i].progress = 100;
                emit taskCompleted(taskId, resultPath);

                Message msg = m_messageModel->messageById(messageId);
                
                for (const MediaFile& mf : msg.mediaFiles) {
                    if (mf.id == fileId) {
                        if (msg.mode == ProcessingMode::Shader && ImageUtils::isImageFile(mf.filePath)) {
                            QString processedDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/processed";
                            QDir().mkpath(processedDir);
                            QString processedPath = processedDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
                            
                            PendingExport pending;
                            pending.taskId = taskId;
                            pending.messageId = messageId;
                            pending.fileId = fileId;
                            pending.originalPath = mf.filePath;
                            pending.outputPath = processedPath;
                            pending.shaderParams = shaderParamsToVariantMap(msg.shaderParams);
                            
                            m_pendingExports[taskId] = pending;
                            
                            qDebug() << "[ProcessingController] Requesting GPU export for task:" << taskId
                                     << "image:" << mf.filePath
                                     << "output:" << processedPath;
                            
                            emit requestShaderExport(taskId, mf.filePath, pending.shaderParams, processedPath);
                        } else {
                            if (m_messageModel) {
                                m_messageModel->updateFileStatus(messageId, fileId,
                                    static_cast<int>(ProcessingStatus::Completed), mf.filePath);
                            }
                            
                            m_currentProcessingCount--;
                            emit currentProcessingCountChanged();
                            m_processingModel->updateTasks(m_tasks);
                            
                            syncMessageProgress(messageId);
                            syncMessageStatus(messageId);
                            processNextTask();
                        }
                        break;
                    }
                }
            }
            break;
        }
    }
}

void ProcessingController::onShaderExportCompleted(const QString& exportId, bool success, const QString& outputPath, const QString& error)
{
    qDebug() << "[ProcessingController] Shader export completed:" << exportId
             << "success:" << success
             << "output:" << outputPath
             << "error:" << error;
    
    if (!m_pendingExports.contains(exportId)) {
        qWarning() << "[ProcessingController] Unknown export ID:" << exportId;
        return;
    }
    
    PendingExport pending = m_pendingExports.take(exportId);
    
    if (success) {
        QImage processedThumb = ImageUtils::generateThumbnail(outputPath, QSize(256, 256));
        QString thumbId = "processed_" + pending.fileId;
        if (ThumbnailProvider::instance() && !processedThumb.isNull()) {
            ThumbnailProvider::instance()->setThumbnail(thumbId, processedThumb);
        }
        
        if (m_messageModel) {
            m_messageModel->updateFileStatus(pending.messageId, pending.fileId,
                static_cast<int>(ProcessingStatus::Completed), outputPath);
        }
    } else {
        if (m_messageModel) {
            m_messageModel->updateFileStatus(pending.messageId, pending.fileId,
                static_cast<int>(ProcessingStatus::Failed), QString());
            m_messageModel->updateErrorMessage(pending.messageId, error);
        }
    }
    
    m_currentProcessingCount--;
    emit currentProcessingCountChanged();
    m_processingModel->updateTasks(m_tasks);
    
    syncMessageProgress(pending.messageId);
    syncMessageStatus(pending.messageId);
    
    processNextTask();
}

QVariantMap ProcessingController::shaderParamsToVariantMap(const ShaderParams& params)
{
    QVariantMap map;
    map["brightness"] = params.brightness;
    map["contrast"] = params.contrast;
    map["saturation"] = params.saturation;
    map["hue"] = params.hue;
    map["sharpness"] = params.sharpness;
    map["blur"] = params.blur;
    map["denoise"] = params.denoise;
    map["exposure"] = params.exposure;
    map["gamma"] = params.gamma;
    map["temperature"] = params.temperature;
    map["tint"] = params.tint;
    map["vignette"] = params.vignette;
    map["highlights"] = params.highlights;
    map["shadows"] = params.shadows;
    return map;
}

void ProcessingController::failTask(const QString& taskId, const QString& error)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            QString messageId = task.messageId;
            QString fileId = task.fileId;

            task.status = ProcessingStatus::Failed;
            emit taskFailed(taskId, error);
            m_currentProcessingCount--;
            emit currentProcessingCountChanged();
            m_processingModel->updateTasks(m_tasks);

            // 更新消息中对应文件的状态
            if (m_messageModel) {
                m_messageModel->updateFileStatus(messageId, fileId,
                    static_cast<int>(ProcessingStatus::Failed), QString());
                m_messageModel->updateErrorMessage(messageId, error);
            }

            // 同步消息整体进度和状态
            syncMessageProgress(messageId);
            syncMessageStatus(messageId);

            // 继续处理下一个任务
            processNextTask();
            break;
        }
    }
}

void ProcessingController::setFileController(FileController* fileController)
{
    m_fileController = fileController;
}

QString ProcessingController::sendToProcessing(int mode, const QVariantMap& params)
{
    if (!m_fileController) {
        qWarning() << "FileController not set!";
        return QString();
    }

    QList<MediaFile> files = m_fileController->getAllFiles();
    if (files.isEmpty()) {
        qWarning() << "No files to process!";
        return QString();
    }

    // 创建 Message 对象
    Message message;
    message.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    message.timestamp = QDateTime::currentDateTime();
    message.mode = (mode == 0) ? ProcessingMode::Shader : ProcessingMode::AIInference;
    message.mediaFiles = files;
    message.parameters = params;
    message.status = ProcessingStatus::Pending;
    message.progress = 0;

    // 设置 Shader 参数
    if (mode == 0) {
        message.shaderParams.brightness = params.value("brightness", 0.0f).toFloat();
        message.shaderParams.contrast = params.value("contrast", 1.0f).toFloat();
        message.shaderParams.saturation = params.value("saturation", 1.0f).toFloat();
        message.shaderParams.hue = params.value("hue", 0.0f).toFloat();
        message.shaderParams.sharpness = params.value("sharpness", 0.0f).toFloat();
        message.shaderParams.blur = params.value("blur", 0.0f).toFloat();
        message.shaderParams.denoise = params.value("denoise", 0.0f).toFloat();
        message.shaderParams.exposure = params.value("exposure", 0.0f).toFloat();
        message.shaderParams.gamma = params.value("gamma", 1.0f).toFloat();
        message.shaderParams.temperature = params.value("temperature", 0.0f).toFloat();
        message.shaderParams.tint = params.value("tint", 0.0f).toFloat();
        message.shaderParams.vignette = params.value("vignette", 0.0f).toFloat();
        message.shaderParams.highlights = params.value("highlights", 0.0f).toFloat();
        message.shaderParams.shadows = params.value("shadows", 0.0f).toFloat();
    }

    // 将消息添加到 MessageModel（QML 会自动更新显示）
    if (m_messageModel) {
        m_messageModel->addMessage(message);
    }
    
    // 同步消息到当前活动的 Session
    if (m_sessionController && m_messageModel) {
        m_sessionController->syncCurrentMessagesToSession();
    }

    // 清空文件列表（文件已转入消息）
    m_fileController->clearFiles();

    qDebug() << "Sending message to processing queue:" << message.id
             << "with" << files.size() << "files";
    return addTask(message);
}

void ProcessingController::syncMessageProgress(const QString& messageId)
{
    if (!m_messageModel) return;

    // 计算该消息下所有任务的加权平均进度
    int totalTasks = 0;
    int totalProgress = 0;

    for (const auto& task : m_tasks) {
        if (task.messageId == messageId) {
            totalTasks++;
            if (task.status == ProcessingStatus::Completed) {
                totalProgress += 100;
            } else if (task.status == ProcessingStatus::Failed ||
                       task.status == ProcessingStatus::Cancelled) {
                totalProgress += 100; // 视为已结束
            } else {
                totalProgress += task.progress;
            }
        }
    }

    int overallProgress = (totalTasks > 0) ? (totalProgress / totalTasks) : 0;
    m_messageModel->updateProgress(messageId, overallProgress);
}

void ProcessingController::syncMessageStatus(const QString& messageId)
{
    if (!m_messageModel) return;

    int totalTasks = 0;
    int completedTasks = 0;
    int failedTasks = 0;
    int cancelledTasks = 0;
    int processingTasks = 0;

    for (const auto& task : m_tasks) {
        if (task.messageId == messageId) {
            totalTasks++;
            switch (task.status) {
            case ProcessingStatus::Completed: completedTasks++; break;
            case ProcessingStatus::Failed:    failedTasks++;    break;
            case ProcessingStatus::Cancelled: cancelledTasks++; break;
            case ProcessingStatus::Processing: processingTasks++; break;
            default: break;
            }
        }
    }

    if (totalTasks == 0) return;

    int finishedTasks = completedTasks + failedTasks + cancelledTasks;

    ProcessingStatus newStatus;
    if (finishedTasks == totalTasks) {
        // 所有任务结束
        if (failedTasks > 0 && completedTasks == 0) {
            newStatus = ProcessingStatus::Failed;
        } else if (cancelledTasks == totalTasks) {
            newStatus = ProcessingStatus::Cancelled;
        } else {
            newStatus = ProcessingStatus::Completed;
        }
    } else if (processingTasks > 0 || completedTasks > 0) {
        newStatus = ProcessingStatus::Processing;
    } else {
        newStatus = ProcessingStatus::Pending;
    }

    m_messageModel->updateStatus(messageId, static_cast<int>(newStatus));

    // 更新队列位置
    int queuePos = 0;
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && task.status == ProcessingStatus::Pending) {
            queuePos = task.position + 1;
            break;
        }
    }
    m_messageModel->updateQueuePosition(messageId, queuePos);
}

} // namespace EnhanceVision
