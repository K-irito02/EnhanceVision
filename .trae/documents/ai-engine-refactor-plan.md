# AI 推理引擎重构计划 - 移除 Vulkan/GPU 代码

## 一、问题分析

### 1.1 当前状态

用户反馈：**即使切换到 CPU 模式仍然崩溃**，这说明问题不仅仅是 Vulkan/GPU 相关代码的问题，而是整体架构设计存在缺陷。

### 1.2 当前架构问题

| 问题 | 严重程度 | 说明 |
|------|----------|------|
| Vulkan/GPU 代码与核心逻辑耦合 | 🔴 致命 | GPU 代码散布在推理流程各处，难以完全隔离 |
| 多线程同步复杂 | 🔴 致命 | 多个互斥锁、条件变量、原子变量交织 |
| 状态管理混乱 | 🟠 严重 | 多个状态标志（isProcessing、cancelled、vulkanReady 等）|
| 生命周期管理复杂 | 🟠 严重 | GPU 实例、Allocator、模型的创建/销毁时机难以控制 |

### 1.3 涉及文件

| 文件 | 代码行数 | Vulkan/GPU 相关代码 |
|------|----------|---------------------|
| `src/core/AIEngine.cpp` | ~2200 行 | ~40% |
| `include/EnhanceVision/core/AIEngine.h` | ~380 行 | ~30% |
| `src/core/AIEnginePool.cpp` | ~285 行 | ~20% |
| `src/core/ai/AIInferenceWorker.cpp` | ~590 行 | ~10% |
| `src/core/video/AIVideoProcessor.cpp` | ~870 行 | ~5% |

---

## 二、重构目标

### 2.1 核心目标

1. **完全移除 Vulkan/GPU 相关代码**：删除所有 `#if NCNN_VULKAN` 代码块
2. **简化推理引擎架构**：只保留纯 CPU 推理逻辑
3. **简化线程同步**：减少互斥锁和状态标志
4. **提高稳定性**：确保在任何情况下都不会崩溃

### 2.2 保留功能

- 模型加载/卸载
- 图像推理（CPU 模式）
- 视频处理
- 进度报告
- 取消机制
- 分块处理

### 2.3 移除功能

- Vulkan GPU 加速
- GPU 实例管理
- Vulkan Allocator 管理
- GPU OOM 检测
- GPU/CPU 模式切换

---

## 三、重构方案

### 3.1 方案对比

| 方案 | 优点 | 缺点 | 推荐度 |
|------|------|------|--------|
| **方案 A：完全重写** | 彻底解决问题，架构清晰 | 工作量大，风险高 | ⭐⭐⭐ |
| **方案 B：渐进式重构** | 风险可控，可逐步验证 | 可能遗留问题 | ⭐⭐⭐⭐ |
| **方案 C：最小化修改** | 工作量小 | 可能无法根本解决问题 | ⭐⭐ |

### 3.2 推荐方案：渐进式重构

**阶段划分**：

```
阶段 1：移除 Vulkan/GPU 代码
    ├── 删除所有 #if NCNN_VULKAN 代码块
    ├── 删除 GPU 实例管理代码
    ├── 删除 Vulkan Allocator 代码
    └── 简化 AIEngine 构造/析构

阶段 2：简化线程同步
    ├── 合并多个互斥锁
    ├── 简化状态标志
    └── 移除不必要的条件变量

阶段 3：重构推理流程
    ├── 简化 process() 方法
    ├── 简化 loadModel() 方法
    └── 移除 GPU 相关的同步等待

阶段 4：清理和测试
    ├── 移除无用代码
    ├── 更新头文件
    └── 全面测试
```

---

## 四、详细重构步骤

### 阶段 1：移除 Vulkan/GPU 代码

#### 1.1 AIEngine.h 修改

