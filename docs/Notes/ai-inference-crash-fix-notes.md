# AI 推理崩溃修复笔记

## 概述

**创建日期**: 2026-03-31
**最后更新**: 2026-04-02
**问题类型**: 首次任务堆内存损坏崩溃 (STATUS_HEAP_CORRUPTION 0xc0000374)
**状态**: ✅ 已完全解决（多层防护机制）

---

## 一、问题总结

AI推理在以下场景出现**首个任务**崩溃：
1. 应用程序启动后首次处理图像/视频任务
2. 取消任务后首次重新处理任务
3. 模型切换后首次推理任务

**根本原因（多因素）**:
1. `ncnn::create_gpu_instance()` 被多个 AIEngine 实例重复调用，导致 Vulkan 状态不一致
2. 模型加载后 GPU 资源未完全初始化就开始推理，导致堆损坏
3. `m_opt.use_vulkan_compute` 跨线程无锁读取导致数据竞争
4. `taskCompleted` 信号双重发射导致状态混乱
5. 跨线程信号连接未使用 `Qt::QueuedConnection`

---

## 二、根因分析（2026-04-02 更新）

### 致命调用链

```
1. AIEngine 构造函数 [主线程]
   └─→ initVulkan()
       ├─→ warmupVulkanPipelineSync()
       └─→ updateOptions() → 获取 allocator_A → 赋值给 m_opt

2. 工作线程 → loadModel(modelId)
   └─→ m_net.opt = m_opt;  // m_net.opt.blob_vkallocator = allocator_A
   └─→ m_net.load_param() / load_model()  // Net 内部层缓存了 allocator_A 引用

3. 首帧推理 → runInference()
   └─→ ensureVulkanReady()  // 🔴 致命！
       ├─→ reclaim_blob_allocator(allocator_A)  // 销毁 m_net 正在引用的！
       ├─→ reclaim_staging_allocator(allocator_A)
       ├─→ acquire_blob_allocator() → allocator_B
       └─→ 更新 m_opt（但没更新 m_net.opt！！！）

4. 推理执行 → ex.input() / ex.extract()
   └─→ m_net.opt.blob_vkallocator 指向已释放的 allocator_A → 💥 崩溃
```

### 为什么是"首个任务"特别容易崩溃

| 阶段 | allocator 状态 | 说明 |
|------|---------------|------|
| initVulkan 后 | allocator_A (有效) | updateOptions() 创建 |
| loadModel 后 | m_net.opt → allocator_A | Net 缓存引用 |
| **ensureVulkanReady (首次)** | **allocator_A 被 reclaim** | **m_net 持有悬空指针** |
| 推理执行 | 访问已释放内存 | **💥 崩溃** |

---

## 三、修复方案（2026-04-02 根因级修复）

### 修改文件清单

| 文件路径 | 修改内容 | 优先级 |
|----------|---------|--------|
| `src/core/AIEngine.cpp` | 重写 ensureVulkanReady(), warmupVulkanPipelineSync(), syncVulkanQueue(), cleanupGpuMemory(), runInference() 后处理 | P0-P1 |
| `src/core/AIEnginePool.cpp` | 清理 release()/acquire() 中的 hacky sleep | P2 |

### 修改 1（P0 致命）：重写 `ensureVulkanReady()` — 不再销毁 allocator

**位置**: AIEngine.cpp:163-182

**修改前**: 每次推理前 reclaim 旧 allocator + acquire 新 allocator（导致 m_net 悬空指针）

**修改后**: 只做 VkCompute 同步 + clear() 缓存（不销毁 allocator 本身）

```cpp
void AIEngine::ensureVulkanReady()
{
#if NCNN_VULKAN
    if (!m_gpuAvailable || !m_useGpu || !m_vkdev) return;

    QMutexLocker allocatorLock(&m_allocatorMutex);

    // 只做 Vulkan 队列同步，不销毁/重建 allocator
    { ncnn::VkCompute cmd(m_vkdev); cmd.submit_and_wait(); }

    // 只清空缓存（不销毁 allocator 本身）
    if (m_blobVkAllocator) m_blobVkAllocator->clear();
    if (m_stagingVkAllocator) m_stagingVkAllocator->clear();
#endif
}
```

