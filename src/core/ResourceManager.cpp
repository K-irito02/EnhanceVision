/**
 * @file ResourceManager.cpp
 * @brief 资源管理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ResourceManager.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#if NCNN_VULKAN
#include <gpu.h>
#endif

namespace EnhanceVision {

ResourceManager* ResourceManager::s_instance = nullptr;

ResourceManager* ResourceManager::instance() {
    if (!s_instance) {
        s_instance = new ResourceManager(QCoreApplication::instance());
    }
    return s_instance;
}

ResourceManager::ResourceManager(QObject* parent)
    : QObject(parent)
    , m_monitorTimer(new QTimer(this))
    , m_totalSystemMemoryMB(0)
    , m_totalGpuMemoryMB(0)
{
    m_totalSystemMemoryMB = queryTotalSystemMemory();
    m_totalGpuMemoryMB = queryTotalGpuMemoryMB();
    
    if (m_totalSystemMemoryMB > 0) {
        m_quota.maxMemoryMB = qMin(m_totalSystemMemoryMB / 2, 4096LL);
    }
    
    connect(m_monitorTimer, &QTimer::timeout, this, &ResourceManager::onMonitorTimeout);
}

bool ResourceManager::tryAcquire(const QString& taskId, qint64 memoryMB, qint64 gpuMemoryMB) {
    if (!canStartNewTask(memoryMB, gpuMemoryMB)) {
        return false;
    }
    
    m_taskResources[taskId] = qMakePair(memoryMB, gpuMemoryMB);
    emit resourceAcquired(taskId);
    return true;
}

void ResourceManager::release(const QString& taskId) {
    if (m_taskResources.remove(taskId) > 0) {
        emit resourceReleased(taskId);
    }
}

qint64 ResourceManager::usedMemoryMB() const {
    qint64 total = 0;
    for (auto it = m_taskResources.constBegin(); it != m_taskResources.constEnd(); ++it) {
        total += it.value().first;
    }
    return total;
}

qint64 ResourceManager::usedGpuMemoryMB() const {
    qint64 total = 0;
    for (auto it = m_taskResources.constBegin(); it != m_taskResources.constEnd(); ++it) {
        total += it.value().second;
    }
    return total;
}

int ResourceManager::activeTaskCount() const {
    return m_taskResources.size();
}

bool ResourceManager::canStartNewTask(qint64 estimatedMemoryMB, qint64 estimatedGpuMemoryMB) const {
    if (activeTaskCount() >= m_quota.maxConcurrentTasks) {
        return false;
    }
    
    if (usedMemoryMB() + estimatedMemoryMB > m_quota.maxMemoryMB) {
        return false;
    }
    
    if (usedGpuMemoryMB() + estimatedGpuMemoryMB > m_quota.maxGpuMemoryMB) {
        return false;
    }
    
    return true;
}

int ResourceManager::recommendedConcurrency() const {
    qint64 availableMemory = m_quota.maxMemoryMB - usedMemoryMB();
    qint64 avgTaskMemory = 256;
    
    if (activeTaskCount() > 0) {
        avgTaskMemory = usedMemoryMB() / activeTaskCount();
    }
    
    if (avgTaskMemory <= 0) {
        avgTaskMemory = 256;
    }
    
    int byMemory = static_cast<int>(availableMemory / avgTaskMemory);
    int byQuota = m_quota.maxConcurrentTasks - activeTaskCount();
    
    return qMax(0, qMin(byMemory, byQuota));
}

ResourcePressure ResourceManager::pressureLevel() const {
    return static_cast<ResourcePressure>(m_pressureLevel.load());
}

qreal ResourceManager::memoryUsagePercent() const {
    if (m_quota.maxMemoryMB <= 0) {
        return 0.0;
    }
    return static_cast<qreal>(usedMemoryMB()) / m_quota.maxMemoryMB * 100.0;
}

qreal ResourceManager::gpuMemoryUsagePercent() const {
    if (m_quota.maxGpuMemoryMB <= 0) {
        return 0.0;
    }
    return static_cast<qreal>(usedGpuMemoryMB()) / m_quota.maxGpuMemoryMB * 100.0;
}

ResourceQuota ResourceManager::quota() const {
    return m_quota;
}

void ResourceManager::setQuota(const ResourceQuota& quota) {
    m_quota = quota;
    emit quotaChanged();
}

void ResourceManager::setMaxConcurrentTasks(int max) {
    if (m_quota.maxConcurrentTasks != max) {
        m_quota.maxConcurrentTasks = max;
        emit quotaChanged();
    }
}

void ResourceManager::setMaxMemoryMB(qint64 maxMB) {
    if (m_quota.maxMemoryMB != maxMB) {
        m_quota.maxMemoryMB = maxMB;
        emit quotaChanged();
    }
}

void ResourceManager::setMaxGpuMemoryMB(qint64 maxMB) {
    if (m_quota.maxGpuMemoryMB != maxMB) {
        m_quota.maxGpuMemoryMB = maxMB;
        emit quotaChanged();
    }
}

qint64 ResourceManager::totalSystemMemoryMB() const {
    return m_totalSystemMemoryMB;
}

qint64 ResourceManager::availableSystemMemoryMB() const {
    return queryAvailableSystemMemory();
}

qint64 ResourceManager::estimateImageMemoryMB(const QString& filePath) const {
    QFileInfo info(filePath);
    qint64 fileSize = info.size();
    
    qint64 estimated = fileSize * 4 / (1024 * 1024);
    
    return qMax(64LL, qMin(estimated, 1024LL));
}

qint64 ResourceManager::estimateVideoMemoryMB(const QString& filePath) const {
    QFileInfo info(filePath);
    qint64 fileSize = info.size();
    
    qint64 fileSizeMB = fileSize / (1024 * 1024);
    
    qint64 estimated = qMax(256LL, fileSizeMB / 10);
    
    return qMin(estimated, 2048LL);
}

qint64 ResourceManager::estimateMemoryUsage(const QString& filePath, MediaType type) const {
    if (type == MediaType::Image) {
        return estimateImageMemoryMB(filePath);
    } else {
        return estimateVideoMemoryMB(filePath);
    }
}

void ResourceManager::startMonitoring(int intervalMs) {
    if (!m_monitorTimer->isActive()) {
        m_monitorTimer->start(intervalMs);
    }
}

void ResourceManager::stopMonitoring() {
    if (m_monitorTimer->isActive()) {
        m_monitorTimer->stop();
    }
}

QHash<QString, QPair<qint64, qint64>> ResourceManager::taskResources() const {
    return m_taskResources;
}

void ResourceManager::onMonitorTimeout() {
    updatePressureLevel();
}

void ResourceManager::updatePressureLevel() {
    qint64 availableMemory = availableSystemMemoryMB();
    qint64 totalMemory = m_totalSystemMemoryMB;
    
    if (totalMemory <= 0) {
        return;
    }
    
    qint64 usedPercent = (totalMemory - availableMemory) * 100 / totalMemory;
    
    int newPressureLevel = 0;
    if (usedPercent > 90) {
        newPressureLevel = 2;
    } else if (usedPercent > 75) {
        newPressureLevel = 1;
    }
    
    if (newPressureLevel != m_pressureLevel.load()) {
        m_pressureLevel.store(newPressureLevel);
        emit resourcePressureChanged(newPressureLevel);
        
        if (newPressureLevel == 2) {
            emit resourceExhausted();
        }
    }
}

qint64 ResourceManager::queryTotalSystemMemory() const {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return static_cast<qint64>(status.ullTotalPhys / (1024 * 1024));
    }
#endif
    
    QFile meminfo("/proc/meminfo");
    if (meminfo.open(QIODevice::ReadOnly)) {
        QByteArray data = meminfo.readAll();
        QRegularExpression re("MemTotal:\\s+(\\d+)");
        QRegularExpressionMatch match = re.match(QString::fromLatin1(data));
        if (match.hasMatch()) {
            return match.captured(1).toLongLong() / 1024;
        }
    }
    
    return 8192;
}

qint64 ResourceManager::queryAvailableSystemMemory() const {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return static_cast<qint64>(status.ullAvailPhys / (1024 * 1024));
    }
#endif
    
    QFile meminfo("/proc/meminfo");
    if (meminfo.open(QIODevice::ReadOnly)) {
        QByteArray data = meminfo.readAll();
        QRegularExpression re("MemAvailable:\\s+(\\d+)");
        QRegularExpressionMatch match = re.match(QString::fromLatin1(data));
        if (match.hasMatch()) {
            return match.captured(1).toLongLong() / 1024;
        }
    }
    
    return m_totalSystemMemoryMB / 2;
}

qint64 ResourceManager::totalGpuMemoryMB() const {
    return m_totalGpuMemoryMB;
}

qint64 ResourceManager::availableGpuMemoryMB() const {
    qint64 budget = queryGpuHeapBudgetMB();
    if (budget > 0) {
        return budget;
    }
    return qMax(0LL, m_quota.maxGpuMemoryMB - usedGpuMemoryMB());
}

qint64 ResourceManager::queryGpuHeapBudgetMB() const {
#if NCNN_VULKAN
    if (ncnn::get_gpu_count() > 0) {
        int gpuIndex = ncnn::get_default_gpu_index();
        const ncnn::VulkanDevice* vkdev = ncnn::get_gpu_device(gpuIndex);
        if (vkdev) {
            uint32_t budgetMB = vkdev->get_heap_budget();
            if (budgetMB > 0) {
                return static_cast<qint64>(budgetMB);
            }
        }
    }
#endif
    return 0;
}

qint64 ResourceManager::queryTotalGpuMemoryMB() const {
#if NCNN_VULKAN
    if (ncnn::get_gpu_count() > 0) {
        int gpuIndex = ncnn::get_default_gpu_index();
        const ncnn::GpuInfo& info = ncnn::get_gpu_info(gpuIndex);
        const VkPhysicalDeviceMemoryProperties& memProps = info.physical_device_memory_properties();
        qint64 totalMB = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                totalMB += static_cast<qint64>(memProps.memoryHeaps[i].size / (1024 * 1024));
            }
        }
        if (totalMB > 0) {
            return totalMB;
        }
    }
#endif
    return 0;
}

} // namespace EnhanceVision
