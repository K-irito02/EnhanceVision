/**
 * @file PriorityTaskQueue.h
 * @brief 优先级任务队列 - 多级桶 + O(1) 优先级访问，替代线性扫描
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PRIORITYTASKQUEUE_H
#define ENHANCEVISION_PRIORITYTASKQUEUE_H

#include <QHash>
#include <QQueue>
#include <QList>
#include <QString>
#include <QDateTime>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

struct PriorityTaskEntry {
    QString taskId;
    QString sessionId;
    QString messageId;
    TaskPriority priority = TaskPriority::UserInitiated;
    ProcessingMode mode = ProcessingMode::Shader;
    QDateTime enqueuedAt;
};

class PriorityTaskQueue
{
public:
    PriorityTaskQueue();

    void enqueue(const PriorityTaskEntry& entry);
    PriorityTaskEntry dequeue();
    PriorityTaskEntry peek() const;

    void updatePriority(const QString& taskId, TaskPriority newPriority);
    void remove(const QString& taskId);
    bool contains(const QString& taskId) const;

    void promoteSessionTasks(const QString& sessionId, TaskPriority newPriority);
    void demoteSessionTasks(const QString& sessionId, TaskPriority newPriority);

    bool isEmpty() const;
    int size() const;
    int sizeByPriority(TaskPriority priority) const;

    QList<PriorityTaskEntry> itemsByPriority(TaskPriority priority) const;
    QList<PriorityTaskEntry> allItems() const;

    PriorityTaskEntry entryForTask(const QString& taskId) const;

    void clear();

private:
    static constexpr int kPriorityCount = 4;
    static int priorityIndex(TaskPriority p);

    QQueue<PriorityTaskEntry> m_buckets[kPriorityCount];
    QHash<QString, TaskPriority> m_taskPriorities;
    int m_totalSize = 0;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PRIORITYTASKQUEUE_H
