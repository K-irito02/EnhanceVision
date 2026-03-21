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
#include "EnhanceVision/core/TaskCoordinator.h"
#include "EnhanceVision/core/ResourceManager.h"
#include <QUuid>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
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
    , m_taskCoordinator(TaskCoordinator::instance())
    , m_resourceManager(ResourceManager::instance())
    , m_resourcePressure(0)
{
    m_maxConcurrentTasks = SettingsController::instance()->maxConcurrentTasks();
    m_threadPool->setMaxThreadCount(m_maxConcurrentTasks);

    connect(SettingsController::instance(), &SettingsController::maxConcurrentTasksChanged,
            this, [this]() {
        m_maxConcurrentTasks = SettingsController::instance()->maxConcurrentTasks();
        m_threadPool->setMaxThreadCount(m_maxConcurrentTasks);
        m_resourceManager->setMaxConcurrentTasks(m_maxConcurrentTasks);
        emit maxConcurrentTasksChanged();
    });
    
    connect(m_taskCoordinator, &TaskCoordinator::taskOrphaned,
            this, &ProcessingController::onTaskOrphaned);
    
    connect(m_resourceManager, &ResourceManager::resourcePressureChanged,
            this, &ProcessingController::onResourcePressureChanged);
    
    connect(m_resourceManager, &ResourceManager::resourceExhausted,
            this, &ProcessingController::resourceExhausted);
    
    m_resourceManager->startMonitoring(2000);

    // 初始化 AI 推理引擎和模型注册表
    m_modelRegistry = new ModelRegistry(this);
    m_aiEngine = new AIEngine(this);

    QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
    if (!QDir(modelsPath).exists()) {
        modelsPath = QCoreApplication::applicationDirPath() + "/../resources/models";
    }
    if (!QDir(modelsPath).exists()) {
        modelsPath = QCoreApplication::applicationDirPath() + "/resources/models";
    }
    if (!QDir(modelsPath).exists()) {
        modelsPath = QDir(QCoreApplication::applicationDirPath()).filePath("../../resources/models");
    }
    m_modelRegistry->initialize(modelsPath);
    m_aiEngine->setModelRegistry(m_modelRegistry);

    // 连接 AI 推理信号
    connect(m_aiEngine, &AIEngine::progressChanged, this, [this](double progress) {
        // 找到正在处理的 AI 任务并更新进度
        for (auto& task : m_tasks) {
            if (task.status == ProcessingStatus::Processing) {
                updateTaskProgress(task.taskId, static_cast<int>(progress * 100));
                break;
            }
        }
    });

    connect(m_aiEngine, &AIEngine::processFileCompleted, this,
            [this](bool success, const QString& resultPath, const QString& error) {
        // 找到正在处理的 AI 任务并完成/失败
        for (auto& task : m_tasks) {
            if (task.status == ProcessingStatus::Processing) {
                if (success) {
                    completeTask(task.taskId, resultPath);
                } else {
                    failTask(task.taskId, error);
                }
                break;
            }
        }
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

ModelRegistry* ProcessingController::modelRegistry() const
{
    return m_modelRegistry;
}

AIEngine* ProcessingController::aiEngine() const
{
    return m_aiEngine;
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
    QString sessionId = m_sessionController ? m_sessionController->activeSessionId() : QString();
    
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
        m_taskMessages[task.taskId] = message;
        
        qint64 estimatedMemory = estimateMemoryUsage(mediaFile.filePath, mediaFile.type);
        registerTaskContext(task.taskId, message.id, sessionId, mediaFile.id, estimatedMemory);

        emit taskAdded(task.taskId);
    }

    updateQueuePositions();
    emit queueSizeChanged();

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
        QueueTask* nextTask = nullptr;
        for (auto& task : m_tasks) {
            if (task.status == ProcessingStatus::Pending) {
                nextTask = &task;
                break;
            }
        }

        if (!nextTask) {
            break;
        }

        if (!tryStartTask(*nextTask)) {
            break;
        }
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

    QString taskId = task.taskId;
    
    // 检查是否为 AI 推理模式
    if (m_taskMessages.contains(taskId)) {
        const Message& message = m_taskMessages[taskId];
        if (message.mode == ProcessingMode::AIInference) {
            // AI 推理模式：使用 AIEngine 处理
            QString modelId = message.aiParams.modelId;
            if (modelId.isEmpty()) {
                failTask(taskId, tr("未指定AI模型"));
                return;
            }

            // 找到对应的媒体文件
            QString inputPath;
            for (const auto& file : message.mediaFiles) {
                for (const auto& t : m_tasks) {
                    if (t.taskId == taskId && t.fileId == file.id) {
                        inputPath = file.filePath;
                        break;
                    }
                }
                if (!inputPath.isEmpty()) break;
            }

            if (inputPath.isEmpty()) {
                failTask(taskId, tr("未找到媒体文件"));
                return;
            }

            // 生成输出路径
            QFileInfo fileInfo(inputPath);
            QString outputPath = fileInfo.absolutePath() + "/" +
                                 fileInfo.completeBaseName() + "_ai_enhanced." + fileInfo.suffix();

            // 加载模型并推理
            if (!m_aiEngine->loadModel(modelId)) {
                failTask(taskId, tr("模型加载失败: %1").arg(modelId));
                return;
            }

            // 设置参数
            m_aiEngine->setUseGpu(message.aiParams.useGpu);
            if (message.aiParams.tileSize > 0) {
                m_aiEngine->setParameter("tileSize", message.aiParams.tileSize);
            }
            for (auto it = message.aiParams.modelParams.begin();
                 it != message.aiParams.modelParams.end(); ++it) {
                m_aiEngine->setParameter(it.key(), it.value());
            }

            // 异步推理
            m_aiEngine->processAsync(inputPath, outputPath);
            return;
        }
    }

    // Shader 模式使用 GPU 实时渲染，无需预处理
    QTimer::singleShot(10, this, [this, taskId]() {
        completeTask(taskId, "");
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
                cleanupTask(taskId);
                emit taskCancelled(taskId);
            } else if (m_taskCoordinator->isOrphaned(taskId)) {
                handleOrphanedTask(taskId);
                return;
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
                            
                            if (m_taskContexts.contains(taskId)) {
                                pending.estimatedMemoryMB = m_taskContexts[taskId].estimatedMemoryMB;
                                pending.estimatedGpuMemoryMB = m_taskContexts[taskId].estimatedGpuMemoryMB;
                            }
                            
                            m_pendingExports[taskId] = pending;
                            emit requestShaderExport(taskId, mf.filePath, pending.shaderParams, processedPath);
                        } else if (msg.mode == ProcessingMode::Shader && ImageUtils::isVideoFile(mf.filePath)) {
                            QString processedDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/processed";
                            QDir().mkpath(processedDir);
                            
                            QString tempFramePath = processedDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "_frame.png";
                            QImage firstFrame = ImageUtils::generateVideoThumbnail(mf.filePath, QSize(1920, 1080));
                            
                            if (!firstFrame.isNull() && firstFrame.save(tempFramePath)) {
                                QString processedThumbPath = processedDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "_thumb.png";
                                
                                PendingExport pending;
                                pending.taskId = taskId;
                                pending.messageId = messageId;
                                pending.fileId = fileId;
                                pending.originalPath = mf.filePath;
                                pending.outputPath = processedThumbPath;
                                pending.shaderParams = shaderParamsToVariantMap(msg.shaderParams);
                                pending.isVideoThumbnail = true;
                                pending.tempFramePath = tempFramePath;
                                
                                if (m_taskContexts.contains(taskId)) {
                                    pending.estimatedMemoryMB = m_taskContexts[taskId].estimatedMemoryMB;
                                    pending.estimatedGpuMemoryMB = m_taskContexts[taskId].estimatedGpuMemoryMB;
                                }
                                
                                m_pendingExports[taskId] = pending;
                                emit requestShaderExport(taskId, tempFramePath, pending.shaderParams, processedThumbPath);
                            } else {
                                if (m_messageModel) {
                                m_messageModel->updateFileStatus(messageId, fileId,
                                    static_cast<int>(ProcessingStatus::Completed), mf.filePath);
                            }
                            cleanupTask(taskId);
                            m_currentProcessingCount--;
                            emit currentProcessingCountChanged();
                            m_processingModel->updateTasks(m_tasks);
                            syncMessageProgress(messageId);
                            syncMessageStatus(messageId);
                            if (m_sessionController) {
                                m_sessionController->syncCurrentMessagesToSession();
                            }
                            processNextTask();
                            }
                        } else {
                            if (m_messageModel) {
                            m_messageModel->updateFileStatus(messageId, fileId,
                                static_cast<int>(ProcessingStatus::Completed), mf.filePath);
                        }
                        
                        cleanupTask(taskId);
                        m_currentProcessingCount--;
                        emit currentProcessingCountChanged();
                        m_processingModel->updateTasks(m_tasks);
                        
                        syncMessageProgress(messageId);
                        syncMessageStatus(messageId);
                        if (m_sessionController) {
                            m_sessionController->syncCurrentMessagesToSession();
                        }
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
    if (!m_pendingExports.contains(exportId)) {
        return;
    }
    
    PendingExport pending = m_pendingExports.take(exportId);
    
    if (pending.isVideoThumbnail && !pending.tempFramePath.isEmpty()) {
        QFile::remove(pending.tempFramePath);
    }
    
    if (success) {
        QImage processedThumb = ImageUtils::generateThumbnail(outputPath, QSize(512, 512));
        QString thumbId = "processed_" + pending.fileId;
        if (ThumbnailProvider::instance() && !processedThumb.isNull()) {
            ThumbnailProvider::instance()->setThumbnail(thumbId, processedThumb);
        }
        
        if (m_messageModel && !m_taskCoordinator->isOrphaned(exportId)) {
            QString resultPath = pending.isVideoThumbnail ? pending.originalPath : outputPath;
            m_messageModel->updateFileStatus(pending.messageId, pending.fileId,
                static_cast<int>(ProcessingStatus::Completed), resultPath);
        }
        
        if (pending.isVideoThumbnail) {
            QFile::remove(outputPath);
        }
    } else {
        if (m_messageModel && !m_taskCoordinator->isOrphaned(exportId)) {
            m_messageModel->updateFileStatus(pending.messageId, pending.fileId,
                static_cast<int>(ProcessingStatus::Failed), QString());
            m_messageModel->updateErrorMessage(pending.messageId, error);
        }
    }
    
    cleanupTask(exportId);
    m_currentProcessingCount--;
    emit currentProcessingCountChanged();
    m_processingModel->updateTasks(m_tasks);
    
    syncMessageProgress(pending.messageId);
    syncMessageStatus(pending.messageId);
    if (m_sessionController) {
        m_sessionController->syncCurrentMessagesToSession();
    }
    
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
            
            cleanupTask(taskId);
            m_currentProcessingCount--;
            emit currentProcessingCountChanged();
            m_processingModel->updateTasks(m_tasks);

            if (m_messageModel && !m_taskCoordinator->isOrphaned(taskId)) {
                m_messageModel->updateFileStatus(messageId, fileId,
                    static_cast<int>(ProcessingStatus::Failed), QString());
                m_messageModel->updateErrorMessage(messageId, error);
            }

            syncMessageProgress(messageId);
            syncMessageStatus(messageId);

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
        return QString();
    }

    QList<MediaFile> files = m_fileController->getAllFiles();
    if (files.isEmpty()) {
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

    // 设置 AI 参数
    if (mode == 1) {
        message.aiParams.modelId = params.value("modelId").toString();
        message.aiParams.useGpu = params.value("useGpu", true).toBool();
        message.aiParams.tileSize = params.value("tileSize", 0).toInt();

        QString categoryStr = params.value("category").toString();
        if (!categoryStr.isEmpty()) {
            static const QMap<QString, ModelCategory> catMap = {
                {"super_resolution", ModelCategory::SuperResolution},
                {"denoising", ModelCategory::Denoising},
                {"deblurring", ModelCategory::Deblurring},
                {"dehazing", ModelCategory::Dehazing},
                {"colorization", ModelCategory::Colorization},
                {"low_light", ModelCategory::LowLight},
                {"frame_interpolation", ModelCategory::FrameInterpolation},
                {"inpainting", ModelCategory::Inpainting}
            };
            message.aiParams.category = catMap.value(categoryStr, ModelCategory::SuperResolution);
        }

        // 收集模型特定参数
        QVariantMap modelParams;
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (it.key() != "modelId" && it.key() != "useGpu" &&
                it.key() != "tileSize" && it.key() != "category" &&
                it.key() != "modelIndex") {
                modelParams[it.key()] = it.value();
            }
        }
        message.aiParams.modelParams = modelParams;
    }

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

    int queuePos = 0;
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && task.status == ProcessingStatus::Pending) {
            queuePos = task.position + 1;
            break;
        }
    }
    m_messageModel->updateQueuePosition(messageId, queuePos);
}

int ProcessingController::resourcePressure() const
{
    return m_resourcePressure;
}

void ProcessingController::cancelMessageTasks(const QString& messageId)
{
    m_taskCoordinator->cancelMessageTasks(messageId);
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].messageId == messageId) {
            if (m_tasks[i].status == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(m_tasks[i].taskId);
                m_tasks.removeAt(i);
            } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                gracefulCancel(m_tasks[i].taskId);
            }
        }
    }
    
    updateQueuePositions();
    m_processingModel->updateTasks(m_tasks);
    emit queueSizeChanged();
    
    if (m_messageModel) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Cancelled));
    }
    
    emit messageTasksCancelled(messageId);
    processNextTask();
}