### 修改 2（P0 致命）：重写 `warmupVulkanPipelineSync()` — 移除临时 allocator

**位置**: AIEngine.cpp:238-256

**修改前**: 创建临时 allocator → 使用 → reclaim（残留 VkCompute 引用已释放内存）

**修改后**: 仅做 VkCompute 同步预热（无 allocator 操作）

### 修改 3（P1）：简化 `syncVulkanQueue()` — 移除多余 clear 和二次同步

### 修改 4（P1）：修复 `cleanupGpuMemory()` 锁一致性 — m_mutex 改为 m_allocatorMutex

### 修改 5（P1）：优化 `runInference()` 推理后同步 — 移除冗余锁嵌套

### 修改 6（P2）：清理 `AIEnginePool` release()/acquire() 中的 hacky msleep()

---

## 四、历史修复记录（2026-03-31 初次修复）

> ⚠️ 下述修复缓解了症状但未触及根本原因。本次（2026-04-02）修复才是真正的根因解决方案。

### 初次修复策略（部分有效）
1. **Vulkan双重同步**: 每次推理前后进行两次Vulkan队列同步
2. **分配器重新初始化**: 每次推理前重新获取Vulkan分配器 ← 🔴 这正是本次确认的根因！
3. **延迟任务切换**: 取消任务后延迟500ms再处理下一个任务
4. **状态检查机制**: 获取引擎前等待Vulkan完全就绪
5. **重复任务检查**: 防止重复提交相同任务

### 初次修复涉及的文件

| 文件路径 | 修改内容 |
|----------|---------|
| `src/controllers/ProcessingController.cpp` | 移除独立 AIEngine，简化清理流程 |
| `src/core/AIEnginePool.cpp` | 添加引擎获取时重置取消标志，释放时同步 Vulkan |
| `src/core/AIEngine.cpp` | 新增 `resetCancelFlag()` 和 `syncVulkanQueue()` 方法 |
| `include/EnhanceVision/core/AIEngine.h` | 添加新方法声明 |
| `include/EnhanceVision/core/AIEnginePool.h` | 添加 `firstEngine()` 方法声明 |

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 启动后立即执行图像推理（首个任务） | 正常完成 | ✅ 通过 |
| 启动后立即执行视频推理（首个任务） | 正常完成 | ✅ 通过 |
| 切换模型后立即推理 | 正常完成 | ✅ 通过 |
| 连续执行多次推理任务 | 全部成功 | ✅ 通过 |
| 单个视频处理 | 正常完成 | ✅ 通过 |
| 快速取消任务 | 无崩溃 | ✅ 通过 |
| 压力测试（快速切换） | 无崩溃 | ✅ 通过 |

---

## 六、2026-04-02 最终修复方案

### 修复 A：GPU 实例单例化

**问题**: 多个 AIEngine 实例各自调用 `ncnn::create_gpu_instance()` 和 `ncnn::destroy_gpu_instance()`，导致 Vulkan 全局状态被破坏。

**解决方案**: 使用 `std::call_once` + 引用计数确保全局只调用一次。

```cpp
// AIEngine.h - 新增静态成员
static std::once_flag s_gpuInstanceOnceFlag;
static std::atomic<int> s_gpuInstanceRefCount;
static std::mutex s_gpuInstanceMutex;
static void ensureGpuInstanceCreated();
static void releaseGpuInstanceRef();

// AIEngine.cpp - 实现
void AIEngine::ensureGpuInstanceCreated() {
    std::call_once(s_gpuInstanceOnceFlag, []() {
        ncnn::create_gpu_instance();
    });
}
```

### 修复 B：Allocator 锁保护 + m_opt 快照

**问题**: `runInference()` 中直接读取 `m_opt.use_vulkan_compute` 存在数据竞争。

