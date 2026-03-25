# AI 模式全面检查计划

## 问题概述

用户报告 AI 模式在处理多媒体文件时存在两个关键问题：

1. **质量问题**：显示处理成功，但生成结果质量不佳，呈现半成品状态
2. **卡死问题**：处理过程长时间停留在"处理中"状态，无法完成

***

## 一、代码架构分析

### 核心组件关系

```
┌─────────────────────────────────────────────────────────────────┐
│                         QML UI Layer                             │
│  AIModelPanel.qml → AIParamsPanel.qml → ProcessingController    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ProcessingController                          │
│  - 任务队列管理                                                   │
│  - m_activeAiTaskId 跟踪当前 AI 任务                              │
│  - 状态同步到 MessageModel/SessionController                      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                          AIEngine                                │
│  - NCNN 模型加载与推理                                            │
│  - 分块处理 (processTiled)                                        │
│  - GPU OOM 自动降级                                              │
│  - 异步推理 (processAsync)                                        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        ModelRegistry                             │
│  - 模型元数据加载                                                 │
│  - 归一化参数配置                                                 │
│  - 模型可用性检查                                                 │
└─────────────────────────────────────────────────────────────────┘
```

***

## 二、问题根因假设

### 问题1：结果质量不佳（半成品状态）

| 假设编号 | 可能原因             | 代码位置                                           | 严重程度 |
| ---- | ---------------- | ---------------------------------------------- | ---- |
| Q1-1 | 分块处理时边缘拼接区域处理不当  | AIEngine.cpp:846-936 (processTiled)            | 高    |
| Q1-2 | 参数残留导致跨任务污染      | ProcessingController.cpp:626-634               | 高    |
| Q1-3 | 归一化/反归一化参数配置错误   | AIEngine.cpp:705-754 (qimageToMat/matToQimage) | 高    |
| Q1-4 | GPU OOM 后降级处理不完整 | AIEngine.cpp:316-346                           | 中    |
| Q1-5 | outscale 应用时机错误  | AIEngine.cpp:348-350                           | 中    |
| Q1-6 | 自动参数计算与用户参数冲突    | AIParamsPanel.qml:94-115 (getParams)           | 中    |
| Q1-7 | 模型推理输出通道数不匹配     | AIEngine.cpp:721-753 (matToQimage)             | 中    |

### 问题2：处理卡在"处理中"状态

| 假设编号 | 可能原因                    | 代码位置                                 | 严重程度 |
| ---- | ----------------------- | ------------------------------------ | ---- |
| S2-1 | m\_activeAiTaskId 未正确清理 | ProcessingController.cpp:139-162     | 高    |
| S2-2 | 信号槽连接断开或跨线程问题           | ProcessingController.cpp:128-163     | 高    |
| S2-3 | GPU OOM 后重试逻辑卡住         | AIEngine.cpp:340-346                 | 高    |
| S2-4 | 任务取消后状态未同步              | ProcessingController.cpp:235-298     | 中    |
| S2-5 | 推理互斥锁死锁                 | AIEngine.cpp:225 (m\_inferenceMutex) | 中    |
| S2-6 | 会话切换时任务状态丢失             | ProcessingController.cpp:1830-1871   | 中    |
| S2-7 | 文件保存失败但未触发错误信号          | AIEngine.cpp:458-463                 | 中    |

***

## 三、检查步骤

### 阶段1：日志与状态追踪检查

#### 1.1 添加详细日志追踪点

**文件**: `AIEngine.cpp`

```cpp
// 在 process() 方法入口添加
qInfo() << "[AIEngine][process] START"
        << "modelId:" << currentModelId
        << "inputSize:" << input.size()
        << "gpuEnabled:" << (m_gpuAvailable && m_useGpu)
        << "tileSize:" << tileSize
        << "effectiveParams:" << effectiveParams;

// 在 processTiled() 每个分块处理前后添加
qInfo() << "[AIEngine][Tiled] tile" << tileIndex
        << "region:(" << x0 << "," << y0 << ") - (" << x1 << "," << y1 << ")"
        << "padding:(" << px0 << "," << py0 << ") - (" << px1 << "," << py1 << ")"
        << "outputCrop:(" << outPadLeft << "," << outPadTop << ") size:" << outW << "x" << outH;

// 在推理完成时添加
qInfo() << "[AIEngine][process] END"
        << "resultSize:" << result.size()
        << "resultFormat:" << result.format()
        << "isNull:" << result.isNull();
```

**文件**: `ProcessingController.cpp`

```cpp
// 在 startTask() AI 推理分支添加
qInfo() << "[ProcessingController][AI] startTask"
        << "taskId:" << taskId
        << "modelId:" << modelId
        << "activeAiTaskId:" << m_activeAiTaskId;

// 在 processFileCompleted 回调添加
qInfo() << "[ProcessingController][AI] processFileCompleted"
        << "success:" << success
        << "resultPath:" << resultPath
        << "error:" << error
        << "activeAiTaskId:" << m_activeAiTaskId;
```