void ProcessingController::cancelSessionTasks(const QString& sessionId)
{
    m_taskCoordinator->cancelSessionTasks(sessionId);
    
    QSet<QString> messageIds;
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        QString taskId = m_tasks[i].taskId;
        TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
        
        if (ctx.sessionId == sessionId) {
            messageIds.insert(m_tasks[i].messageId);
            
            if (m_tasks[i].status == ProcessingStatus::Pending) {
                m_tasks[i].status = ProcessingStatus::Cancelled;
                cleanupTask(taskId);
                m_tasks.removeAt(i);
            } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                gracefulCancel(taskId);
            }
        }
    }
    
    updateQueuePositions();
    m_processingModel->updateTasks(m_tasks);
    emit queueSizeChanged();
    
    for (const QString& messageId : messageIds) {
        if (m_messageModel) {
            m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Cancelled));
        }
    }
    
    emit sessionTasksCancelled(sessionId);
    processNextTask();
}

bool ProcessingController::waitForMessageCancellation(const QString& messageId, int timeoutMs)
{
    return m_taskCoordinator->waitForAllMessageTasksCancelled(messageId, timeoutMs);
}

bool ProcessingController::waitForSessionCancellation(const QString& sessionId, int timeoutMs)
{
    return m_taskCoordinator->waitForAllSessionTasksCancelled(sessionId, timeoutMs);
}