**删除内容**：
```cpp
// 删除以下成员和方法
#if NCNN_VULKAN
#include <gpu.h>  // 删除
#endif

// 删除静态成员
static std::once_flag s_gpuInstanceOnceFlag;
static std::atomic<int> s_gpuInstanceRefCount;
static std::mutex s_gpuInstanceMutex;
static void ensureGpuInstanceCreated();
static void releaseGpuInstanceRef();

// 删除成员变量
ncnn::VkAllocator* m_blobVkAllocator = nullptr;
ncnn::VkAllocator* m_stagingVkAllocator = nullptr;
ncnn::VulkanDevice* m_vkdev = nullptr;
bool m_gpuAvailable = false;
bool m_useGpu = true;
std::atomic<bool> m_vulkanReady{false};
std::atomic<bool> m_vulkanInitialized{false};
std::atomic<bool> m_vulkanWarmupInProgress{false};
std::atomic<bool> m_modelSyncComplete{true};
std::mutex m_modelSyncMutex;
std::condition_variable m_modelSyncCv;
mutable QMutex m_allocatorMutex;

// 删除方法
void initVulkan();
void destroyVulkan();
void updateOptions();
void ensureVulkanReady();
void warmupVulkanPipeline();
void warmupVulkanPipelineSync();
bool waitForVulkanReady(int timeoutMs = 5000);
bool waitForModelSyncComplete(int timeoutMs = 5000);
void syncVulkanQueue();
void cleanupGpuMemory();
bool gpuAvailable() const;
bool useGpu() const;
void setUseGpu(bool use);

// 删除信号
void useGpuChanged(bool useGpu);
void gpuAvailableChanged(bool available);
```

**保留内容**：
```cpp
// 核心推理功能
bool loadModel(const QString &modelId);
void unloadModel();
QImage process(const QImage &input);
void cancelProcess();
void resetCancelFlag();
void forceCancel();
bool isForceCancelled() const;
void resetState();

// 辅助方法
ncnn::Mat qimageToMat(const QImage &image, const ModelInfo &model);
QImage matToQimage(const ncnn::Mat &mat, const ModelInfo &model);
ncnn::Mat runInference(const ncnn::Mat &input, const ModelInfo &model);

// 分块处理
QImage processSingle(const QImage &input, const ModelInfo &model);
QImage processTiled(const QImage &input, const ModelInfo &model);
QImage processWithTTA(const QImage &input, const ModelInfo &model);

// 状态
std::atomic<bool> m_isProcessing{false};
std::atomic<bool> m_cancelRequested{false};
std::atomic<bool> m_forceCancelled{false};
std::atomic<double> m_progress{0.0};
ncnn::Net m_net;
ncnn::Option m_opt;
ModelInfo m_currentModel;
QString m_currentModelId;
```

#### 1.2 AIEngine.cpp 修改

**删除方法**：
- `initVulkan()` - 完全删除
- `destroyVulkan()` - 完全删除
- `updateOptions()` - 简化，只保留线程数设置
- `ensureVulkanReady()` - 完全删除
- `warmupVulkanPipeline()` - 完全删除
- `warmupVulkanPipelineSync()` - 完全删除
- `waitForVulkanReady()` - 完全删除
- `waitForModelSyncComplete()` - 完全删除
- `syncVulkanQueue()` - 完全删除
- `cleanupGpuMemory()` - 完全删除

**简化方法**：
- `AIEngine()` - 只初始化 ProgressReporter
- `~AIEngine()` - 只调用 unloadModel()
- `loadModel()` - 移除所有 Vulkan 同步代码
- `process()` - 移除所有 GPU 相关逻辑
- `runInference()` - 移除所有 Vulkan 相关代码

#### 1.3 AIEnginePool.cpp 修改

**删除内容**：
- `ensureEngineReady()` 中的 Vulkan 等待代码
- `release()` 中的 `syncVulkanQueue()` 调用
- `destroyEngines()` 中的 `cleanupGpuMemory()` 调用

#### 1.4 AIInferenceWorker.cpp 修改

**删除内容**：
- `ensureGpuReady()` 方法
- `cleanupGpuResources()` 方法

#### 1.5 AIVideoProcessor.cpp 修改

**删除内容**：
- `waitForModelSyncComplete()` 调用
- `cleanupGpuMemory()` 调用

---

### 阶段 2：简化线程同步

#### 2.1 合并互斥锁

