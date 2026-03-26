/**
 * @file PriorityAdjuster.cpp
 * @brief 动态优先级调整器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/PriorityAdjuster.h"
#include <QDateTime>
#include <QDebug>

namespace EnhanceVision {

PriorityAdjuster::PriorityAdjuster(PriorityTaskQueue* queue, QObject* parent)
    : QObject(parent)
    , m_queue(queue)
    , m_checkTimer(new QTimer(this))
{
    m_checkTimer->setTimerType(Qt::CoarseTimer);
    connect(m_checkTimer, &QTimer::timeout, this, &PriorityAdjuster::performCheck);
}

PriorityAdjuster::~PriorityAdjuster()
{
    stop();
}

void PriorityAdjuster::setConfig(const Config& config)
{
    m_config = config;
    if (m_checkTimer->isActive()) {
        m_checkTimer->setInterval(m_config.checkIntervalMs);
    }
}

PriorityAdjuster::Config PriorityAdjuster::config() const
{
    return m_config;
}

void PriorityAdjuster::start()
{
    if (!m_checkTimer->isActive()) {
        m_checkTimer->start(m_config.checkIntervalMs);
    }
}

void PriorityAdjuster::stop()
{
    m_checkTimer->stop();
}

bool PriorityAdjuster::isRunning() const
{
    return m_checkTimer->isActive();
}

void PriorityAdjuster::onSessionChanged(const QString& activeSessionId)
{
    if (m_activeSessionId == activeSessionId) {
        return;
    }

    const QString oldSessionId = m_activeSessionId;
    m_activeSessionId = activeSessionId;

    if (!oldSessionId.isEmpty()) {
        m_queue->demoteSessionTasks(oldSessionId, TaskPriority::Utility);
    }
    if (!activeSessionId.isEmpty()) {
        m_queue->promoteSessionTasks(activeSessionId, TaskPriority::UserInteractive);
    }

    qInfo() << "[PriorityAdjuster] session changed"
            << "old:" << oldSessionId
            << "new:" << activeSessionId;
}

void PriorityAdjuster::performCheck()
{
    if (m_queue->isEmpty()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (int p = static_cast<int>(TaskPriority::Background); p > static_cast<int>(m_config.starvationPromoteTo); --p) {
        TaskPriority priority = static_cast<TaskPriority>(p);
        QList<PriorityTaskEntry> items = m_queue->itemsByPriority(priority);

        for (const auto& entry : items) {
            const qint64 waitMs = nowMs - entry.enqueuedAt.toMSecsSinceEpoch();
            if (waitMs >= m_config.starvationThresholdMs) {
                TaskPriority oldPriority = entry.priority;
                TaskPriority newPriority = static_cast<TaskPriority>(static_cast<int>(oldPriority) - 1);
                if (static_cast<int>(newPriority) < static_cast<int>(m_config.starvationPromoteTo)) {
                    newPriority = m_config.starvationPromoteTo;
                }

                m_queue->updatePriority(entry.taskId, newPriority);

                qInfo() << "[PriorityAdjuster] starvation promotion"
                        << "task:" << entry.taskId
                        << "from:" << static_cast<int>(oldPriority)
                        << "to:" << static_cast<int>(newPriority)
                        << "waited:" << waitMs << "ms";

                emit starvationDetected(entry.taskId, waitMs);
                emit taskPromoted(entry.taskId, oldPriority, newPriority);
            }
        }
    }
}

} // namespace EnhanceVision