void ProcessingController::pauseSessionTasks(const QString& sessionId)
{
    QSet<QString> taskIds = m_taskCoordinator->sessionTaskIds(sessionId);
    for (const QString& taskId : taskIds) {
        m_taskCoordinator->triggerPause(taskId, true);
    }
}

void ProcessingController::resumeSessionTasks(const QString& sessionId)
{
    QSet<QString> taskIds = m_taskCoordinator->sessionTaskIds(sessionId);
    for (const QString& taskId : taskIds) {
        m_taskCoordinator->triggerPause(taskId, false);
    }
}

void ProcessingController::onSessionChanging(const QString& oldSessionId, const QString& newSessionId)
{
    Q_UNUSED(newSessionId)
    
    QSet<QString> taskIds = m_taskCoordinator->sessionTaskIds(oldSessionId);
    for (const QString& taskId : taskIds) {
        TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
        ctx.priority = TaskPriority::Background;
        m_taskCoordinator->updateTaskState(taskId, ctx.state);
    }
}

void ProcessingController::setMaxConcurrentTasks(int max)
{
    if (m_maxConcurrentTasks != max) {
        m_maxConcurrentTasks = max;
        m_threadPool->setMaxThreadCount(max);
        m_resourceManager->setMaxConcurrentTasks(max);
        emit maxConcurrentTasksChanged();
    }
}

