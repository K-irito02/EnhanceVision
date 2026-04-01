/**
 * @file AIEnginePool.cpp
 * @brief AI 推理引擎池实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/AIEnginePool.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/core/ModelRegistry.h"
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

            if (!m_slots[i].engine->waitForVulkanReady(5000)) {
                qWarning() << "[AIEnginePool] Engine slot" << i << "Vulkan not ready, skipping";
                continue;
            }

            m_slots[i].engine->syncVulkanQueue();
            m_slots[i].engine->resetCancelFlag();

            m_slots[i].state = EngineState::InUse;
            m_slots[i].taskId = taskId;
            m_taskToSlot[taskId] = i;
            
            qInfo() << "[AIEnginePool][DEBUG] Engine acquired for task:" << taskId
                    << ", slot index:" << i;

            emit engineAcquired(taskId, i);
            return m_slots[i].engine;
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
    
    auto& slot = m_slots[slotIndex];
    
    if (!slot.engine) {
        slot.state = EngineState::Error;
        slot.lastError = "Engine not created";
        return false;
    }
    
    if (slot.state == EngineState::Error) {
        qInfo() << "[AIEnginePool] Attempting to recover engine in slot" << slotIndex;
        slot.engine->resetState();
        slot.engine->cleanupGpuMemory();
    }
    
    // 【关键修复】强制等待 Vulkan 就绪，确保首次推理前 GPU 资源完全就绪
    if (!slot.engine->waitForVulkanReady(5000)) {
        qWarning() << "[AIEnginePool] Engine" << slotIndex << "Vulkan not ready after timeout";
        slot.state = EngineState::Error;
        slot.lastError = "Vulkan initialization timeout";
        return false;
    }
    
    slot.engine->ensureVulkanReady();
    
    if (slot.engine->gpuAvailable() || !slot.engine->useGpu()) {
        slot.state = EngineState::Ready;
        return true;
    }
    
    slot.state = EngineState::Error;
    slot.lastError = "GPU not available";
    return false;
}

void AIEnginePool::release(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) {
        return;
    }

    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        if (m_slots[idx].engine) {
            m_slots[idx].engine->cancelProcess();
            m_slots[idx].engine->syncVulkanQueue();
        }

        m_slots[idx].state = EngineState::Ready;
        m_slots[idx].taskId.clear();
        m_slots[idx].wasUsed = true;

        qInfo() << "[AIEnginePool][DEBUG] Task released:" << taskId << ", slot index:" << idx;
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
    qInfo() << "[AIEnginePool][DEBUG] Destroying engines, slot count:" << m_slots.size();
    
    // 第一步：取消所有进行中的处理
    for (auto& slot : m_slots) {
        if (slot.engine) {
            slot.engine->cancelProcess();
        }
    }
    
    // 第二步：清理 GPU 内存
    for (auto& slot : m_slots) {
        if (slot.engine) {
            slot.engine->cleanupGpuMemory();
        }
    }
    
    // 第三步：删除引擎
    for (auto& slot : m_slots) {
        delete slot.engine;
        slot.engine = nullptr;
    }
    m_slots.clear();
    m_taskToSlot.clear();
    
    qInfo() << "[AIEnginePool][DEBUG] All engines destroyed";
}

} // namespace EnhanceVision
