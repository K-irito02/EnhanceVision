# AI推理模式间歇性崩溃问题修复计划

## 问题概述

应用程序运行后首次处理任务时存在间歇性崩溃闪退问题，表现为任务处理时而成功时而失败并导致程序异常退出。

## 根本原因分析

通过代码审查，发现以下核心问题：

### 1. Vulkan 初始化竞态条件（核心问题）

**位置**: `AIEngine.cpp` - `initVulkan()` 和 `warmupVulkanPipeline()`

**问题描述**:
- `warmupVulkanPipeline()` 在 `QtConcurrent::run` 中异步执行
- 首次推理可能在 Vulkan warmup 完成前开始
- `ensureVulkanReady()` 的等待机制存在竞态条件
- `m_vulkanReady` 标志和 `m_vulkanInitCv` 条件变量的同步不够可靠

**代码证据**:
```cpp
// AIEngine.cpp:209-237
void AIEngine::warmupVulkanPipeline()
{
    // ...
    QtConcurrent::run([this]() {
        // 异步执行 warmup
        m_vulkanReady.store(true);  // 可能晚于首次推理
        m_vulkanInitCv.notify_all();
    });
}
```

### 2. Allocator 生命周期管理问题

**位置**: `AIEngine.cpp` - `updateOptions()` 和 `ensureVulkanReady()`

**问题描述**:
- 多处获取和释放 allocator，可能导致状态不一致
- `loadModel()` 中清理 allocator 可能影响正在进行的推理
- `m_blobVkAllocator` 和 `m_stagingVkAllocator` 的生命周期管理复杂

**代码证据**:
```cpp
// AIEngine.cpp:298-309 - loadModel() 中清理 allocator
if (m_vkdev) {
    if (m_blobVkAllocator) {
        m_blobVkAllocator->clear();  // 可能影响正在进行的推理
    }
    // ...
}
```

### 3. 推理互斥锁问题

**位置**: `AIEngine.cpp` - `runInference()`

**问题描述**:
- 持有 `m_mutex` 时调用 `ensureVulkanReady()`
- `ensureVulkanReady()` 内部也获取 `m_mutex`
- 可能导致死锁或状态不一致

**代码证据**:
```cpp
// AIEngine.cpp:1123-1128
if (m_opt.use_vulkan_compute) {
    ensureVulkanReady();  // 内部获取 m_mutex
}
QMutexLocker netLocker(&m_mutex);  // 再次获取 m_mutex
```

### 4. 引擎池状态管理问题

**位置**: `AIEnginePool.cpp` - `acquire()` 和 `release()`

**问题描述**:
- 引擎被复用时，前一个任务的状态可能未完全清理
- `ensureEngineReady()` 的状态检查不够严格
- 首次获取引擎时，Vulkan 可能未完全初始化

### 5. 首次推理时的初始化时序问题

**位置**: `ProcessingController.cpp` - `launchAiInference()`

**问题描述**:
- 首次推理时，Vulkan 可能未完全初始化
- 没有同步等待机制确保 GPU 资源就绪

## 修复方案

### 修复 1: 改进 Vulkan 初始化同步机制

**文件**: `src/core/AIEngine.cpp`

**修改内容**:
1. 将 `warmupVulkanPipeline()` 改为同步执行（首次推理前必须完成）
2. 添加 `waitForVulkanReady()` 方法，提供可靠的同步等待
3. 在 `process()` 入口处添加 Vulkan 就绪检查

### 修复 2: 改进 Allocator 生命周期管理

**文件**: `src/core/AIEngine.cpp`

**修改内容**:
1. 添加 `m_allocatorMutex` 保护 allocator 操作
2. 在 `loadModel()` 中不直接清理 allocator，改为标记需要重新初始化
3. 在推理开始时检查并重新初始化 allocator

### 修复 3: 修复推理互斥锁问题

**文件**: `src/core/AIEngine.cpp`

**修改内容**:
1. 在 `runInference()` 中，在获取 `m_mutex` 之前调用 `ensureVulkanReady()`
2. 避免嵌套锁获取

### 修复 4: 改进引擎池状态管理

**文件**: `src/core/AIEnginePool.cpp`

**修改内容**:
1. 在 `acquire()` 时强制同步等待 Vulkan 就绪
2. 添加更严格的状态验证
3. 在 `release()` 时确保引擎完全清理

### 修复 5: 添加首次推理前的同步等待

**文件**: `src/controllers/ProcessingController.cpp`

**修改内容**:
1. 在 `launchAiInference()` 前添加引擎就绪检查
2. 添加超时机制防止无限等待

## 详细实现步骤

### 步骤 1: 修改 AIEngine.h

添加新的成员变量和方法声明：

```cpp
// 新增成员变量
std::atomic<bool> m_vulkanInitialized{false};
std::atomic<bool> m_allocatorNeedsInit{false};
mutable QMutex m_allocatorMutex;

// 新增方法
bool waitForVulkanReady(int timeoutMs = 5000);
bool ensureAllocatorReady();
```

### 步骤 2: 修改 AIEngine.cpp - initVulkan()

改为同步初始化：

```cpp
void AIEngine::initVulkan()
{
#if NCNN_VULKAN
    // ... 现有初始化代码 ...
    
    if (m_gpuAvailable) {
        // 同步执行 warmup，不再使用 QtConcurrent::run
        warmupVulkanPipelineSync();
    }
    
    m_vulkanInitialized.store(m_gpuAvailable || !m_useGpu);
    m_vulkanReady.store(true);
    m_vulkanInitCv.notify_all();
#endif
}
```

