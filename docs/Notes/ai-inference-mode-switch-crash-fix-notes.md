# AI 推理模式切换崩溃问题修复笔记

## 概述

修复 CPU → GPU 推理模式切换时应用程序崩溃闪退的问题。

**创建日期**: 2026-04-05
**作者**: AI Assistant
**问题类型**: 多模式任务调度崩溃

---

## 一、问题描述

### 问题现象

当系统中存在两个推理任务（CPU推理任务和GPU推理任务）时：
1. CPU推理任务成功完成
2. 在准备处理GPU推理任务的过渡阶段，应用程序发生崩溃并闪退

### 复现步骤

1. 启动应用程序
2. 提交一个 CPU 后端的 AI 推理任务
3. 等待 CPU 任务完成
4. 提交一个 GPU (Vulkan) 后端的 AI 推理任务
5. 应用程序崩溃闪退

### 日志分析

```
[03:39:49.280] CPU推理任务完成
[03:39:49.394] 任务释放
[03:39:49.405] 任务终止
[03:39:49.503] resetState() 被调用
[03:39:49.535] Vulkan 初始化成功
[03:39:49.535] 模型重新加载
[03:39:49.576] GPU 推理开始
[03:39:49.588] 崩溃（在 ex.extract() 调用时）
```

---

## 二、根因分析

### 核心问题

1. **Vulkan 实例生命周期管理缺陷**
   - `probeGpuAvailability()` 在启动时创建并销毁 Vulkan 实例
   - `setBackendType()` 切换到 GPU 时重新初始化
   - 可能存在资源竞争或状态不一致

2. **模型加载时后端类型未正确跟踪**
   - `loadModel()` 检查 `m_currentModelId == modelId && m_currentModel.isLoaded` 时直接返回
   - 没有检查模型是否是用当前后端加载的
   - 导致切换后端时模型没有重新加载

3. **引擎池获取与释放时序问题**
   - `AIEnginePool::acquireWithBackend()` 调用 `resetState()` 和 `setBackendType()`
   - `setBackendType()` 会触发 Vulkan 初始化和模型重新加载
   - 如果上一个任务刚完成，引擎可能未完全清理

### 调用链分析

```
CPU任务完成
  └─→ ProcessingController::completeTask()
      └─→ disconnectAiEngineForTask()
      └─→ AIEnginePool::release(taskId)

GPU任务开始
  └─→ ProcessingController::processNextTask()
      └─→ AIEnginePool::acquireWithBackend(taskId, BackendType::NCNN_Vulkan)
          └─→ engine->resetState()
          └─→ engine->setBackendType(BackendType::NCNN_Vulkan)
              └─→ initializeVulkan()
              └─→ loadModel()  // 问题：模型没有重新加载！
          └─→ launchAiInference()
              └─→ engine->processAsync()
                  └─→ runInference()
                      └─→ ex.extract() → 💥 崩溃
```

---

## 三、解决方案

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/core/AIEngine.h` | 添加 Vulkan 实例单例化静态成员、模型同步机制、GPU 就绪状态 |
| `src/core/AIEngine.cpp` | 实现 Vulkan 实例单例化、修复模型加载后端检测、添加 GPU 资源验证 |
| `src/core/AIEnginePool.cpp` | 优化 acquireWithBackend() 等待机制 |

### 关键修改

#### 1. Vulkan 实例单例化管理

```cpp
// AIEngine.h
static std::once_flag s_gpuInstanceOnceFlag;
static std::atomic<int> s_gpuInstanceRefCount;
static std::mutex s_gpuInstanceMutex;
static std::atomic<bool> s_gpuInstanceCreated;

static void ensureGpuInstanceCreated();
static void releaseGpuInstanceRef();
static bool isGpuInstanceCreated();

// AIEngine.cpp
std::once_flag AIEngine::s_gpuInstanceOnceFlag;
std::atomic<int> AIEngine::s_gpuInstanceRefCount{0};
std::mutex AIEngine::s_gpuInstanceMutex;
std::atomic<bool> AIEngine::s_gpuInstanceCreated{false};

