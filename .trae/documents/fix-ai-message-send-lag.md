# AI 推理模式消息发送卡顿问题修复计划

## 问题分析

### 现象
用户在使用 `realesrgan_x4plus` 模型发送 AI 推理消息时，UI 出现约 2-3 秒的卡顿，消息才发送出去。

### 根本原因

从日志分析：
```
[2026-03-27 19:06:45.769] [INFO] [AIEngine][loadModel] loading model...
[2026-03-27 19:06:48.749] [INFO] [AIEngine][loadModel] model loaded successfully layers: 999
[2026-03-27 19:06:48.749] [INFO] [Perf][ProcessingController] startTask loadModel cost: 2980 ms
```

**核心问题：模型加载在主线程执行，阻塞 UI**

代码位置：[ProcessingController.cpp:660](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp#L660)

```cpp
// startTask 方法中，主线程同步加载模型
if (!engine->loadModel(modelId)) {  // <-- 阻塞主线程约 3 秒
    failTask(taskId, tr("模型加载失败: %1").arg(modelId));
    return;
}
```

### 问题链路

1. 用户点击发送 → `sendToProcessing()` → `addTask()` → `processNextTask()` → `startTask()`
2. `startTask()` 从引擎池获取引擎 → **主线程同步调用 `loadModel()`**
3. `loadModel()` 执行：读取 32MB 模型文件 → 初始化 NCNN 网络 → GPU 内存分配
4. 整个过程阻塞主线程约 3 秒，UI 无响应

### 现有预加载机制的缺陷

代码中已有 `requestModelPreload()` 方法，但存在以下问题：

1. **预加载对象错误**：预加载到 `m_aiEngine`（主引擎），但实际任务使用的是引擎池中的引擎
2. **引擎池隔离**：引擎池中每个引擎是独立实例，预加载不会影响池中引擎
3. **触发时机滞后**：预加载在 `addTask()` 时触发，用户已点击发送

---

## 解决方案

### 方案：异步模型加载 + 引擎池模型缓存

将模型加载从主线程移到工作线程，同时优化引擎池的模型管理。

---

## 实施步骤

### 步骤 1：修改 AIEngine 支持异步模型加载

**文件**：`src/core/AIEngine.cpp` / `include/EnhanceVision/core/AIEngine.h`

**修改内容**：
1. 添加 `loadModelAsync(const QString& modelId)` 方法
2. 内部使用 `QtConcurrent::run` 在工作线程执行加载
3. 加载完成后通过信号通知

**关键代码**：
```cpp
// AIEngine.h
void loadModelAsync(const QString& modelId);

signals:
    void modelLoadCompleted(bool success, const QString& modelId, const QString& error);

// AIEngine.cpp
void AIEngine::loadModelAsync(const QString& modelId)
{
    QtConcurrent::run([this, modelId]() {
        bool success = loadModel(modelId);  // 复用现有同步方法
        QString error;
        if (!success) {
            error = tr("模型加载失败: %1").arg(modelId);
        }
        emit modelLoadCompleted(success, modelId, error);
    });
}
```

### 步骤 2：修改 ProcessingController 使用异步加载

**文件**：`src/controllers/ProcessingController.cpp`

**修改内容**：
1. 在 `startTask()` 中改用 `loadModelAsync()`
2. 添加模型加载状态跟踪
3. 模型加载完成后再启动推理

**关键代码**：
```cpp
void ProcessingController::startTask(QueueTask& task)
{
    // ... 获取引擎 ...
    
    AIEngine* engine = m_aiEnginePool->acquire(taskId);
    connectAiEngineForTask(engine, taskId);
    
    // 检查引擎是否已加载正确模型
    if (engine->currentModelId() == modelId) {
        // 模型已加载，直接启动推理
        launchAiInference(engine, taskId, inputPath, outputPath, message);
    } else {
        // 异步加载模型
        m_pendingModelLoadTasks[modelId] = taskId;  // 跟踪加载中的任务
        
        connect(engine, &AIEngine::modelLoadCompleted, this, 
                [this, engine, taskId, inputPath, outputPath](bool success, const QString& modelId, const QString& error) {
            m_pendingModelLoadTasks.remove(modelId);
            if (success) {
                launchAiInference(engine, taskId, inputPath, outputPath, m_taskMessages[taskId]);
            } else {
                failTask(taskId, error);
            }
        }, Qt::SingleShotConnection);
        
        engine->loadModelAsync(modelId);
    }
}
```

### 步骤 3：优化引擎池模型缓存

**文件**：`src/core/AIEnginePool.cpp`

**修改内容**：
1. 添加 `ensureModelLoaded(const QString& modelId)` 方法
2. 引擎释放后保留模型加载状态
3. 按需预加载模型到空闲引擎

**关键代码**：
```cpp
// AIEnginePool.h
void warmupModel(const QString& modelId);  // 预热模型到所有空闲引擎

// AIEnginePool.cpp
void AIEnginePool::warmupModel(const QString& modelId)
{
    QMutexLocker locker(&m_mutex);
    for (auto& slot : m_slots) {
        if (!slot.inUse && slot.engine && slot.engine->currentModelId() != modelId) {
            slot.engine->loadModelAsync(modelId);  // 异步预加载
        }
    }
}
```

### 步骤 4：改进预加载触发时机

**文件**：`src/controllers/ProcessingController.cpp`

**修改内容**：
1. 在用户选择模型时（而非发送消息时）触发预加载
2. 需要暴露 QML 接口供前端调用

**关键代码**：
```cpp
// ProcessingController.h
Q_INVOKABLE void preloadModel(const QString& modelId);

// ProcessingController.cpp
void ProcessingController::preloadModel(const QString& modelId)
{
    if (m_aiEnginePool) {
        m_aiEnginePool->warmupModel(modelId);
    }
}
```

### 步骤 5：前端集成

**文件**：QML 中模型选择相关组件

**修改内容**：
1. 用户选择模型时调用 `processingController.preloadModel(modelId)`
2. 提前预热模型，减少发送时的延迟

---

## 验证方案

1. 构建项目
2. 运行应用，选择 `realesrgan_x4plus` 模型
3. 观察日志确认模型预加载
4. 发送 AI 推理消息，确认 UI 无卡顿
5. 检查日志中 `loadModel cost` 是否消失或显著降低

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 异步加载时引擎被其他任务占用 | 任务失败 | 引擎池 acquire 时标记占用状态 |
| 模型加载失败 | 任务失败 | 添加重试机制和错误提示 |
| 多任务并发加载同一模型 | 资源浪费 | 添加加载去重逻辑 |

---

## 文件修改清单

| 文件 | 修改类型 |
|------|----------|
| `include/EnhanceVision/core/AIEngine.h` | 添加方法声明 |
| `src/core/AIEngine.cpp` | 实现异步加载 |
| `include/EnhanceVision/core/AIEnginePool.h` | 添加预热方法 |
| `src/core/AIEnginePool.cpp` | 实现模型预热 |
| `include/EnhanceVision/controllers/ProcessingController.h` | 添加预加载接口 |
| `src/controllers/ProcessingController.cpp` | 改用异步加载 |
