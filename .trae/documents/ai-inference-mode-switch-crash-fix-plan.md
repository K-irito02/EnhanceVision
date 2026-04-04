# AI 推理模式切换崩溃问题修复计划

## 问题概述

**创建日期**: 2026-04-05
**问题类型**: CPU→GPU 推理模式切换时崩溃
**状态**: 🔄 分析中

---

## 一、问题现象

当系统中存在两个推理任务（CPU推理任务和GPU推理任务）时：
1. CPU推理任务成功完成
2. 在准备处理GPU推理任务的过渡阶段，应用程序发生崩溃并闪退

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

### 2.1 核心问题

1. **Vulkan 实例生命周期管理缺陷**
   - `probeGpuAvailability()` 在启动时创建并销毁 Vulkan 实例
   - `setBackendType()` 切换到 GPU 时重新初始化
   - 可能存在资源竞争或状态不一致

2. **引擎池获取与释放时序问题**
   - `AIEnginePool::acquireWithBackend()` 调用 `resetState()` 和 `setBackendType()`
   - `setBackendType()` 会触发 Vulkan 初始化和模型重新加载
   - 如果上一个任务刚完成，引擎可能未完全清理

3. **模型加载与推理同步问题**
   - 模型加载后 GPU 资源需要时间初始化
   - 推理可能过早开始，导致访问未就绪的资源

4. **NCNN Vulkan Allocator 生命周期问题**
   - 根据 `ai-inference-crash-fix-notes.md`，allocator 与 ncnn::Net 绑定
   - 后端切换时模型重新加载，allocator 可能处于不一致状态

### 2.2 调用链分析

```
CPU任务完成
  └─→ ProcessingController::completeTask()
      └─→ disconnectAiEngineForTask()
      └─→ AIEnginePool::release(taskId)
          └─→ engine->cancelProcess() [如果还在处理]
          └─→ m_slots[idx].state = EngineState::Ready

GPU任务开始
  └─→ ProcessingController::processNextTask()
      └─→ AIEnginePool::acquireWithBackend(taskId, BackendType::NCNN_Vulkan)
          └─→ engine->resetState()  // 重置状态
          └─→ engine->setBackendType(BackendType::NCNN_Vulkan)
              └─→ initializeVulkan()  // 初始化 Vulkan
              └─→ loadModel()  // 重新加载模型
          └─→ launchAiInference()
              └─→ engine->processAsync()
                  └─→ process() → processTiled() → runInference()
                      └─→ ex.extract() → 💥 崩溃
```

### 2.3 可能的崩溃原因

1. **Vulkan 实例重复创建/销毁**
   - `probeGpuAvailability()` 销毁实例后，`initializeVulkan()` 重新创建
   - 可能导致 GPU 资源状态不一致

2. **模型加载后 GPU 资源未就绪**
   - `loadModel()` 完成后立即开始推理
   - GPU pipeline 可能还在初始化中

3. **内存/资源竞争**
   - CPU 推理后的清理与 GPU 推理的初始化存在竞争
   - 某些资源可能被过早释放或重复访问

---

## 三、解决方案

### 3.1 方案一：Vulkan 实例单例化管理（推荐）

**目标**: 确保 Vulkan 实例全局只创建一次，避免重复创建/销毁导致的状态不一致

**修改文件**:
- `src/core/AIEngine.cpp`
- `include/EnhanceVision/core/AIEngine.h`

**实现要点**:
1. 使用 `std::call_once` 确保 `ncnn::create_gpu_instance()` 只调用一次
2. 使用引用计数管理实例生命周期
3. `probeGpuAvailability()` 不再销毁实例，只做探测

### 3.2 方案二：引擎池后端切换同步机制

**目标**: 确保后端切换时引擎状态完全清理，GPU 资源完全就绪

**修改文件**:
- `src/core/AIEnginePool.cpp`
- `src/core/AIEngine.cpp`

**实现要点**:
1. `acquireWithBackend()` 中添加后端切换等待机制
2. `setBackendType()` 添加 GPU 就绪等待
3. 添加模型加载同步屏障

### 3.3 方案三：推理前 GPU 资源验证

**目标**: 在推理开始前验证 GPU 资源状态，避免访问未就绪资源

