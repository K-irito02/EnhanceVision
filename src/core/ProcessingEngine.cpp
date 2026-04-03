/**
 * @file ProcessingEngine.cpp
 * @brief 处理引擎调度器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ProcessingEngine.h"
#include <QCoreApplication>
#include <QDir>
#include <QUuid>

namespace EnhanceVision {

ProcessingEngine* ProcessingEngine::s_instance = nullptr;

ProcessingEngine::ProcessingEngine(QObject *parent)
    : QObject(parent)
    , m_queueStatus(QueueStatus::Running)
    , m_maxConcurrentTasks(2)
{
    m_imageProcessor = new ImageProcessor(this);
    m_videoProcessor = new VideoProcessor(this);
    m_aiEngine = new AIEngine(this);
    m_modelRegistry = new ModelRegistry(this);

    // 初始化模型注册表
    QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
    if (!QDir(modelsPath).exists()) {
        modelsPath = QCoreApplication::applicationDirPath() + "/../resources/models";
    }
    if (!QDir(modelsPath).exists()) {
        modelsPath = QCoreApplication::applicationDirPath() + "/resources/models";
    }
    if (!QDir(modelsPath).exists()) {
        // 开发模式：从源码目录查找
        modelsPath = QDir(QCoreApplication::applicationDirPath()).filePath("../../resources/models");
    }
    m_modelRegistry->initialize(modelsPath);
    m_aiEngine->setModelRegistry(m_modelRegistry);

    // 连接图像处理信号
    connect(m_imageProcessor, &ImageProcessor::progressChanged,
            this, &ProcessingEngine::onImageProgressChanged);
    connect(m_imageProcessor, &ImageProcessor::finished,
            this, &ProcessingEngine::onImageFinished);

    // 连接视频处理信号
    connect(m_videoProcessor, &VideoProcessor::progressChanged,
            this, &ProcessingEngine::onVideoProgressChanged);
    connect(m_videoProcessor, &VideoProcessor::finished,
            this, &ProcessingEngine::onVideoFinished);

    // 连接 AI 推理信号
    connect(m_aiEngine, &AIEngine::progressChanged,
            this, &ProcessingEngine::onAIProgressChanged);
    connect(m_aiEngine, &AIEngine::processFileCompleted,
            this, &ProcessingEngine::onAIFinished);
}

ProcessingEngine::~ProcessingEngine()
{
}

ProcessingEngine* ProcessingEngine::instance()
{
    if (!s_instance) {
        s_instance = new ProcessingEngine(QCoreApplication::instance());
    }
    return s_instance;
}

void ProcessingEngine::addTask(const QueueTask& task, const Message& message)
{
    QMutexLocker locker(&m_mutex);

    QueueTask newTask = task;
    if (newTask.taskId.isEmpty()) {
        newTask.taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    newTask.position = m_taskQueue.size() + m_processingTasks.size();
    newTask.queuedAt = QDateTime::currentDateTime();
    newTask.status = ProcessingStatus::Pending;

    m_taskQueue.enqueue(qMakePair(newTask, message));

    emit taskAdded(newTask);

    // 如果队列正在运行且有空闲槽位，开始处理下一个任务
    if (m_queueStatus == QueueStatus::Running && m_processingTasks.size() < m_maxConcurrentTasks) {
        QMetaObject::invokeMethod(this, "processNextTask", Qt::QueuedConnection);
    }
}

void ProcessingEngine::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    // 检查是否在处理中
    for (int i = 0; i < m_processingTasks.size(); ++i) {
        if (m_processingTasks[i].first.taskId == taskId) {
            // 正在处理中，取消
            if (m_imageProcessor->isProcessing() && m_currentTaskId == taskId) {
                m_imageProcessor->cancel();
            }
            if (m_videoProcessor->isProcessing() && m_currentTaskId == taskId) {
                m_videoProcessor->cancel();
            }

            QueueTask task = m_processingTasks[i].first;
            task.status = ProcessingStatus::Cancelled;
            emit taskCancelled(task);
            m_processingTasks.removeAt(i);

            // 继续处理下一个任务
            QMetaObject::invokeMethod(this, "processNextTask", Qt::QueuedConnection);
            return;
        }
    }

    // 检查是否在队列中
    QQueue<QPair<QueueTask, Message>> newQueue;
    while (!m_taskQueue.isEmpty()) {
        auto pair = m_taskQueue.dequeue();
        if (pair.first.taskId != taskId) {
            newQueue.enqueue(pair);
        } else {
            QueueTask task = pair.first;
            task.status = ProcessingStatus::Cancelled;
            emit taskCancelled(task);
        }
    }
    m_taskQueue = newQueue;

    // 更新队列位置
    for (int i = 0; i < m_taskQueue.size(); ++i) {
        m_taskQueue[i].first.position = i + m_processingTasks.size();
    }
}

void ProcessingEngine::pauseQueue()
{
    QMutexLocker locker(&m_mutex);
    if (m_queueStatus != QueueStatus::Paused) {
        m_queueStatus = QueueStatus::Paused;
        emit queueStatusChanged(m_queueStatus);
    }
}

void ProcessingEngine::resumeQueue()
{
    QMutexLocker locker(&m_mutex);
    if (m_queueStatus != QueueStatus::Running) {
        m_queueStatus = QueueStatus::Running;
        emit queueStatusChanged(m_queueStatus);

        // 开始处理下一个任务
        QMetaObject::invokeMethod(this, "processNextTask", Qt::QueuedConnection);
    }
}

QueueStatus ProcessingEngine::getQueueStatus() const
{
    QMutexLocker locker(&m_mutex);
    return m_queueStatus;
}

QList<QueueTask> ProcessingEngine::getQueuedTasks() const
{
    QMutexLocker locker(&m_mutex);
    QList<QueueTask> tasks;
    for (const auto& pair : m_taskQueue) {
        tasks.append(pair.first);
    }
    return tasks;
}

QueueTask ProcessingEngine::getCurrentTask() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_processingTasks.isEmpty()) {
        return m_processingTasks.first().first;
    }
    return QueueTask();
}

int ProcessingEngine::getMaxConcurrentTasks() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxConcurrentTasks;
}

void ProcessingEngine::setMaxConcurrentTasks(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxConcurrentTasks = qMax(1, count);
}

void ProcessingEngine::processNextTask()
{
    QMutexLocker locker(&m_mutex);

    if (m_queueStatus != QueueStatus::Running) {
        return;
    }

    if (m_taskQueue.isEmpty()) {
        return;
    }

    if (m_processingTasks.size() >= m_maxConcurrentTasks) {
        return;
    }

    auto pair = m_taskQueue.dequeue();
    pair.first.status = ProcessingStatus::Processing;
    pair.first.startedAt = QDateTime::currentDateTime();
    m_processingTasks.append(pair);

    locker.unlock();

    emit taskStarted(pair.first);

    // 开始处理任务
    startTask(pair.first, pair.second);
}

void ProcessingEngine::startTask(const QueueTask& task, const Message& message)
{
    m_currentTaskId = task.taskId;

    // 找到对应的媒体文件
    MediaFile mediaFile;
    for (const auto& file : message.mediaFiles) {
        if (file.id == task.fileId) {
            mediaFile = file;
            break;
        }
    }

    if (mediaFile.filePath.isEmpty()) {
        emit taskFinished(task, false, QString(), tr("未找到媒体文件"));
        return;
    }

    // 生成输出路径
    QString outputPath;
    QFileInfo fileInfo(mediaFile.filePath);
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.suffix();
    outputPath = fileInfo.absolutePath() + "/" + baseName + "_enhanced." + extension;

    if (message.mode == ProcessingMode::AIInference) {
        // AI 推理模式
        QString modelId = message.aiParams.modelId;
        if (modelId.isEmpty()) {
            emit taskFinished(task, false, QString(), tr("未指定AI模型"));
            return;
        }

        // 加载模型
        if (!m_aiEngine->loadModel(modelId)) {
            emit taskFinished(task, false, QString(), tr("模型加载失败: %1").arg(modelId));
            return;
        }

        // 设置模型参数
        for (auto it = message.aiParams.modelParams.begin();
             it != message.aiParams.modelParams.end(); ++it) {
            m_aiEngine->setParameter(it.key(), it.value());
        }

        // 异步推理
        m_aiEngine->processAsync(mediaFile.filePath, outputPath);
    } else if (mediaFile.type == MediaType::Image) {
        // Shader 图像处理
        m_imageProcessor->processImageAsync(
            mediaFile.filePath,
            outputPath,
            message.shaderParams
        );
    } else {
        // Shader 视频处理
        m_videoProcessor->processVideoAsync(
            mediaFile.filePath,
            outputPath,
            message.shaderParams
        );
    }
}

void ProcessingEngine::onImageProgressChanged(int progress, const QString& status)
{
    emit taskProgressChanged(m_currentTaskId, progress, status);
}

void ProcessingEngine::onImageFinished(bool success, const QString& resultPath, const QString& error)
{
    QMutexLocker locker(&m_mutex);

    // 找到对应的任务
    for (int i = 0; i < m_processingTasks.size(); ++i) {
        if (m_processingTasks[i].first.taskId == m_currentTaskId) {
            QueueTask task = m_processingTasks[i].first;
            task.status = success ? ProcessingStatus::Completed : ProcessingStatus::Failed;
            task.progress = success ? 100 : 0;

            emit taskFinished(task, success, resultPath, error);
            m_processingTasks.removeAt(i);
            break;
        }
    }

    m_currentTaskId.clear();

    locker.unlock();

    // 继续处理下一个任务
    processNextTask();
}

void ProcessingEngine::onVideoProgressChanged(int progress, const QString& status)
{
    emit taskProgressChanged(m_currentTaskId, progress, status);
}

void ProcessingEngine::onVideoFinished(bool success, const QString& resultPath, const QString& error)
{
    QMutexLocker locker(&m_mutex);

    // 找到对应的任务
    for (int i = 0; i < m_processingTasks.size(); ++i) {
        if (m_processingTasks[i].first.taskId == m_currentTaskId) {
            QueueTask task = m_processingTasks[i].first;
            task.status = success ? ProcessingStatus::Completed : ProcessingStatus::Failed;
            task.progress = success ? 100 : 0;

            emit taskFinished(task, success, resultPath, error);
            m_processingTasks.removeAt(i);
            break;
        }
    }

    m_currentTaskId.clear();

    locker.unlock();

    // 继续处理下一个任务
    processNextTask();
}

void ProcessingEngine::onAIProgressChanged(double progress)
{
    emit taskProgressChanged(m_currentTaskId, static_cast<int>(progress * 100), tr("AI推理中..."));
}

void ProcessingEngine::onAIFinished(bool success, const QString& resultPath, const QString& error)
{
    QMutexLocker locker(&m_mutex);

    for (int i = 0; i < m_processingTasks.size(); ++i) {
        if (m_processingTasks[i].first.taskId == m_currentTaskId) {
            QueueTask task = m_processingTasks[i].first;
            task.status = success ? ProcessingStatus::Completed : ProcessingStatus::Failed;
            task.progress = success ? 100 : 0;

            emit taskFinished(task, success, resultPath, error);
            m_processingTasks.removeAt(i);
            break;
        }
    }

    m_currentTaskId.clear();

    locker.unlock();

    processNextTask();
}

} // namespace EnhanceVision
