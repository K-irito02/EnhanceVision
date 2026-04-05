# AI推理GPU模式问题调查与修复计划

## 问题描述

用户报告：在AI推理模式下，选择GPU进行推理时，视频类型的多媒体文件似乎并没有真实进行推理。需要检查图片类型是否也有类似情况。

## 调查发现

### 问题根源

**核心问题：`AIEngine::setBackendType()` 切换到 Vulkan 后端时，没有重新加载模型，导致 `m_net.set_vulkan_device()` 未被调用，GPU推理不生效。**

#### 代码流程分析

1. **任务启动时后端选择** ([ProcessingController.cpp:635-638](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp#L635-L638))
   ```cpp
   const BackendType requestedBackend = message.aiParams.useGpu
       ? BackendType::NCNN_Vulkan
       : BackendType::NCNN_CPU;
   AIEngine* engine = m_aiEnginePool->acquireWithBackend(taskId, requestedBackend);
   ```
   - 正确根据 `useGpu` 参数选择后端类型

2. **引擎池获取引擎** ([AIEnginePool.cpp:101](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/core/AIEnginePool.cpp#L101))
   ```cpp
   if (!engine->setBackendType(backendType)) {
       // ...
   }
   ```
   - 调用 `setBackendType` 切换后端

3. **`setBackendType` 的实现问题** ([AIEngine.cpp:691-729](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/core/AIEngine.cpp#L691-L729))
   ```cpp
   bool AIEngine::setBackendType(BackendType type) {
       // ...
       if (type == BackendType::NCNN_Vulkan) {
           if (!initializeVulkan()) { return false; }
           m_backendType = BackendType::NCNN_Vulkan;
           applyBackendOptions(BackendType::NCNN_Vulkan);  // 只设置 m_net.opt
           // ❌ 没有调用 m_net.set_vulkan_device()
           // ❌ 没有重新加载模型
       }
   }
   ```

4. **模型加载时正确设置 Vulkan** ([AIEngine.cpp:166-172](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/core/AIEngine.cpp#L166-L172))
   ```cpp
   m_net.opt = m_opt;
   #ifdef NCNN_VULKAN_AVAILABLE
   if (m_backendType == BackendType::NCNN_Vulkan && m_gpuAvailable.load()) {
       m_net.set_vulkan_device(m_gpuDeviceId);  // ✅ 这里才设置 Vulkan 设备
   }
   #endif
   ```
   - `m_net.set_vulkan_device()` 只在 `loadModel()` 中调用
   - 如果模型已加载，切换后端后不会重新调用

### 影响范围

| 文件类型 | 是否受影响 | 原因 |
|---------|-----------|------|
| **视频** | ✅ 受影响 | 使用 `AIVideoProcessor`，依赖 `AIEngine::process()` |
| **图片** | ✅ 受影响 | 使用 `AIEngine::processAsync()` → `process()` |

**结论：图片和视频都受此问题影响，GPU推理均不生效。**

### 其他发现

1. **视频处理配置硬编码** ([ProcessingController.cpp:2124](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp#L2124))
   ```cpp
   config.useGpu = false;  // 硬编码为 false
   ```
   - 这个配置在 `AIVideoProcessor` 中未被使用来改变引擎行为
   - 不是根本原因，但应该修正以保持一致性

## 修复方案

### 方案一：在 `setBackendType` 中重新加载模型（推荐）

**修改文件**: `src/core/AIEngine.cpp`

**修改内容**:
```cpp
bool AIEngine::setBackendType(BackendType type)
{
    // ... 现有检查代码 ...

    QString currentModelId = m_currentModelId;  // 保存当前模型ID
    
    if (type == BackendType::NCNN_Vulkan) {
        if (!initializeVulkan()) {
            return false;
        }
        m_backendType = BackendType::NCNN_Vulkan;
        applyBackendOptions(BackendType::NCNN_Vulkan);
        
        // 【新增】如果已有模型加载，需要重新加载以应用 Vulkan 设置
        if (!currentModelId.isEmpty()) {
            qInfo() << "[AIEngine] Reloading model for Vulkan backend:" << currentModelId;
            if (!loadModel(currentModelId)) {
                qWarning() << "[AIEngine] Failed to reload model after backend switch";
                // 回退到 CPU
                m_backendType = BackendType::NCNN_CPU;
                applyBackendOptions(BackendType::NCNN_CPU);
                return false;
            }
        }
        
        qInfo() << "[AIEngine] Switched to Vulkan GPU backend";
    } else {
        // CPU 模式
        shutdownVulkan();
        m_backendType = BackendType::NCNN_CPU;
        applyBackendOptions(BackendType::NCNN_CPU);
        
        // 【新增】如果已有模型加载，需要重新加载
        if (!currentModelId.isEmpty()) {
            qInfo() << "[AIEngine] Reloading model for CPU backend:" << currentModelId;
            loadModel(currentModelId);
        }
        
        qInfo() << "[AIEngine] Switched to CPU backend";
    }

    emit backendTypeChanged(m_backendType);
    return true;
}
```

**优点**:
- 确保模型在正确的后端上加载
- 与现有 `loadModel` 逻辑一致
- 最小化代码修改

**缺点**:
- 切换后端时需要重新加载模型（有性能开销）

### 方案二：在 `setBackendType` 中直接设置 Vulkan 设备

**修改文件**: `src/core/AIEngine.cpp`

**修改内容**:
```cpp
bool AIEngine::setBackendType(BackendType type)
{
    // ... 现有检查代码 ...

    if (type == BackendType::NCNN_Vulkan) {
        if (!initializeVulkan()) {
            return false;
        }
        m_backendType = BackendType::NCNN_Vulkan;
        applyBackendOptions(BackendType::NCNN_Vulkan);
        
        // 【新增】直接设置 Vulkan 设备
        if (m_gpuAvailable.load() && m_gpuDeviceId >= 0) {
            m_net.set_vulkan_device(m_gpuDeviceId);
            qInfo() << "[AIEngine] Vulkan device set:" << m_gpuDeviceId;
        }
        
        qInfo() << "[AIEngine] Switched to Vulkan GPU backend";
    }
    // ...
}
```

**优点**:
- 不需要重新加载模型
- 切换更快

**缺点**:
- 可能存在模型状态不一致的风险
- NCNN 的 `set_vulkan_device` 在模型加载后调用可能有副作用

### 推荐方案

**采用方案一**，因为它更安全、更可靠，与现有的模型加载逻辑保持一致。

## 实施步骤

### 步骤 1：修复 `AIEngine::setBackendType`
- 文件: `src/core/AIEngine.cpp`
- 在切换后端后重新加载当前模型

### 步骤 2：修复视频处理配置
- 文件: `src/controllers/ProcessingController.cpp`
- 将 `config.useGpu = false` 改为使用实际的用户选择

### 步骤 3：添加日志验证
- 在关键路径添加日志，确认 GPU 推理是否生效
- 包括：后端切换、模型加载、推理执行

### 步骤 4：构建验证
- 使用 `qt-build-and-fix` 技能构建项目
- 确保无编译错误

### 步骤 5：功能测试
- 测试图片 GPU 推理
- 测试视频 GPU 推理
- 验证日志中显示 GPU 相关信息

## 验证方法

1. **日志验证**：检查日志中是否出现以下信息
   - `[AIEngine] Model will use Vulkan GPU device: X`
   - `[AIEngine] Switched to Vulkan GPU backend`
   - 推理时间应该明显缩短（GPU 比 CPU 快）

2. **功能验证**：
   - 图片处理：选择 GPU 模式，处理图片，检查输出
   - 视频处理：选择 GPU 模式，处理视频，检查输出

## 风险评估

| 风险 | 等级 | 缓解措施 |
|-----|-----|---------|
| 模型重新加载失败 | 中 | 回退到 CPU 模式并通知用户 |
| Vulkan 初始化失败 | 低 | 已有回退逻辑 |
| 性能开销 | 低 | 只在切换时发生一次 |

## 相关文件

- `src/core/AIEngine.cpp` - 主要修改
- `src/controllers/ProcessingController.cpp` - 配置修复
- `src/core/AIEnginePool.cpp` - 引擎池管理
- `src/core/video/AIVideoProcessor.cpp` - 视频处理