**解决方案**: 
1. 推理后清理 allocator 时加 `m_allocatorMutex` 锁
2. 在推理开始时快照 `m_opt.use_vulkan_compute`，后续使用快照值

```cpp
bool useVulkan = false;
{
    QMutexLocker optLock(&m_mutex);
    useVulkan = m_opt.use_vulkan_compute;
}
```

### 修复 C：修复 taskCompleted 双重发射

**问题**: `checkCancellation()` 内部 `emit taskCompleted()`，但调用方返回后 `startTask()` 又会 `emit taskCompleted(result)`。

**解决方案**: 从 `checkCancellation()` 移除 `emit taskCompleted()`，统一由 `startTask()` 发射。

### 修复 D：信号连接改用 Qt::QueuedConnection

**问题**: `ProcessingController::connectAiEngineForTask()` 中跨线程信号连接未指定连接类型。

**解决方案**: 明确使用 `Qt::QueuedConnection`：
```cpp
conns << connect(engine, &AIEngine::progressChanged, this, 
    [...], Qt::QueuedConnection);
```

### 修复 E：模型同步等待机制（关键）

**问题**: 模型加载完成后，GPU 资源需要时间完成初始化，但推理线程立即开始处理导致堆损坏。

**解决方案**: 添加 `waitForModelSyncComplete()` 阻塞机制：

```cpp
// AIEngine.h - 新增成员
std::atomic<bool> m_modelSyncComplete{true};
std::mutex m_modelSyncMutex;
std::condition_variable m_modelSyncCv;
Q_INVOKABLE bool waitForModelSyncComplete(int timeoutMs = 5000);

// AIEngine.cpp - loadModel() 中
m_modelSyncComplete.store(false);  // 开始加载
// ... 模型加载和 Vulkan 同步 ...
m_modelSyncComplete.store(true);   // 完成
m_modelSyncCv.notify_all();

// AIVideoProcessor.cpp - 处理前等待
if (!m_impl->aiEngine->waitForModelSyncComplete(10000)) {
    emitCompleted(false, QString(), tr("模型同步超时"));
    return;
}
```

---

## 七、修改文件清单

| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/core/AIEngine.h` | 添加 GPU 单例静态成员、模型同步成员、`waitForModelSyncComplete()` 声明 |
| `src/core/AIEngine.cpp` | 实现 GPU 单例、allocator 锁保护、m_opt 快照、模型同步机制 |
| `src/core/ai/AIInferenceWorker.cpp` | 移除 `checkCancellation()` 中的 `emit taskCompleted()` |
| `src/controllers/ProcessingController.cpp` | 信号连接添加 `Qt::QueuedConnection` |
| `src/core/video/AIVideoProcessor.cpp` | 处理前调用 `waitForModelSyncComplete()` |

---

## 八、设计原则总结

### GPU 实例管理规则

1. **全局单例**: `ncnn::create_gpu_instance()` 只在首个 AIEngine 构造时调用一次
2. **引用计数**: 最后一个 AIEngine 析构时才调用 `ncnn::destroy_gpu_instance()`

### Vulkan Allocator 生命周期规则

1. **Allocator 与 ncnn::Net 绑定**: 只在 `loadModel()` 时通过 `m_net.opt = m_opt` 设置 allocator
2. **推理过程中不重建 allocator**: `ensureVulkanReady()` 只做同步和 clear 缓存
3. **如需重置 allocator 必须重新加载模型**: 确保 m_net.opt 始终指向有效 allocator

### 模型同步规则

1. **阻塞等待**: 推理前必须等待 `waitForModelSyncComplete()` 返回 true
2. **条件变量**: 使用 `std::condition_variable` 高效等待，避免忙轮询

### 线程安全规则

1. 使用 `m_allocatorMutex` 保护所有 Vulkan allocator 操作
2. 跨线程信号连接必须使用 `Qt::QueuedConnection`
3. 跨线程读取 `m_opt` 必须先加锁并做快照
