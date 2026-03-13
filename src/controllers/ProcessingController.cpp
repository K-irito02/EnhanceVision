/**
 * @file ProcessingController.cpp
 * @brief 处理控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/FileController.h"
#include <QUuid>
#include <QDebug>
#include <QTimer>
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

void ProcessingController::cancelTask(const QString& taskId)
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            if (m_tasks[i].status == ProcessingStatus::Pending) {
                // 待处理任务直接移除
                m_tasks[i].status = ProcessingStatus::Cancelled;
                m_tasks.removeAt(i);
                updateQueuePositions();
                emit taskCancelled(taskId);
                emit queueSizeChanged();
            } else if (m_tasks[i].status == ProcessingStatus::Processing) {
                // 处理中的任务发送取消信号
                m_tasks[i].status = ProcessingStatus::Cancelled;
                emit taskCancelled(taskId);
            }
            m_processingModel->updateTasks(m_tasks);
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

    // 模拟任务处理（实际项目中这里会调用真实的处理引擎）
    QTimer::singleShot(100, this, [this, taskId = task.taskId]() {
        // 模拟进度更新
        QTimer* progressTimer = new QTimer(this);
        int progress = 0;

        connect(progressTimer, &QTimer::timeout, this, [this, progressTimer, taskId, &progress]() {
            progress += 10;
            if (progress >= 100) {
                progressTimer->stop();
                progressTimer->deleteLater();
                completeTask(taskId, "");
            } else {
                updateTaskProgress(taskId, progress);
            }
        });

        progressTimer->start(500);
    });
}

void ProcessingController::updateTaskProgress(const QString& taskId, int progress)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.progress = progress;
            emit taskProgress(taskId, progress);
            m_processingModel->updateTasks(m_tasks);
            break;
        }
    }
}

void ProcessingController::completeTask(const QString& taskId, const QString& resultPath)
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].taskId == taskId) {
            if (m_tasks[i].status == ProcessingStatus::Cancelled) {
                // 任务已被取消
                emit taskCancelled(taskId);
            } else {
                m_tasks[i].status = ProcessingStatus::Completed;
                m_tasks[i].progress = 100;
                emit taskCompleted(taskId, resultPath);
            }

            m_currentProcessingCount--;
            emit currentProcessingCountChanged();
            m_processingModel->updateTasks(m_tasks);

            // 继续处理下一个任务
            processNextTask();
            break;
        }
    }
}

void ProcessingController::failTask(const QString& taskId, const QString& error)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.status = ProcessingStatus::Failed;
            emit taskFailed(taskId, error);
            m_currentProcessingCount--;
            emit currentProcessingCountChanged();
            m_processingModel->updateTasks(m_tasks);

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

    // 设置 Shader 参数
    if (mode == 0) {
        message.shaderParams.brightness = params.value("brightness", 0.0f).toFloat();
        message.shaderParams.contrast = params.value("contrast", 1.0f).toFloat();
        message.shaderParams.saturation = params.value("saturation", 1.0f).toFloat();
        message.shaderParams.sharpness = params.value("sharpness", 0.0f).toFloat();
        message.shaderParams.denoise = params.value("denoise", 0.0f).toFloat();
    }

    qDebug() << "Sending message to processing queue:" << message.id;
    return addTask(message);
}

} // namespace EnhanceVision
