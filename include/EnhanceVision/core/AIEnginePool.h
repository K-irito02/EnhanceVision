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
#include <QMutex>
#include <QString>

namespace EnhanceVision {

class AIEngine;
class ModelRegistry;

class AIEnginePool : public QObject
{
    Q_OBJECT

public:
    explicit AIEnginePool(int poolSize, ModelRegistry* registry, QObject* parent = nullptr);
    ~AIEnginePool() override;

    AIEngine* acquire(const QString& taskId);
    void release(const QString& taskId);

    AIEngine* engineForTask(const QString& taskId) const;

    int poolSize() const;
    int availableCount() const;
    int activeCount() const;

    bool preloadModel(const QString& modelId);
    void unloadAll();

    void setPoolSize(int size);

signals:
    void engineAcquired(const QString& taskId, int engineIndex);
    void engineReleased(const QString& taskId, int engineIndex);
    void poolExhausted();

private:
    void createEngines(int count);
    void destroyEngines();

    struct EngineSlot {
        AIEngine* engine = nullptr;
        QString taskId;
        bool inUse = false;
    };

    QList<EngineSlot> m_slots;
    QHash<QString, int> m_taskToSlot;
    ModelRegistry* m_modelRegistry;
    mutable QMutex m_mutex;
    int m_poolSize;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIENGINEPOOL_H
