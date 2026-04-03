/**
 * @file AIEnginePool.h
 * @brief AI 推理引擎池 - 管理多个 AIEngine 实例，支持并行 AI 推理
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_AIENGINEPOOL_H
#define ENHANCEVISION_AIENGINEPOOL_H

#include <QObject>
#include <QList>
#include <QHash>
#include <QString>
#include <QMutex>
#include "EnhanceVision/core/inference/InferenceTypes.h"

namespace EnhanceVision {

class AIEngine;
class ModelRegistry;

enum class EngineState {
    Uninitialized,
    Ready,
    InUse,
    Error
};

class AIEnginePool : public QObject
{
    Q_OBJECT

public:
    explicit AIEnginePool(int poolSize, ModelRegistry* registry, QObject* parent = nullptr);
    ~AIEnginePool() override;

    AIEngine* acquire(const QString& taskId);

    /**
     * @brief 获取引擎并指定后端类型（CPU 或 Vulkan GPU）
     * @param taskId 任务 ID
     * @param backendType 目标后端类型
     * @return 引擎指针，失败返回 nullptr
     */
    AIEngine* acquireWithBackend(const QString& taskId, BackendType backendType);

    void release(const QString& taskId);

    AIEngine* engineForTask(const QString& taskId) const;
    
    // 返回池中第一个引擎（用于兼容旧的 aiEngine() 接口）
    AIEngine* firstEngine() const;

    int poolSize() const;
    int availableCount() const;
    int activeCount() const;

    bool preloadModel(const QString& modelId);
    void unloadAll();

    void setPoolSize(int size);

    void warmupModel(const QString& modelId);

signals:
    void engineAcquired(const QString& taskId, int engineIndex);
    void engineReleased(const QString& taskId, int engineIndex);
    void poolExhausted();

private:
    void createEngines(int count);
    void destroyEngines();
    bool ensureEngineReady(int slotIndex);

    struct EngineSlot {
        AIEngine* engine = nullptr;
        QString taskId;
        EngineState state = EngineState::Uninitialized;
        BackendType backendType = BackendType::NCNN_CPU;
        QString lastError;
        bool wasUsed = false;
    };

    QList<EngineSlot> m_slots;
    QHash<QString, int> m_taskToSlot;
    ModelRegistry* m_modelRegistry;
    mutable QMutex m_mutex;
    int m_poolSize;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIENGINEPOOL_H