void ProcessingController::setResourceQuota(const ResourceQuota& quota)
{
    m_resourceManager->setQuota(quota);
    m_maxConcurrentTasks = quota.maxConcurrentTasks;
    m_threadPool->setMaxThreadCount(m_maxConcurrentTasks);
    emit maxConcurrentTasksChanged();
}

void ProcessingController::onTaskOrphaned(const QString& taskId)
{
    handleOrphanedTask(taskId);
}

void ProcessingController::onResourcePressureChanged(int pressureLevel)
{
    m_resourcePressure = pressureLevel;
    emit resourcePressureChanged();
    
    if (pressureLevel == 2) {
        pauseQueue();
    } else if (pressureLevel == 0 && m_queueStatus == QueueStatus::Paused) {
        resumeQueue();
    }
}

bool ProcessingController::tryStartTask(QueueTask& task)
{
    if (m_taskCoordinator->isOrphaned(task.taskId)) {
        handleOrphanedTask(task.taskId);
        return false;
    }
    
    TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
    
    if (!m_resourceManager->canStartNewTask(ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB)) {
        return false;
    }
    
    if (!m_resourceManager->tryAcquire(task.taskId, ctx.estimatedMemoryMB, ctx.estimatedGpuMemoryMB)) {
        return false;
    }
    
    startTask(task);
    return true;
}

