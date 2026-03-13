/**
 * @file ProcessingModel.h
 * @brief 处理状态模型
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PROCESSINGMODEL_H
#define ENHANCEVISION_PROCESSINGMODEL_H

#include <QObject>
#include <QAbstractListModel>
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
    void updateTaskProgress(const QString& taskId, int progress);
    void updateTaskStatus(const QString& taskId, ProcessingStatus status);
    void removeTask(const QString& taskId);

signals:
    void tasksChanged();

private:
    QList<QueueTask> m_tasks;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGMODEL_H
