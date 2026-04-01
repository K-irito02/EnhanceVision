# AI 推理首次任务崩溃根因分析与修复计划

## 问题现象
AI 推理模式在处理视频或图像时，**首个处理任务**（新任务或重新处理任务）进行中造成应用程序崩溃闪退。

---

## 根因分析（ROOT CAUSE）

### 核心根因：`ensureVulkanReady()` 销毁 ncnn::Net 正在使用的 Vulkan Allocator

**调用链追踪（首次任务的完整执行路径）：**

```
1. AIEngine 构造函数 [主线程]
   └─→ initVulkan()
       ├─→ ncnn::create_gpu_instance()
       ├─→ warmupVulkanPipelineSync()  // 创建临时allocator → 使用 → reclaim(销毁)
       └─→ updateOptions()             // 获取新 allocator_A → 赋值给 m_opt

2. 工作线程启动 → loadModel(modelId)
   └─→ m_net.opt = m_opt;              // ⚠️ m_net.opt.blob_vkallocator = allocator_A
   └─→ m_net.load_param() / load_model()  // Net 内部层缓存了 allocator_A 的引用

3. 首帧推理 → runInference()
   └─→ ensureVulkanReady()             // 🔴 致命问题！
       ├─→ reclaim_blob_allocator(allocator_A)   // 销毁 m_net 正在引用的 allocator！
       ├─→ reclaim_staging_allocator(allocator_A) // 同上！
       ├─→ acquire_blob_allocator() → allocator_B // 获取新的
       └─→ 更新 m_opt（但没更新 m_net.opt！！！）

4. 推理执行 → ex.input() / ex.extract()
   └─→ m_net.opt.blob_vkallocator 指向已释放的 allocator_A → 堆损坏/崩溃
```