void ProcessingController::gracefulCancel(const QString& taskId, int timeoutMs)
{
    m_taskCoordinator->triggerCancellation(taskId);
    
    if (!m_taskCoordinator->waitForCancellation(taskId, timeoutMs)) {
        qWarning() << "Task" << taskId << "did not cancel gracefully within timeout";
    }
    
    cleanupTask(taskId);
}

void ProcessingController::handleOrphanedTask(const QString& taskId)
{
    cleanupTask(taskId);
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].taskId == taskId) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            m_tasks.removeAt(i);
            break;
        }
    }
    
    updateQueuePositions();
    m_processingModel->updateTasks(m_tasks);
    emit queueSizeChanged();
}

void ProcessingController::cleanupTask(const QString& taskId)
{
    m_resourceManager->release(taskId);
    m_taskCoordinator->unregisterTask(taskId);
    m_pendingExports.remove(taskId);
    m_taskContexts.remove(taskId);
}

qint64 ProcessingController::estimateMemoryUsage(const QString& filePath, MediaType type) const
{
    return m_resourceManager->estimateMemoryUsage(filePath, type);
}

void ProcessingController::registerTaskContext(const QString& taskId, const QString& messageId,
                                                const QString& sessionId, const QString& fileId,
                                                qint64 estimatedMemoryMB)
{
    TaskContext ctx(taskId, messageId, sessionId, fileId);
    ctx.estimatedMemoryMB = estimatedMemoryMB;
    ctx.state = TaskState::Pending;
    
    m_taskContexts[taskId] = ctx;
    m_taskCoordinator->registerTask(ctx);
}

} // namespace EnhanceVision