**当前状态**：
```cpp
mutable QMutex m_mutex;           // 保护 loadModel/unloadModel/process
mutable QMutex m_inferenceMutex;  // 保护 runInference
mutable QMutex m_paramsMutex;     // 保护 m_parameters
mutable QMutex m_allocatorMutex;  // 保护 Vulkan allocator
mutable QMutex m_lastErrorMutex;  // 保护 m_lastError
```

**重构后**：
```cpp
mutable QMutex m_mutex;           // 保护所有状态
mutable QMutex m_inferenceMutex;  // 保护推理过程（保持独立）
```

#### 2.2 简化状态标志

**当前状态**：
```cpp
std::atomic<bool> m_isProcessing{false};
std::atomic<bool> m_cancelRequested{false};
std::atomic<bool> m_forceCancelled{false};
std::atomic<bool> m_gpuOomDetected{false};
std::atomic<bool> m_vulkanReady{false};
std::atomic<bool> m_vulkanInitialized{false};
std::atomic<bool> m_vulkanWarmupInProgress{false};
std::atomic<bool> m_modelSyncComplete{true};
```

**重构后**：
```cpp
std::atomic<bool> m_isProcessing{false};
std::atomic<bool> m_cancelRequested{false};
std::atomic<bool> m_forceCancelled{false};
```

---

### 阶段 3：重构推理流程

#### 3.1 简化 process() 方法

**重构前**（约 200 行）：
- GPU OOM 检测
- 分块大小计算
- GPU 回退逻辑
- 多次重试机制

**重构后**（约 80 行）：
```cpp
QImage AIEngine::process(const QImage &input)
{
    QMutexLocker inferenceLocker(&m_inferenceMutex);
    
    // 基本验证
    if (m_currentModelId.isEmpty() || !m_currentModel.isLoaded) {
        emitError(tr("未加载模型"));
        return QImage();
    }
    
    if (input.isNull() || input.bits() == nullptr) {
        emitError(tr("输入图像为空或数据无效"));
        return QImage();
    }
    
    // 快照当前模型和参数
    ModelInfo currentModel;
    QVariantMap effectiveParams;
    {
        QMutexLocker locker(&m_mutex);
        currentModel = m_currentModel;
        effectiveParams = getEffectiveParamsLocked(currentModel);
    }
    
    // 计算分块大小
    int tileSize = effectiveParams.value("tileSize", 0).toInt();
    if (tileSize == 0) {
        tileSize = computeAutoTileSizeForModel(input.size(), currentModel);
    }
    
    // 执行推理
    QImage result;
    if (tileSize > 0 && (input.width() > tileSize || input.height() > tileSize)) {
        result = processTiled(input, currentModel);
    } else {
        result = processSingle(input, currentModel);
    }
    
    setProgress(1.0);
    setProcessing(false);
    
    if (!result.isNull()) {
        emit processCompleted(result);
    }
    
    return result;
}
```

#### 3.2 简化 loadModel() 方法

**重构前**（约 150 行）：
- Vulkan 同步
- GPU 资源等待
- 多次同步调用

**重构后**（约 60 行）：
```cpp
bool AIEngine::loadModel(const QString &modelId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isProcessing.load()) {
        emit processError(tr("推理进行中，无法切换模型"));
        return false;
    }
    
    if (!m_modelRegistry || !m_modelRegistry->hasModel(modelId)) {
        emit processError(tr("模型未注册: %1").arg(modelId));
        return false;
    }
    
    ModelInfo info = m_modelRegistry->getModelInfo(modelId);
    
    // 卸载旧模型
    if (!m_currentModelId.isEmpty()) {
        m_net.clear();
        m_currentModel.isLoaded = false;
    }
    
    // 设置选项（仅 CPU）
    m_opt.num_threads = std::max(1, QThread::idealThreadCount() - 1);
    m_opt.use_packing_layout = true;
    m_opt.use_vulkan_compute = false;  // 强制 CPU 模式
    
    m_net.opt = m_opt;
    
    // 加载模型
    int ret = m_net.load_param(info.paramPath.toStdString().c_str());
    if (ret != 0) {
        emit processError(tr("加载模型参数失败: %1").arg(info.paramPath));
        return false;
    }
    
    ret = m_net.load_model(info.binPath.toStdString().c_str());
    if (ret != 0) {
        m_net.clear();
        emit processError(tr("加载模型权重失败: %1").arg(info.binPath));
        return false;
    }
    
    m_currentModelId = modelId;
    m_currentModel = info;
    m_currentModel.isLoaded = true;
    
    emit modelLoaded(modelId);
    emit modelChanged();
    return true;
}
```

