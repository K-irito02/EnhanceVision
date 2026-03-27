/**
 * @file AIEnginePool.cpp
 * @brief AI 推理引擎池实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/AIEnginePool.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include <QDebug>

namespace EnhanceVision {

AIEnginePool::AIEnginePool(int poolSize, ModelRegistry* registry, QObject* parent)
    : QObject(parent)
    , m_modelRegistry(registry)
    , m_poolSize(qMax(1, poolSize))
{
    createEngines(m_poolSize);
    qInfo() << "[AIEnginePool] created with pool size:" << m_poolSize;
}

AIEnginePool::~AIEnginePool()
{
    destroyEngines();
}

AIEngine* AIEnginePool::acquire(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (m_taskToSlot.contains(taskId)) {
        int idx = m_taskToSlot[taskId];
        return m_slots[idx].engine;
    }

    for (int i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].inUse) {
            m_slots[i].inUse = true;
            m_slots[i].taskId = taskId;
            m_taskToSlot[taskId] = i;

            qInfo() << "[AIEnginePool] engine acquired"
                    << "task:" << taskId
                    << "slot:" << i
                    << "available:" << availableCount();

            emit engineAcquired(taskId, i);
            return m_slots[i].engine;
        }
    }

    qWarning() << "[AIEnginePool] pool exhausted, no available engine for task:" << taskId;
    emit poolExhausted();
    return nullptr;
}

void AIEnginePool::release(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) {
        return;
    }

    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        m_slots[idx].inUse = false;
        m_slots[idx].taskId.clear();

        qInfo() << "[AIEnginePool] engine released"
                << "task:" << taskId
                << "slot:" << idx
                << "available:" << availableCount();

        emit engineReleased(taskId, idx);
    }
}

AIEngine* AIEnginePool::engineForTask(const QString& taskId) const
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) {
        return nullptr;
    }
    int idx = m_taskToSlot[taskId];
    if (idx >= 0 && idx < m_slots.size()) {
        return m_slots[idx].engine;
    }
    return nullptr;
}

int AIEnginePool::poolSize() const
{
    return m_poolSize;
}

int AIEnginePool::availableCount() const
{
    int count = 0;
    for (const auto& slot : m_slots) {
        if (!slot.inUse) {
            count++;
        }
    }
    return count;
}

int AIEnginePool::activeCount() const
{
    return m_poolSize - availableCount();
}

bool AIEnginePool::preloadModel(const QString& modelId)
{
    QMutexLocker locker(&m_mutex);

    bool allOk = true;
    for (auto& slot : m_slots) {
        if (!slot.inUse && slot.engine) {
            if (!slot.engine->loadModel(modelId)) {
                allOk = false;
            }
        }
    }
    return allOk;
}

void AIEnginePool::unloadAll()
{
    QMutexLocker locker(&m_mutex);

    for (auto& slot : m_slots) {
        if (slot.engine) {
            slot.engine->unloadModel();
        }
    }
}

void AIEnginePool::setPoolSize(int size)
{
    QMutexLocker locker(&m_mutex);

    int newSize = qMax(1, size);
    if (newSize == m_poolSize) {
        return;
    }

    if (newSize > m_poolSize) {
        for (int i = m_poolSize; i < newSize; ++i) {
            EngineSlot slot;
            slot.engine = new AIEngine(this);
            slot.engine->setModelRegistry(m_modelRegistry);
            m_slots.append(slot);
        }
    } else {
        for (int i = m_slots.size() - 1; i >= newSize; --i) {
            if (!m_slots[i].inUse) {
                delete m_slots[i].engine;
                m_slots.removeAt(i);
            }
        }
    }

    m_poolSize = m_slots.size();
    qInfo() << "[AIEnginePool] pool resized to:" << m_poolSize;
}

void AIEnginePool::warmupModel(const QString& modelId)
{
    QMutexLocker locker(&m_mutex);

    int warmedCount = 0;
    for (auto& slot : m_slots) {
        if (!slot.inUse && slot.engine && slot.engine->currentModelId() != modelId) {
            slot.engine->loadModelAsync(modelId);
            warmedCount++;
        }
    }

    if (warmedCount > 0) {
        qInfo() << "[AIEnginePool] warmupModel started for model:" << modelId
                << "engines:" << warmedCount;
    }
}

void AIEnginePool::createEngines(int count)
{
    m_slots.reserve(count);
    for (int i = 0; i < count; ++i) {
        EngineSlot slot;
        slot.engine = new AIEngine(this);
        slot.engine->setModelRegistry(m_modelRegistry);
        m_slots.append(slot);
    }
}

void AIEnginePool::destroyEngines()
{
    for (auto& slot : m_slots) {
        if (slot.engine) {
            slot.engine->cancelProcess();
        }
    }
    for (auto& slot : m_slots) {
        delete slot.engine;
        slot.engine = nullptr;
    }
    m_slots.clear();
    m_taskToSlot.clear();
}

} // namespace EnhanceVision
