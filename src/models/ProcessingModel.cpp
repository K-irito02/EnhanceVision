/**
 * @file ProcessingModel.cpp
 * @brief 处理状态模型实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/models/ProcessingModel.h"
#include <QTimer>

namespace EnhanceVision {

ProcessingModel::ProcessingModel(QObject *parent)
    : QObject(parent)
    , m_tasksChangedTimer(new QTimer(this))
{
    m_tasksChangedTimer->setSingleShot(true);
    m_tasksChangedTimer->setInterval(66);
    connect(m_tasksChangedTimer, &QTimer::timeout, this, [this]() {
        if (!m_tasksChangedPending) {
            return;
        }

        m_tasksChangedPending = false;
        emit tasksChanged();
    });
}

ProcessingModel::~ProcessingModel()
{
}

void ProcessingModel::scheduleTasksChanged()
{
    m_tasksChangedPending = true;
    if (m_tasksChangedTimer) {
        m_tasksChangedTimer->start();
    }
}

void ProcessingModel::addTask(const QueueTask& task)
{
    m_tasks.append(task);
    scheduleTasksChanged();
}

void ProcessingModel::updateTasks(const QList<QueueTask>& tasks)
{
    m_tasks = tasks;
    scheduleTasksChanged();
}

void ProcessingModel::updateTask(const QueueTask& task)
{
    for (auto& existingTask : m_tasks) {
        if (existingTask.taskId == task.taskId) {
            existingTask = task;
            scheduleTasksChanged();
            return;
        }
    }
}

void ProcessingModel::updateTaskProgress(const QString& taskId, int progress)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.progress = progress;
            scheduleTasksChanged();
            break;
        }
    }
}

void ProcessingModel::updateTaskStatus(const QString& taskId, ProcessingStatus status)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.status = status;
            scheduleTasksChanged();
            break;
        }
    }
}

void ProcessingModel::removeTask(const QString& taskId)
{
    m_tasks.removeIf([&taskId](const QueueTask& task) {
        return task.taskId == taskId;
    });
    scheduleTasksChanged();
}

} // namespace EnhanceVision
