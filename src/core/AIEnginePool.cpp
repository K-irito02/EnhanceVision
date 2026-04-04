/**
 * @file AIEnginePool.cpp
 * @brief AI 推理引擎池实现（CPU / GPU 双模式）
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/AIEnginePool.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
#include "EnhanceVision/core/inference/InferenceTypes.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>

namespace EnhanceVision {

AIEnginePool::AIEnginePool(int poolSize, ModelRegistry* registry, QObject* parent)
    : QObject(parent)
    , m_modelRegistry(registry)
    , m_poolSize(qMax(1, poolSize))
{
    createEngines(m_poolSize);
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
        if (m_slots[i].state == EngineState::Ready || 
            m_slots[i].state == EngineState::Uninitialized) {
            
            if (!ensureEngineReady(i)) {
                qWarning() << "[AIEnginePool] Engine slot" << i << "not ready, skipping";
                continue;
            }

            // 确保引擎不在处理中（防止上一个任务取消后引擎仍在运行）
            AIEngine* engine = m_slots[i].engine;
            if (engine->isProcessing()) {
                qWarning() << "[AIEnginePool] Engine slot" << i << "still processing, skipping (will retry later)";
                // 不阻塞等待，直接跳过这个引擎，让调用方稍后重试
                continue;
            }

            engine->resetState();

            m_slots[i].state = EngineState::InUse;
            m_slots[i].taskId = taskId;
            m_taskToSlot[taskId] = i;
            
            qInfo() << "[AIEnginePool] Engine acquired for task:" << taskId
                    << ", slot index:" << i;

            emit engineAcquired(taskId, i);
            return engine;
        }
    }

    qWarning() << "[AIEnginePool] pool exhausted, no available engine for task:" << taskId;
    emit poolExhausted();
    return nullptr;
}

AIEngine* AIEnginePool::acquireWithBackend(const QString& taskId, BackendType backendType)
{
    QMutexLocker locker(&m_mutex);

    if (m_taskToSlot.contains(taskId)) {
        int idx = m_taskToSlot[taskId];
        return m_slots[idx].engine;
    }

    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].state == EngineState::Ready ||
            m_slots[i].state == EngineState::Uninitialized) {

            if (!ensureEngineReady(i)) {
                qWarning() << "[AIEnginePool] Engine slot" << i << "not ready, skipping";
                continue;
            }

            AIEngine* engine = m_slots[i].engine;
            if (engine->isProcessing()) {
                qWarning() << "[AIEnginePool] Engine slot" << i << "still processing, skipping";
                continue;
            }

            engine->resetState();

            qInfo() << "[AIEnginePool] Switching backend for slot" << i
                    << "from" << static_cast<int>(m_slots[i].backendType)
                    << "to" << static_cast<int>(backendType);

            if (!engine->setBackendType(backendType)) {
                qWarning() << "[AIEnginePool] Failed to set backend type for slot" << i
                           << ", requested:" << static_cast<int>(backendType);
                continue;
            }

            if (backendType == BackendType::NCNN_Vulkan) {
                if (!engine->waitForModelSyncComplete(10000)) {
                    qWarning() << "[AIEnginePool] Model sync timeout for slot" << i;
                    continue;
                }
                if (!engine->waitForGpuReady(5000)) {
                    qWarning() << "[AIEnginePool] GPU ready timeout for slot" << i;
                    continue;
                }
            }

            m_slots[i].state = EngineState::InUse;
            m_slots[i].taskId = taskId;
            m_slots[i].backendType = backendType;
            m_taskToSlot[taskId] = i;

            qInfo() << "[AIEnginePool] Engine acquired for task:" << taskId
                    << ", slot index:" << i
                    << ", backend:" << static_cast<int>(backendType);

            emit engineAcquired(taskId, i);
            return engine;
        }
    }

    qWarning() << "[AIEnginePool] pool exhausted, no available engine for task:" << taskId;
    emit poolExhausted();
    return nullptr;
}

bool AIEnginePool::ensureEngineReady(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= m_slots.size()) {
        return false;
    }

    EngineSlot& slot = m_slots[slotIndex];

    if (!slot.engine) {
        slot.state = EngineState::Error;
        slot.lastError = "Engine not created";
        return false;
    }
    
    if (slot.state == EngineState::Error) {
        qInfo() << "[AIEnginePool] Attempting to recover engine in slot" << slotIndex;
        slot.engine->resetState();
    }
    
    slot.state = EngineState::Ready;
    return true;
}

void AIEnginePool::release(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) {
        return;
    }

    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        AIEngine* engine = m_slots[idx].engine;
        
        // 释放前取消引擎处理，确保下次获取时引擎空闲
        if (engine && engine->isProcessing()) {
            qInfo() << "[AIEnginePool] Cancelling engine before release, task:" << taskId;
            engine->cancelProcess();
        }
        
        m_slots[idx].state = EngineState::Ready;
        m_slots[idx].taskId.clear();
        m_slots[idx].backendType = BackendType::NCNN_CPU;
        m_slots[idx].wasUsed = true;

        qInfo() << "[AIEnginePool] Task released:" << taskId << ", slot index:" << idx;
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

AIEngine* AIEnginePool::firstEngine() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_slots.isEmpty()) {
        return m_slots[0].engine;
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
        if (slot.state == EngineState::Ready || slot.state == EngineState::Uninitialized) {
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
        if (slot.state != EngineState::InUse && slot.engine) {
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
            if (m_slots[i].state != EngineState::InUse) {
                delete m_slots[i].engine;
                m_slots.removeAt(i);
            }
        }
    }

    m_poolSize = m_slots.size();
}

void AIEnginePool::warmupModel(const QString& modelId)
{
    QMutexLocker locker(&m_mutex);

    for (auto& slot : m_slots) {
        if (slot.state != EngineState::InUse && slot.engine && slot.engine->currentModelId() != modelId) {
            slot.engine->loadModelAsync(modelId);
        }
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
    qInfo() << "[AIEnginePool] Destroying engines, slot count:" << m_slots.size();
    
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
    
    qInfo() << "[AIEnginePool] All engines destroyed";
}

} // namespace EnhanceVision