#### 1.2 检查点清单

- [ ] 确认 `m_activeAiTaskId` 在任务开始时正确设置
- [ ] 确认 `m_activeAiTaskId` 在任务完成/失败时正确清理
- [ ] 确认进度信号正常发射
- [ ] 确认状态信号正常发射
- [ ] 确认 GPU OOM 检测和降级日志

***

### 阶段2：分块处理逻辑检查

#### 2.1 分块拼接问题排查

**检查文件**: `AIEngine.cpp:846-936` (processTiled)

```cpp
// 检查点：
// 1. padding 计算是否正确
int px0 = std::max(x0 - padding, 0);
int py0 = std::max(y0 - padding, 0);

// 2. 输出裁剪区域是否越界
if (outPadLeft + outW > tileResult.width() || outPadTop + outH > tileResult.height()) {
    // 问题：边界检查已存在，但可能跳过处理导致黑块
}

// 3. 分块结果是否正确绘制
painter.drawImage(x0 * scale, y0 * scale, croppedResult);
```

**潜在问题**:

- 边界检查失败时跳过处理，可能导致输出图像出现黑块
- padding 区域处理不当可能导致拼接缝隙

#### 2.2 修复建议

```cpp
// 在 processTiled 中，当边界检查失败时不应跳过，而应尝试调整
if (outPadLeft + outW > tileResult.width() || outPadTop + outH > tileResult.height()) {
    qWarning() << "[AIEngine][Tiled] tile" << tileIndex << "crop region adjusted";
    outW = std::min(outW, tileResult.width() - outPadLeft);
    outH = std::min(outH, tileResult.height() - outPadTop);
}
```

***

### 阶段3：参数传递与残留检查

#### 3.1 参数残留问题排查

**检查文件**: `ProcessingController.cpp:626-634`

```cpp
// 当前代码：
m_aiEngine->clearParameters();  // 清空参数
if (message.aiParams.tileSize > 0) {
    m_aiEngine->setParameter("tileSize", message.aiParams.tileSize);
}
for (auto it = message.aiParams.modelParams.begin(); ...) {
    m_aiEngine->setParameter(it.key(), it.value());
}
```

**潜在问题**:

- `clearParameters()` 后立即设置参数，但自动参数可能在之后覆盖
- 用户手动参数与自动参数优先级不明确

#### 3.2 修复建议

```cpp
// 明确参数优先级：用户参数 > 自动参数 > 默认参数
m_aiEngine->clearParameters();

// 1. 先应用自动参数（最低优先级）
QVariantMap autoParams = m_aiEngine->computeAutoParams(mediaSize, isVideo);
for (auto it = autoParams.begin(); it != autoParams.end(); ++it) {
    if (it.key() != "tileSize") {  // tileSize 单独处理
        m_aiEngine->setParameter(it.key(), it.value());
    }
}

// 2. 再应用用户参数（最高优先级）
for (auto it = message.aiParams.modelParams.begin(); ...) {
    m_aiEngine->setParameter(it.key(), it.value());
}
```

***

### 阶段4：GPU OOM 处理检查

#### 4.1 GPU OOM 降级逻辑检查

**检查文件**: `AIEngine.cpp:316-346`

```cpp
// 当前逻辑：
// 1. 检测 GPU OOM (extractRet == -100)
// 2. 设置 m_gpuOomDetected 标志
// 3. 重试时使用分块模式

// 潜在问题：
// - m_gpuOomDetected 是全局标志，可能影响后续所有任务
// - 重试逻辑可能无限循环
```

#### 4.2 修复建议

```cpp
// 在任务完成或失败后重置 GPU OOM 标志
void AIEngine::processAsync(...) {
    // ... 推理完成后 ...
    
    // 如果本次推理成功，重置 GPU OOM 标志
    if (!result.isNull()) {
        m_gpuOomDetected.store(false);
    }
}

// 或者添加重试次数限制
int retryCount = 0;
const int maxRetries = 2;
while (result.isNull() && m_gpuOomDetected.load() && retryCount < maxRetries) {
    // ... 重试逻辑 ...
    retryCount++;
}
```

***

### 阶段5：状态同步与信号检查

#### 5.1 信号槽连接检查

**检查文件**: `ProcessingController.cpp:128-163`

```cpp
// 当前连接：
connect(m_aiEngine, &AIEngine::processFileCompleted, this, [...]);

// 潜在问题：
// - 使用 lambda 连接，如果 ProcessingController 被销毁可能导致连接断开
// - 跨线程信号需要使用 Qt::QueuedConnection
```

#### 5.2 修复建议

```cpp
// 使用 Qt::QueuedConnection 确保跨线程安全
connect(m_aiEngine, &AIEngine::processFileCompleted, this,
        [this](bool success, const QString& resultPath, const QString& error) {
            // ... 处理逻辑 ...
        }, Qt::QueuedConnection);

// 或者使用 QMetaObject::invokeMethod 确保在主线程执行
```

#### 5.3 m\_activeAiTaskId 状态检查