**修改文件**:
- `src/core/AIEngine.cpp`

**实现要点**:
1. `runInference()` 前添加 GPU 就绪检查
2. 添加 Vulkan 队列同步
3. 添加 allocator 有效性验证

### 3.4 方案四：任务调度优化

**目标**: 优化任务切换时序，避免资源竞争

**修改文件**:
- `src/controllers/ProcessingController.cpp`

**实现要点**:
1. 任务完成后添加短暂延迟再处理下一个任务
2. 不同后端任务之间添加冷却期
3. 优化引擎释放和获取的时序

---

## 四、实施步骤

### 阶段一：核心修复（P0）

| 步骤 | 任务 | 文件 | 优先级 |
|------|------|------|--------|
| 1 | 实现 Vulkan 实例单例化管理 | AIEngine.cpp/h | P0 |
| 2 | 修复 `probeGpuAvailability()` 不销毁实例 | AIEngine.cpp | P0 |
| 3 | 添加 `setBackendType()` GPU 就绪等待 | AIEngine.cpp | P0 |
| 4 | 添加模型加载同步屏障 | AIEngine.cpp | P0 |

### 阶段二：引擎池优化（P1）

| 步骤 | 任务 | 文件 | 优先级 |
|------|------|------|--------|
| 5 | 优化 `acquireWithBackend()` 等待机制 | AIEnginePool.cpp | P1 |
| 6 | 添加引擎状态验证 | AIEnginePool.cpp | P1 |
| 7 | 优化后端切换时序 | AIEnginePool.cpp | P1 |

### 阶段三：推理保护（P1）

| 步骤 | 任务 | 文件 | 优先级 |
|------|------|------|--------|
| 8 | 添加推理前 GPU 资源验证 | AIEngine.cpp | P1 |
| 9 | 添加 Vulkan 队列同步点 | AIEngine.cpp | P1 |
| 10 | 添加 allocator 有效性检查 | AIEngine.cpp | P1 |

### 阶段四：任务调度优化（P2）

| 步骤 | 任务 | 文件 | 优先级 |
|------|------|------|--------|
| 11 | 添加任务切换冷却期 | ProcessingController.cpp | P2 |
| 12 | 优化引擎释放时序 | ProcessingController.cpp | P2 |
| 13 | 添加后端切换日志追踪 | 多个文件 | P2 |

---

## 五、测试计划

### 5.1 功能测试

| 测试场景 | 预期结果 |
|----------|----------|
| CPU → GPU 任务顺序 | 无崩溃，正常完成 |
| GPU → CPU 任务顺序 | 无崩溃，正常完成 |
| CPU → Shader → GPU 顺序 | 无崩溃，正常完成 |
| GPU → Shader → CPU 顺序 | 无崩溃，正常完成 |
| Shader → CPU → GPU 顺序 | 无崩溃，正常完成 |
| 相同后端连续任务 | 无崩溃，正常完成 |

### 5.2 媒体类型测试

| 测试场景 | 预期结果 |
|----------|----------|
| CPU 图像推理 | 正常完成 |
| CPU 视频推理 | 正常完成 |
| GPU 图像推理 | 正常完成 |
| GPU 视频推理 | 正常完成 |
| 混合媒体类型任务 | 无崩溃 |

### 5.3 压力测试

| 测试场景 | 预期结果 |
|----------|----------|
| 快速连续提交多个任务 | 无崩溃 |
| 任务取消后重新提交 | 无崩溃 |
| 模型切换后推理 | 无崩溃 |
| 长时间运行稳定性 | 无内存泄漏 |

---

## 六、风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| Vulkan 单例化可能影响多 GPU 场景 | 中 | 设计支持多 GPU 的扩展接口 |
| 同步等待可能影响性能 | 低 | 使用条件变量高效等待 |
| 修改范围较大可能引入新问题 | 中 | 分阶段实施，每阶段充分测试 |

---

## 七、参考文档

- `docs/Notes/ai-inference-crash-fix-notes.md` - 首次任务崩溃修复笔记
- `docs/Notes/cpu-video-inference-crash-fix-notes.md` - CPU 视频推理崩溃修复笔记