**关键代码位置**：
- [AIEngine.cpp:163-215](src/core/AIEngine.cpp#L163-L215) — `ensureVulkanReady()` 销毁并重建 allocator
- [AIEngine.cpp:480](src/core/AIEngine.cpp#L480) — `loadModel()` 中 `m_net.opt = m_opt` 复制了 allocator 指针
- [AIEngine.cpp:1331-1333](src/core/AIEngine.cpp#L1331-L1333) — `runInference()` 在推理前调用 `ensureVulkanReady()`
- [AIEngine.cpp:131-161](src/core/AIEngine.cpp#L131-L161) — `updateOptions()` 获取 allocator

### 为什么是"首个任务"特别容易崩溃

| 阶段 | allocator 状态 | 说明 |
|------|---------------|------|
| initVulkan 后 | allocator_A (有效) | updateOptions() 创建 |
| loadModel 后 | m_net.opt → allocator_A | Net 缓存引用 |
| **ensureVulkanReady (首次)** | **allocator_A 被 reclaim** | **m_net 持有悬空指针** |
| 推理执行 | 访问已释放内存 | **💥 崩溃** |

后续任务可能因为堆内存尚未被覆写而暂时"幸存"，但堆损坏是累积的。

### 次要问题

| # | 问题 | 严重度 | 位置 |
|---|------|--------|------|
| 2 | Vulkan 资源在主线程创建，工作线程使用（跨线程上下文） | 高 | [AIEngine.cpp:41-111](src/core/AIEngine.cpp#L41-L111), [AIInferenceWorker.cpp:535](src/core/ai/AIInferenceWorker.cpp#L535) |
| 3 | `warmupVulkanPipelineSync()` 使用临时 allocator 后立即 reclaim，残留 VkCompute 命令引用已释放内存 | 中 | [AIEngine.cpp:262-313](src/core/AIEngine.cpp#L262-L313) |
| 4 | `cleanupGpuMemory()` 使用 `m_mutex` 而非 `m_allocatorMutex`，锁不一致 | 中 | [AIEngine.cpp:982](src/core/AIEngine.cpp#L982) |
| 5 | `AIEnginePool::release()` 用 hacky `msleep(50/100)` 替代正确同步 | 低 | [AIEnginePool.cpp:140,146](src/core/AIEnginePool.cpp#L140,L146) |

---

## 修复方案

### 修改 1（P0 致命）：重写 `ensureVulkanReady()` — 不再销毁 allocator

**文件**: [AIEngine.cpp:163-215](src/core/AIEngine.cpp#L163-L215)

**当前问题代码**:
```cpp
void AIEngine::ensureVulkanReady() {
    // 先同步并清理现有分配器 ← 这里销毁了 m_net 正在用的 allocator！
    if (m_blobVkAllocator || m_stagingVkAllocator) {
        ncnn::VkCompute cmd(m_vkdev);
        cmd.submit_and_wait();
        if (m_blobVkAllocator) {
            m_blobVkAllocator->clear();
            m_vkdev->reclaim_blob_allocator(m_blobVkAllocator);  // 🔴 销毁！
            m_blobVkAllocator = nullptr;
        }
        // ... staging 同上 ...
    }
    // 获取新的分配器 ← 新 allocator 不会同步到 m_net.opt
    m_blobVkAllocator = m_vkdev->acquire_blob_allocator();
    // ...
}
```

**修改为**:
```cpp
void AIEngine::ensureVulkanReady() {
#if NCNN_VULKAN
    if (!m_gpuAvailable || !m_useGpu || !m_vkdev) {
        return;
    }

    QMutexLocker allocatorLock(&m_allocatorMutex);

    // 只做 Vulkan 队列同步，确保之前的 GPU 操作已完成
    // 不再销毁和重建 allocator，因为 ncnn::Net (m_net) 的 opt 
    // 持有 allocator 引用，销毁会导致悬空指针
    {
        ncnn::VkCompute cmd(m_vkdev);
        cmd.submit_and_wait();
    }

    // 只清空 allocator 缓存（不销毁 allocator 本身）
    if (m_blobVkAllocator) {
        m_blobVkAllocator->clear();
    }
    if (m_stagingVkAllocator) {
        m_stagingVkAllocator->clear();
    }

    qInfo() << "[AIEngine][DEBUG] ensureVulkanReady() completed (sync only)";
#endif
}
```

### 修改 2（P0 致命）：重写 `warmupVulkanPipelineSync()` — 复用正式 allocator

**文件**: [AIEngine.cpp:262-313](src/core/AIEngine.cpp#L262-L313)

**修改为**: warmup 阶段使用正式的 allocator（通过 updateOptions 获取），warmup 后保留不复用。将 warmup 逻辑简化为仅做 VkCompute 同步预热：

```cpp
void AIEngine::warmupVulkanPipelineSync() {
#if NCNN_VULKAN
    if (!m_vkdev) return;

    qInfo() << "[AIEngine][DEBUG] warmupVulkanPipelineSync() starting";

    // 先确保 options 和 allocator 已就绪
    updateOptions();

    // 仅做多次 VkCompute 同步来预热 GPU 管线
    for (int i = 0; i < 3; ++i) {
        {
            ncnn::VkCompute cmd(m_vkdev);
            cmd.submit_and_wait();
        }
        QThread::msleep(50);
    }

    qInfo() << "[AIEngine][DEBUG] Vulkan pipeline sync warmup completed";
#endif
}
```

### 修改 3（P0 致命）：确保 `initVulkan()` 中 `updateOptions()` 在 `warmup` 之后正确设置

**文件**: [AIEngine.cpp:41-111](src/core/AIEngine.cpp#L41-L111)

调整初始化顺序：
1. 创建 GPU 实例
2. 调用 `warmupVulkanPipelineSync()` （只做同步预热）
3. 调用 `updateOptions()` （获取正式 allocator 并赋值给 m_opt）
4. 后续 `loadModel()` 时 `m_net.opt = m_opt` 将持有正确的 allocator

当前顺序已经是正确的，但需要确认 `warmupVulkanPipelineSync` 不再内部 reclaim allocator。

### 修改 4（P1 重要）：`syncVulkanQueue()` 移除不必要的 clear()

**文件**: [AIEngine.cpp:348-385](src/core/AIEngine.cpp#L348-345)

`syncVulkanQueue()` 中的 `clear()` 可能与正在进行的推理冲突。改为仅做同步：

```cpp
void AIEngine::syncVulkanQueue() {
#if NCNN_VULKAN
    if (!m_gpuAvailable || !m_useGpu || !m_vkdev) return;

    QMutexLocker locker(&m_allocatorMutex);

    // 只做队列同步，不清理 allocator 缓存
    // clear() 应只在推理完成后调用
    {
        ncnn::VkCompute cmd(m_vkdev);
        cmd.submit_and_wait();
    }

    qInfo() << "[AIEngine][DEBUG] syncVulkanQueue() completed";
#endif
}
```

### 修改 5（P1 重要）：`runInference()` 推理后同步时只做 clear

**文件**: [AIEngine.cpp:1386-1413](src/core/AIEngine.cpp#L1386-L1413)

推理后的 Vulkan 同步保持不变（已经只是 clear + sync），但移除多余的 allocatorMutex 加锁（因为此时 m_inferenceMutex 已持有）。

### 修改 6（P1 重要）：`cleanupGpuMemory()` 锁一致性修复

**文件**: [AIEngine.cpp:970-996](src/core/AIEngine.cpp#L970-L996)

将 `m_mutex` 改为 `m_allocatorMutex`：

```cpp
void AIEngine::cleanupGpuMemory() {
#if NCNN_VULKAN
    if (!m_vkdev) return;

    // 使用 m_allocatorMutex 保护 allocator 操作（而非 m_mutex）
    QMutexLocker locker(&m_allocatorMutex);

    if (m_blobVkAllocator) {
        m_blobVkAllocator->clear();
    }
    if (m_stagingVkAllocator) {
        m_stagingVkAllocator->clear();
    }

    m_gpuOomDetected.store(false);
#endif
}
```

### 修改 7（P2 改进）：清理 `AIEnginePool` 中的 hacky sleep

**文件**: [AIEnginePool.cpp:123-156](src/core/AIEnginePool.cpp#L123-L156)

移除 `release()` 中的 `QThread::msleep(50)` 和 `QThread::msleep(100)`，用单次 `syncVulkanQueue()` 替代：

```cpp
void AIEnginePool::release(const QString& taskId) {
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) return;

    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        if (m_slots[idx].engine) {
            m_slots[idx].engine->cancelProcess();
            // 单次同步即可确保 GPU 操作完成
            m_slots[idx].engine->syncVulkanQueue();
        }

        m_slots[idx].state = EngineState::Ready;
        m_slots[idx].taskId.clear();
        m_slots[idx].wasUsed = true;
        emit engineReleased(taskId, idx);
    }
}
```

同样清理 `acquire()` 中的多余 sleep（[AIEnginePool.cpp:58](src/core/AIEnginePool.cpp#L58), [AIEnginePool.cpp:64](src/core/AIEnginePool.cpp#L64)）。

---

## 修改文件清单

| 文件 | 修改内容 | 优先级 |
|------|---------|--------|
| `src/core/AIEngine.cpp` | 重写 ensureVulkanReady(), warmupVulkanPipelineSync(), syncVulkanQueue(), cleanupGpuMemory(), runInference() 后处理 | P0-P1 |
| `include/EnhanceVision/core/AIEngine.h` | 无需修改（接口不变） | - |
| `src/core/AIEnginePool.cpp` | 清理 release()/acquire() 中的 hacky sleep | P2 |
| `src/core/ai/AIInferenceWorker.cpp` | 无需修改（调用 ensureGpuReady 即可） | - |

## 验证方案

1. **构建验证**：编译通过，无新增警告
2. **功能验证**：
   - 启动应用后立即执行图像推理 → 应正常完成
   - 启动应用后立即执行视频推理 → 应正常完成
   - 切换模型后立即推理 → 应正常完成
   - 连续执行 10+ 次推理任务 → 应全部成功
3. **日志验证**：运行时日志中不应出现 Vulkan 相关错误或警告
4. **稳定性验证**：长时间运行多任务后无内存泄漏或性能退化