**检查文件**: `ProcessingController.cpp:139-162`

```cpp
// 当前代码：
const QString finishedTaskId = m_activeAiTaskId;
m_activeAiTaskId.clear();  // 立即清理

// 潜在问题：
// - 如果 completeTask 内部发生异常，m_activeAiTaskId 已清理但任务未完成
```

#### 5.4 修复建议

```cpp
// 使用 RAII 模式管理 activeAiTaskId
class ActiveTaskGuard {
public:
    ActiveTaskGuard(QString& taskId, const QString& newId) 
        : m_taskId(taskId) { m_taskId = newId; }
    ~ActiveTaskGuard() { m_taskId.clear(); }
private:
    QString& m_taskId;
};

// 在 startTask 中使用
ActiveTaskGuard guard(m_activeAiTaskId, taskId);
```

***

### 阶段6：归一化参数检查

#### 6.1 模型配置检查

**检查文件**: `resources/models/models.json`

```json
// 确认每个模型的归一化参数配置正确
{
  "id": "realesrgan-x4plus",
  "normMean": [0.0, 0.0, 0.0],
  "normScale": [0.003921568627, 0.003921568627, 0.003921568627],  // 1/255
  "denormMean": [0.0, 0.0, 0.0],
  "denormScale": [255.0, 255.0, 255.0]
}
```

#### 6.2 转换函数检查

**检查文件**: `AIEngine.cpp:705-754`

```cpp
// qimageToMat: RGB888 -> ncnn::Mat
// matToQimage: ncnn::Mat -> RGB888

// 检查点：
// 1. 输入图像格式转换是否正确
// 2. 归一化参数应用顺序是否正确
// 3. 输出值范围是否正确 (0-255)
```

***

## 四、测试用例

### 测试用例1：单图 AI 增强

1. 上传一张 1920x1080 图片
2. 选择超分辨率模型 (4x)
3. 观察日志输出
4. 检查输出图像质量和尺寸

**预期结果**: 输出图像应为 7680x4320，无黑块或拼接痕迹

### 测试用例2：多图批量处理

1. 上传 5 张不同尺寸图片
2. 选择同一模型处理
3. 观察任务队列状态
4. 检查每张图片处理结果

**预期结果**: 所有图片正确处理，状态正确更新

### 测试用例3：GPU OOM 场景

1. 上传一张超大图片 (8K+)
2. 强制使用 GPU 模式
3. 观察是否触发 OOM 降级
4. 检查降级后处理结果

**预期结果**: 自动降级到分块模式，处理成功完成

### 测试用例4：取消任务

1. 开始处理一张大图
2. 在处理过程中点击取消
3. 观察状态是否正确更新
4. 检查后续任务是否能正常启动

**预期结果**: 任务正确取消，状态更新为 Cancelled

### 测试用例5：会话切换

1. 开始处理任务
2. 切换到另一个会话
3. 再切换回来
4. 检查任务状态

**预期结果**: 任务状态正确保持

***

## 五、修复优先级

| 优先级 | 问题                     | 修复方案                |
| --- | ---------------------- | ------------------- |
| P0  | m\_activeAiTaskId 状态管理 | 使用 RAII 模式管理        |
| P0  | 参数残留问题                 | 明确参数优先级             |
| P1  | 分块拼接边界处理               | 调整边界检查逻辑            |
| P1  | GPU OOM 无限重试           | 添加重试次数限制            |
| P2  | 跨线程信号安全                | 使用 QueuedConnection |
| P2  | 日志追踪不足                 | 添加详细日志              |

***

## 六、实施步骤

### 第一步：添加诊断日志

1. 在 AIEngine 关键方法添加详细日志
2. 在 ProcessingController 状态变更处添加日志
3. 构建并运行，收集问题复现时的日志

### 第二步：修复状态管理问题

1. 实现 ActiveTaskGuard RAII 类
2. 修改 startTask 和 processFileCompleted 方法
3. 测试任务状态流转

### 第三步：修复参数处理问题

1. 明确参数优先级逻辑
2. 修改 ProcessingController 参数传递代码
3. 测试参数传递正确性

### 第四步：修复分块处理问题

1. 修改边界检查逻辑
2. 添加分块处理失败重试
3. 测试大图处理

### 第五步：修复 GPU OOM 处理

1. 添加重试次数限制
2. 重置 GPU OOM 标志
3. 测试 GPU OOM 场景

### 第六步：全面测试

1. 执行所有测试用例
2. 检查日志无警告/错误
3. 确认问题修复

***

## 七、验收标准

1. **质量标准**:
   - AI 处理结果无黑块、拼接痕迹
   - 输出图像尺寸正确
   - 颜色无异常
2. **状态标准**:
   - 任务状态正确更新
   - 进度正确显示
   - 取消功能正常
3. **稳定性标准**:
   - 无卡死现象
   - GPU OOM 自动恢复
   - 多任务并发正常
4. **日志标准**:
   - 无警告信息
   - 无错误信息
   - 无异常堆栈

