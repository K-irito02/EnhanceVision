/**
 * @file ProcessingModel.h
 * @brief 处理状态模型
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PROCESSINGMODEL_H
#define ENHANCEVISION_PROCESSINGMODEL_H

#include <QObject>
#include <QAbstractListModel>

class QTimer;
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class ProcessingModel : public QObject
{
    Q_OBJECT

public:
    explicit ProcessingModel(QObject *parent = nullptr);
    ~ProcessingModel() override;

    void addTask(const QueueTask& task);
    void updateTasks(const QList<QueueTask>& tasks);
    void updateTask(const QueueTask& task);
    void updateTaskProgress(const QString& taskId, int progress);
    void updateTaskStatus(const QString& taskId, ProcessingStatus status);
    void removeTask(const QString& taskId);

signals:
    void tasksChanged();

private:
    void scheduleTasksChanged();

    QList<QueueTask> m_tasks;
    QTimer* m_tasksChangedTimer = nullptr;
    bool m_tasksChangedPending = false;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGMODEL_H