---

### 阶段 4：清理和测试

#### 4.1 清理内容

1. **CMakeLists.txt**：
   - 移除 `NCNN_VULKAN` 相关配置
   - 移除 Vulkan SDK 路径配置

2. **头文件**：
   - 移除 `#if NCNN_VULKAN` 条件编译
   - 移除 GPU 相关声明

3. **文档更新**：
   - 更新 `docs/Notes/ai-inference-crash-fix-notes.md`
   - 更新 `.trae/rules/01-project-overview.md`

#### 4.2 测试计划

| 测试项 | 预期结果 |
|--------|----------|
| 图像推理（小图） | 正常完成 |
| 图像推理（大图，分块） | 正常完成 |
| 视频推理 | 正常完成 |
| 取消推理 | 正常取消 |
| 连续推理 | 无崩溃 |
| 快速切换模型 | 无崩溃 |
| 内存泄漏检测 | 无泄漏 |

---

## 五、风险评估

### 5.1 风险列表

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| 推理速度下降 | 高 | 中 | 可接受的代价 |
| 遗留 GPU 代码 | 低 | 高 | 仔细审查所有文件 |
| 线程同步问题 | 中 | 高 | 简化后更容易验证 |
| 功能缺失 | 低 | 中 | 保留核心功能 |

### 5.2 回滚计划

如果重构后问题仍然存在：
1. 检查 NCNN 版本是否兼容
2. 检查模型文件是否损坏
3. 检查内存分配是否有问题
4. 考虑使用 ONNX Runtime 替代

---

## 六、实施计划

### 6.1 时间估算

| 阶段 | 预计时间 |
|------|----------|
| 阶段 1：移除 Vulkan/GPU 代码 | 2-3 小时 |
| 阶段 2：简化线程同步 | 1-2 小时 |
| 阶段 3：重构推理流程 | 2-3 小时 |
| 阶段 4：清理和测试 | 1-2 小时 |
| **总计** | **6-10 小时** |

### 6.2 文件修改清单

| 文件 | 修改类型 | 预计删除行数 |
|------|----------|--------------|
| `include/EnhanceVision/core/AIEngine.h` | 重写 | ~150 行 |
| `src/core/AIEngine.cpp` | 重写 | ~900 行 |
| `src/core/AIEnginePool.cpp` | 修改 | ~50 行 |
| `src/core/ai/AIInferenceWorker.cpp` | 修改 | ~30 行 |
| `src/core/video/AIVideoProcessor.cpp` | 修改 | ~20 行 |
| `CMakeLists.txt` | 修改 | ~20 行 |

---

## 七、待确认事项

在开始实施前，需要确认以下问题：

### 7.1 功能确认

1. **是否保留 GPU 加速选项**？
   - 选项 A：完全移除，只支持 CPU
   - 选项 B：保留接口，但暂时禁用

2. **推理速度要求**？
   - CPU 模式会比 GPU 慢 2-5 倍
   - 是否可接受？

3. **是否需要保留 GPU 相关代码作为备选**？
   - 选项 A：完全删除
   - 选项 B：注释保留

### 7.2 架构确认

1. **是否需要引入其他推理引擎**？
   - 如 ONNX Runtime 作为备选

2. **是否需要重新设计引擎池**？
   - 当前引擎池设计是否过于复杂

---

## 八、总结

### 核心改动

1. **移除所有 Vulkan/GPU 相关代码**
2. **简化线程同步机制**
3. **简化推理流程**
4. **只保留纯 CPU 推理**

### 预期效果

1. **稳定性大幅提升**：移除复杂的 GPU 资源管理
2. **代码更简洁**：减少约 1000 行代码
3. **维护更容易**：逻辑更清晰

### 权衡

1. **性能下降**：CPU 推理比 GPU 慢
2. **功能减少**：失去 GPU 加速能力

---

*计划创建时间：2026-04-02*
*适用项目：EnhanceVision*