### 步骤 3: 添加同步 warmup 方法

```cpp
void AIEngine::warmupVulkanPipelineSync()
{
#if NCNN_VULKAN
    if (!m_vkdev) return;
    
    ncnn::VkAllocator* warmupBlobAlloc = m_vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* warmupStagingAlloc = m_vkdev->acquire_staging_allocator();
    
    if (warmupBlobAlloc && warmupStagingAlloc) {
        ncnn::Mat dummy(64, 64, 3);
        dummy.fill(0.5f);
        // 简单的 warmup 操作
        
        m_vkdev->reclaim_blob_allocator(warmupBlobAlloc);
        m_vkdev->reclaim_staging_allocator(warmupStagingAlloc);
    }
#endif
}
```

### 步骤 4: 添加可靠的等待方法

```cpp
bool AIEngine::waitForVulkanReady(int timeoutMs)
{
#if NCNN_VULKAN
    if (!m_gpuAvailable || !m_useGpu) {
        return true;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    while (!m_vulkanReady.load() && timer.elapsed() < timeoutMs) {
        QThread::msleep(10);
    }
    
    return m_vulkanReady.load();
#else
    return true;
#endif
}
```

### 步骤 5: 修改 ensureVulkanReady()

```cpp
void AIEngine::ensureVulkanReady()
{
#if NCNN_VULKAN
    if (!m_gpuAvailable || !m_useGpu || !m_vkdev) {
        return;
    }
    
    // 同步等待 Vulkan 初始化完成
    if (!waitForVulkanReady(5000)) {
        qWarning() << "[AIEngine] Vulkan not ready after timeout";
        return;
    }
    
    // 使用独立的锁保护 allocator 操作
    QMutexLocker allocatorLock(&m_allocatorMutex);
    
    if (!m_blobVkAllocator || !m_stagingVkAllocator) {
        // 初始化 allocator
        m_blobVkAllocator = m_vkdev->acquire_blob_allocator();
        m_stagingVkAllocator = m_vkdev->acquire_staging_allocator();
        m_opt.blob_vkallocator = m_blobVkAllocator;
        m_opt.workspace_vkallocator = m_stagingVkAllocator;
    }
#endif
}
```

### 步骤 6: 修改 runInference()

```cpp
ncnn::Mat AIEngine::runInference(const ncnn::Mat &input, const ModelInfo &model)
{
    // ... 输入验证代码 ...
    
    // 在获取 m_mutex 之前确保 Vulkan 就绪
    if (m_opt.use_vulkan_compute) {
        ensureVulkanReady();
    }
    
    // 现在获取推理锁
    QMutexLocker netLocker(&m_mutex);
    
    // ... 推理代码 ...
}
```

### 步骤 7: 修改 AIEnginePool::acquire()

```cpp
AIEngine* AIEnginePool::acquire(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    
    // ... 现有代码 ...
    
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].state == EngineState::Ready || 
            m_slots[i].state == EngineState::Uninitialized) {
            
            // 强制等待 Vulkan 就绪
            if (!m_slots[i].engine->waitForVulkanReady(5000)) {
                qWarning() << "[AIEnginePool] Engine" << i << "Vulkan not ready";
                continue;
            }
            
            // 确保引擎状态正确
            m_slots[i].engine->ensureVulkanReady();
            
            // ... 其余代码 ...
        }
    }
    // ...
}
```

### 步骤 8: 修改 ProcessingController::launchAiInference()

```cpp
void ProcessingController::launchAiInference(AIEngine* engine, const QString& taskId, 
                                              const QString& inputPath, const QString& outputPath, 
                                              const Message& message)
{
    // 添加引擎就绪检查
    if (engine && !engine->waitForVulkanReady(10000)) {
        failTask(taskId, tr("AI引擎初始化超时，请重试"));
        return;
    }
    
    // ... 现有代码 ...
}
```

## 测试验证

### 测试场景

1. **首次启动后立即处理任务**
   - 启动应用后立即添加 AI 推理任务
   - 验证是否稳定处理，不再崩溃

2. **连续处理多个任务**
   - 连续添加多个 AI 推理任务
   - 验证任务间切换是否稳定

3. **GPU/CPU 模式切换**
   - 在 GPU 和 CPU 模式间切换
   - 验证模式切换后推理是否稳定

4. **视频处理**
   - 处理视频文件
   - 验证多帧推理是否稳定

### 验证标准

1. 首次推理成功率：100%
2. 连续推理稳定性：无崩溃
3. 日志检查：无 Vulkan 相关错误或警告
4. 性能影响：初始化时间增加不超过 2 秒

## 风险评估

### 低风险修改
- 添加 `waitForVulkanReady()` 方法
- 修改 `runInference()` 中的锁获取顺序

### 中等风险修改
- 修改 `initVulkan()` 为同步初始化
- 修改 `ensureVulkanReady()` 的锁策略

### 需要特别注意
- 确保所有修改不破坏现有的多线程推理逻辑
- 确保修改不会导致死锁
- 确保修改不会影响性能

## 回滚方案

如果修复引入新问题，可以通过以下方式回滚：
1. 恢复 `AIEngine.cpp` 中的原始初始化逻辑
2. 恢复 `AIEnginePool.cpp` 中的原始 acquire 逻辑
3. 保留新增的 `waitForVulkanReady()` 方法作为可选功能

## 预计工作量

- 代码修改：2-3 小时
- 编译测试：1 小时
- 功能验证：1-2 小时
- 总计：4-6 小时
