/**
 * @file ResourceManager.h
 * @brief 资源管理器 - 监控系统资源使用，动态调整并发限制
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_RESOURCEMANAGER_H
#define ENHANCEVISION_RESOURCEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QPair>
#include <atomic>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

/**
 * @brief 资源压力级别
 */
enum class ResourcePressure {
    Low = 0,     ///< 低压力 (< 75% 内存使用)
    Medium = 1,  ///< 中压力 (75% - 90% 内存使用)
    High = 2     ///< 高压力 (> 90% 内存使用)
};

/**
 * @brief 资源管理器
 * 
 * 负责：
 * - 监控系统资源使用（内存、GPU显存）
 * - 动态调整并发限制
 * - 资源压力时降级处理
 * - 任务资源申请和释放
 */
class ResourceManager : public QObject {
    Q_OBJECT

public:
    static ResourceManager* instance();
    
    bool tryAcquire(const QString& taskId, qint64 memoryMB, qint64 gpuMemoryMB);
    void release(const QString& taskId);
    
    qint64 usedMemoryMB() const;
    qint64 usedGpuMemoryMB() const;
    int activeTaskCount() const;
    
    bool canStartNewTask(qint64 estimatedMemoryMB, qint64 estimatedGpuMemoryMB) const;
    int recommendedConcurrency() const;
    
    ResourcePressure pressureLevel() const;
    qreal memoryUsagePercent() const;
    qreal gpuMemoryUsagePercent() const;
    
    ResourceQuota quota() const;
    void setQuota(const ResourceQuota& quota);
    void setMaxConcurrentTasks(int max);
    void setMaxMemoryMB(qint64 maxMB);
    void setMaxGpuMemoryMB(qint64 maxMB);
    
    qint64 totalSystemMemoryMB() const;
    qint64 availableSystemMemoryMB() const;
    
    qint64 totalGpuMemoryMB() const;
    qint64 availableGpuMemoryMB() const;
    
    qint64 estimateImageMemoryMB(const QString& filePath) const;
    qint64 estimateVideoMemoryMB(const QString& filePath) const;
    qint64 estimateMemoryUsage(const QString& filePath, MediaType type) const;
    
    void startMonitoring(int intervalMs = 2000);
    void stopMonitoring();
    
    QHash<QString, QPair<qint64, qint64>> taskResources() const;

signals:
    void resourcePressureChanged(int pressureLevel);
    void resourceExhausted();
    void resourceAcquired(const QString& taskId);
    void resourceReleased(const QString& taskId);
    void quotaChanged();

private slots:
    void onMonitorTimeout();

private:
    explicit ResourceManager(QObject* parent = nullptr);
    ~ResourceManager() override = default;
    
    Q_DISABLE_COPY(ResourceManager)

    ResourceQuota m_quota;
    QHash<QString, QPair<qint64, qint64>> m_taskResources;
    std::atomic<int> m_pressureLevel{0};
    QTimer* m_monitorTimer;
    qint64 m_totalSystemMemoryMB;
    qint64 m_totalGpuMemoryMB;
    
    static ResourceManager* s_instance;
    
    void updatePressureLevel();
    qint64 queryTotalSystemMemory() const;
    qint64 queryAvailableSystemMemory() const;
    qint64 queryGpuHeapBudgetMB() const;
    qint64 queryTotalGpuMemoryMB() const;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_RESOURCEMANAGER_H
