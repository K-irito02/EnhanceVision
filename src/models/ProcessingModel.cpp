/**
 * @file ProcessingModel.cpp
 * @brief 处理状态模型实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/models/ProcessingModel.h"

namespace EnhanceVision {

ProcessingModel::ProcessingModel(QObject *parent)
    : QObject(parent)
{
}

ProcessingModel::~ProcessingModel()
{
}

void ProcessingModel::addTask(const QueueTask& task)
{
    m_tasks.append(task);
    emit tasksChanged();
}

void ProcessingModel::updateTasks(const QList<QueueTask>& tasks)
{
    m_tasks = tasks;
    emit tasksChanged();
}

void ProcessingModel::updateTaskProgress(const QString& taskId, int progress)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.progress = progress;
            emit tasksChanged();
            break;
        }
    }
}

void ProcessingModel::updateTaskStatus(const QString& taskId, ProcessingStatus status)
{
    for (auto& task : m_tasks) {
        if (task.taskId == taskId) {
            task.status = status;
            emit tasksChanged();
            break;
        }
    }
}

void ProcessingModel::removeTask(const QString& taskId)
{
    m_tasks.removeIf([&taskId](const QueueTask& task) {
        return task.taskId == taskId;
    });
    emit tasksChanged();
}

} // namespace EnhanceVision
