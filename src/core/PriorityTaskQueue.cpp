/**
 * @file PriorityTaskQueue.cpp
 * @brief 优先级任务队列实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/PriorityTaskQueue.h"

namespace EnhanceVision {

PriorityTaskQueue::PriorityTaskQueue() = default;

int PriorityTaskQueue::priorityIndex(TaskPriority p)
{
    int idx = static_cast<int>(p);
    if (idx < 0) idx = 0;
    if (idx >= kPriorityCount) idx = kPriorityCount - 1;
    return idx;
}

void PriorityTaskQueue::enqueue(const PriorityTaskEntry& entry)
{
    if (m_taskPriorities.contains(entry.taskId)) {
        remove(entry.taskId);
    }
    int idx = priorityIndex(entry.priority);
    m_buckets[idx].enqueue(entry);
    m_taskPriorities[entry.taskId] = entry.priority;
    m_totalSize++;
}

PriorityTaskEntry PriorityTaskQueue::dequeue()
{
    for (int i = 0; i < kPriorityCount; ++i) {
        if (!m_buckets[i].isEmpty()) {
            PriorityTaskEntry entry = m_buckets[i].dequeue();
            m_taskPriorities.remove(entry.taskId);
            m_totalSize--;
            return entry;
        }
    }
    return {};
}

PriorityTaskEntry PriorityTaskQueue::peek() const
{
    for (int i = 0; i < kPriorityCount; ++i) {
        if (!m_buckets[i].isEmpty()) {
            return m_buckets[i].head();
        }
    }
    return {};
}

void PriorityTaskQueue::updatePriority(const QString& taskId, TaskPriority newPriority)
{
    if (!m_taskPriorities.contains(taskId)) {
        return;
    }

    TaskPriority oldPriority = m_taskPriorities[taskId];
    if (oldPriority == newPriority) {
        return;
    }

    int oldIdx = priorityIndex(oldPriority);
    PriorityTaskEntry found;
    bool removed = false;

    for (int j = 0; j < m_buckets[oldIdx].size(); ++j) {
        if (m_buckets[oldIdx].at(j).taskId == taskId) {
            found = m_buckets[oldIdx].at(j);
            m_buckets[oldIdx].removeAt(j);
            removed = true;
            break;
        }
    }

    if (removed) {
        found.priority = newPriority;
        int newIdx = priorityIndex(newPriority);
        m_buckets[newIdx].enqueue(found);
        m_taskPriorities[taskId] = newPriority;
    }
}

void PriorityTaskQueue::remove(const QString& taskId)
{
    if (!m_taskPriorities.contains(taskId)) {
        return;
    }

    TaskPriority priority = m_taskPriorities[taskId];
    int idx = priorityIndex(priority);

    for (int j = 0; j < m_buckets[idx].size(); ++j) {
        if (m_buckets[idx].at(j).taskId == taskId) {
            m_buckets[idx].removeAt(j);
            m_taskPriorities.remove(taskId);
            m_totalSize--;
            return;
        }
    }

    m_taskPriorities.remove(taskId);
    m_totalSize--;
}

bool PriorityTaskQueue::contains(const QString& taskId) const
{
    return m_taskPriorities.contains(taskId);
}

void PriorityTaskQueue::promoteSessionTasks(const QString& sessionId, TaskPriority newPriority)
{
    for (int i = 0; i < kPriorityCount; ++i) {
        QQueue<PriorityTaskEntry> kept;
        QList<PriorityTaskEntry> toMove;

        while (!m_buckets[i].isEmpty()) {
            PriorityTaskEntry entry = m_buckets[i].dequeue();
            if (entry.sessionId == sessionId && static_cast<int>(entry.priority) > static_cast<int>(newPriority)) {
                entry.priority = newPriority;
                toMove.append(entry);
            } else {
                kept.enqueue(entry);
            }
        }
        m_buckets[i] = kept;

        int targetIdx = priorityIndex(newPriority);
        for (const auto& entry : toMove) {
            m_buckets[targetIdx].enqueue(entry);
            m_taskPriorities[entry.taskId] = newPriority;
        }
    }
}

void PriorityTaskQueue::demoteSessionTasks(const QString& sessionId, TaskPriority newPriority)
{
    for (int i = 0; i < kPriorityCount; ++i) {
        QQueue<PriorityTaskEntry> kept;
        QList<PriorityTaskEntry> toMove;

        while (!m_buckets[i].isEmpty()) {
            PriorityTaskEntry entry = m_buckets[i].dequeue();
            if (entry.sessionId == sessionId && static_cast<int>(entry.priority) < static_cast<int>(newPriority)) {
                entry.priority = newPriority;
                toMove.append(entry);
            } else {
                kept.enqueue(entry);
            }
        }
        m_buckets[i] = kept;

        int targetIdx = priorityIndex(newPriority);
        for (const auto& entry : toMove) {
            m_buckets[targetIdx].enqueue(entry);
            m_taskPriorities[entry.taskId] = newPriority;
        }
    }
}

bool PriorityTaskQueue::isEmpty() const
{
    return m_totalSize <= 0;
}

int PriorityTaskQueue::size() const
{
    return m_totalSize;
}

int PriorityTaskQueue::sizeByPriority(TaskPriority priority) const
{
    return m_buckets[priorityIndex(priority)].size();
}

QList<PriorityTaskEntry> PriorityTaskQueue::itemsByPriority(TaskPriority priority) const
{
    const auto& bucket = m_buckets[priorityIndex(priority)];
    QList<PriorityTaskEntry> result;
    result.reserve(bucket.size());
    for (const auto& entry : bucket) {
        result.append(entry);
    }
    return result;
}

QList<PriorityTaskEntry> PriorityTaskQueue::allItems() const
{
    QList<PriorityTaskEntry> result;
    result.reserve(m_totalSize);
    for (int i = 0; i < kPriorityCount; ++i) {
        for (const auto& entry : m_buckets[i]) {
            result.append(entry);
        }
    }
    return result;
}

PriorityTaskEntry PriorityTaskQueue::entryForTask(const QString& taskId) const
{
    if (!m_taskPriorities.contains(taskId)) {
        return {};
    }
    TaskPriority priority = m_taskPriorities[taskId];
    int idx = priorityIndex(priority);
    for (const auto& entry : m_buckets[idx]) {
        if (entry.taskId == taskId) {
            return entry;
        }
    }
    return {};
}

void PriorityTaskQueue::clear()
{
    for (int i = 0; i < kPriorityCount; ++i) {
        m_buckets[i].clear();
    }
    m_taskPriorities.clear();
    m_totalSize = 0;
}

} // namespace EnhanceVision