void AIEngine::ensureGpuInstanceCreated()
{
#ifdef NCNN_VULKAN_AVAILABLE
    std::call_once(s_gpuInstanceOnceFlag, []() {
        qInfo() << "[AIEngine] Creating global Vulkan GPU instance (once)";
        int ret = ncnn::create_gpu_instance();
        if (ret == 0) {
            s_gpuInstanceCreated.store(true);
        }
    });
    
    if (s_gpuInstanceCreated.load()) {
        std::lock_guard<std::mutex> lock(s_gpuInstanceMutex);
        s_gpuInstanceRefCount++;
    }
#endif
}
```

#### 2. 模型加载后端类型跟踪

```cpp
// AIEngine.h
BackendType m_modelLoadedBackendType = BackendType::NCNN_CPU;

// AIEngine.cpp - loadModel()
if (m_currentModelId == modelId && m_currentModel.isLoaded) {
    if (m_modelLoadedBackendType != m_backendType) {
        qInfo() << "[AIEngine][loadModel] Model loaded with different backend, forcing reload";
    } else {
        return true;  // 模型已加载且后端匹配
    }
}

// 模型加载成功后
m_modelLoadedBackendType = m_backendType;
```

#### 3. GPU 就绪等待机制

```cpp
// AIEngine.h
std::atomic<bool> m_gpuReady{false};
std::mutex m_gpuReadyMutex;
std::condition_variable m_gpuReadyCv;

bool waitForGpuReady(int timeoutMs = 5000);

// AIEngine.cpp
bool AIEngine::waitForGpuReady(int timeoutMs)
{
    if (m_gpuReady.load()) {
        return true;
    }
    
    std::unique_lock<std::mutex> lock(m_gpuReadyMutex);
    return m_gpuReadyCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
        [this] { return m_gpuReady.load(); });
}
```

#### 4. 推理前 GPU 资源验证

```cpp
// runInference() 中
#ifdef NCNN_VULKAN_AVAILABLE
    if (m_backendType == BackendType::NCNN_Vulkan) {
        if (!waitForModelSyncComplete(5000)) {
            qWarning() << "[AIEngine][Inference] Model sync not complete";
            return ncnn::Mat();
        }
        if (!waitForGpuReady(3000)) {
            qWarning() << "[AIEngine][Inference] GPU not ready";
            return ncnn::Mat();
        }
        if (!s_gpuInstanceCreated.load()) {
            qWarning() << "[AIEngine][Inference] Vulkan instance not created";
            return ncnn::Mat();
        }
    }
#endif
```

#### 5. 引擎池获取优化

```cpp
// AIEnginePool::acquireWithBackend()
if (backendType == BackendType::NCNN_Vulkan) {
    if (!engine->waitForModelSyncComplete(10000)) {
        qWarning() << "[AIEnginePool] Model sync timeout";
        continue;
    }
    if (!engine->waitForGpuReady(5000)) {
        qWarning() << "[AIEnginePool] GPU ready timeout";
        continue;
    }
}
```

---

## 四、测试验证

### 测试场景

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| CPU → GPU 任务顺序 | 无崩溃，正常完成 | ✅ 通过 |
| GPU → CPU 任务顺序 | 无崩溃，正常完成 | ✅ 通过 |
| CPU → GPU → CPU 顺序 | 无崩溃，正常完成 | ✅ 通过 |
| 相同后端连续任务 | 无崩溃，正常完成 | ✅ 通过 |
| 图片推理 | 正常完成 | ✅ 通过 |
| 视频推理 | 正常完成 | ✅ 通过 |

### 边界条件

- 快速连续提交多个任务：✅ 通过
- 任务取消后重新提交：✅ 通过
- 模型切换后推理：✅ 通过

---

## 五、性能影响

| 指标 | 变更前 | 变更后 | 影响 |
|------|--------|--------|------|
| Vulkan 实例创建次数 | 每次切换都创建 | 仅启动时创建一次 | 减少 GPU 资源开销 |
| 后端切换延迟 | 无等待 | 增加同步等待 | 略微增加切换时间 |
| 内存占用 | 可能泄漏 | 稳定 | 改善 |

---

## 六、后续工作

- [ ] 考虑支持多 GPU 设备选择
- [ ] 优化后端切换性能
- [ ] 添加更详细的性能监控日志

---

## 七、参考资料

- `docs/Notes/ai-inference-crash-fix-notes.md` - 首次任务崩溃修复笔记
- `docs/Notes/cpu-video-inference-crash-fix-notes.md` - CPU 视频推理崩溃修复笔记
- NCNN Vulkan 文档：https://github.com/Tencent/ncnn/wiki/ncnn-vulkan
