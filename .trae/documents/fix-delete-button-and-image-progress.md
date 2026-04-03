# 修复计划：消息卡片删除按钮显示 & CPU图片推理进度条问题

## 问题分析

### 问题1：删除按钮在特定状态下不显示

**根因定位**：[MessageItem.qml L329](../qml/components/MessageItem.qml#L329)

```qml
visible: root.status === 2 || root.status === 3 || root.successFileCount > 0 || root.hasFailedFiles || root.allFilesPending
```

当前操作按钮行（Row）使用复杂的状态组合条件控制可见性。当消息卡片中所有任务都是"正在处理"或"待处理"状态时，条件不满足，整个操作行被隐藏，导致删除按钮不可见。

**修复方案**：删除按钮应始终显示（只要消息卡片存在/有文件），移除所有状态相关条件。将操作按钮行的 `visible` 简化为 `root.totalFileCount > 0`，即只要有文件就显示操作行。删除按钮自身的 visible 也保持为 `root.totalFileCount > 0` 即可。

修改 L329：
```qml
// 旧: visible: root.status === 2 || root.status === 3 || root.successFileCount > 0 || root.hasFailedFiles || root.allFilesPending
// 新: visible: root.totalFileCount > 0
```

同时确认删除行为：点击删除后清除所有状态的文件（待处理、处理中、成功、失败全部删除），需验证现有 `deleteClicked()` 信号的处理逻辑已支持此行为。

### 问题2：CPU模式下图片推理进度条不移动

**根因定位**：[AIEngine.cpp processSingle() L915-945](../src/core/AIEngine.cpp#L915-L945) 和 [AIInferenceWorker.cpp processImage() L276-290](../src/core/ai/AIInferenceWorker.cpp#L276-L290)

进度更新链路分析：

```
AIInferenceWorker::processImage()
  ├── updateProgress(0.05) "加载图像..."     ← 有更新
  ├── updateProgress(0.10) "加载模型..."     ← 有更新
  ├── updateProgress(0.15) "AI推理中..."     ← 有更新
  ├── 连接 AIEngine::progressChanged 信号    ← 映射到 0.15~0.90
  ├── m_engine->process(inputImage)          ← 阻塞调用！
  │     └── AIEngine::processSingle()
  │           ├── setProgress(0.02)          ← 立即被覆盖
  │           ├── setProgress(0.05)
  │           ├── qimageToMat()              ← 无中间进度
  │           ├── setProgress(0.15)
  │           ├── runInference()             ← ★ 核心阻塞点，无任何中间进度！
  │           ├── setProgress(0.85)
  │           └── setProgress(0.95)
  ├── updateProgress(0.92) "保存结果..."
  └── updateProgress(1.0) "完成"
```

**关键问题**：`runInference()` 内部调用 `ncnn::Extractor::extract()` 是一个**原子阻塞操作**，NCNN 不提供推理过程中的中间回调。对于小图（走 `processSingle` 路径），从 15% 到 85% 的进度完全空白，直到推理结束后直接跳到 100%。

对比视频处理（走 `AIVideoProcessor`，逐帧报告进度）和分块图像处理（走 `processTiled`，逐块报告进度），两者都有自然的进度更新点。

**修复方案**：在 `AIEngine::processSingle()` 的 `runInference()` 阻塞调用期间，启动一个**基于时间的渐进式进度模拟器**，在独立线程/定时器中以平滑速率推进进度，推理完成后停止并设置真实进度值。需考虑同一卡片中图片+视频混合任务的情况——图片任务的进度模拟不应干扰视频任务的帧级精确进度。

---

## 实施步骤

### 步骤1：修复消息卡片删除按钮 — 始终显示

**文件**：`qml/components/MessageItem.qml`

**修改 L329**：将操作按钮行可见性条件简化为仅判断是否有文件

将：
```qml
visible: root.status === 2 || root.status === 3 || root.successFileCount > 0 || root.hasFailedFiles || root.allFilesPending
```
改为：
```qml
visible: root.totalFileCount > 0
```

**验证要点**：
- 任何状态下（待处理/处理中/成功/失败/混合）→ 删除按钮始终显示 ✅
- 空卡片（无文件）→ 操作行隐藏 ✅
- 删除功能：点击后清除所有状态的文件

**注意**：操作行内的各个子按钮（重试、下载、删除）仍保留各自的可见性条件：
- 重试按钮：`visible: root.hasFailedFiles`（仅失败时显示）
- 下载按钮：`visible: root.successFileCount > 0`（仅成功时显示）
- 删除按钮：保持 `root.totalFileCount > 0`（始终显示）

### 步骤2：为 CPU 图片推理添加渐进式进度机制

**文件**：`src/core/AIEngine.cpp` — `processSingle()` 方法

**方案设计**：

在 `processSingle()` 中，在调用 `runInference()` 前启动一个基于 `QElapsedTimer` 的渐进式进度更新：

1. 记录 `runInference()` 调用前的起始时间和起始进度（0.15）
2. 调用 `runInference()`（阻塞）
3. 根据**实际经过时间**计算并发出模拟的中间进度值

由于 `runInference()` 是同步阻塞调用，无法在其执行过程中发射信号。因此采用**事后补偿策略**：在 `runInference()` 返回后，如果发现推理耗时超过阈值（如 500ms），则快速补发一系列递增的进度值到信号槽系统，让 UI 层看到平滑过渡。

但更优的做法是使用**异步进度模拟**：在 `processSingle()` 内部，将 `runInference()` 放到一个独立的 lambda 中运行，同时在主引擎对象上通过定时器或元对象调用持续发射渐进进度。

**最终采用方案 — 推理前启动进度模拟器**：

修改 `AIEngine::processSingle()`：
```
1. setProgress(0.15)  // 进入推理阶段
2. 记录 startTime = QElapsedTimer::start()
3. 设置 m_inferProgressSimulator = true, m_inferStartProgress = 0.15
4. 调用 runInference(in, model)  // 阻塞
5. 清除模拟器标志
6. setProgress(0.85)  // 推理结束
```

同时在 `AIEngine` 中新增一个 `QTimer` 或利用现有的进度报告机制，当检测到 `m_inferProgressSimulator == true` 时，以固定间隔（如 100ms）根据经过时间线性插值计算并发射进度值（0.15 → 目标 0.80）。

**具体实现**：

1. 在 `AIEngine.h` 中添加成员变量：
   - `std::atomic<bool> m_simulatingProgress{false}`
   - `QElapsedTimer m_inferStartTime`
   - `double m_simulateStartProgress = 0.0`
   - `double m_simulateTargetProgress = 0.80`
   - `qint64 m_simulateEstimatedMs = 3000` （默认估计推理时长）

2. 修改 `processSingle()`：
   - 在 `runInference()` 前启动模拟
   - 在 `runInference()` 后停止模拟

3. 新增 `_updateSimulatedProgress()` 方法，由外部定时器或在 `setProgress()` 中触发检查

4. 修改 `AIInferenceWorker::processImage()` 中的进度连接 lambda，确保模拟进度能正确传播

### 步骤3：处理图片+视频混合任务的进度隔离

**关键考量**：一个消息卡片可能同时包含图片和视频任务。图片任务的模拟进度不应影响视频任务的精确帧级进度。

**解决方案**：
- 进度模拟仅在 `processSingle()`（非分块、非TTA、非视频）路径中激活
- 视频路径（`processVideo` → `AIVideoProcessor`）不受影响
- 分块图片路径（`processTiled`）本身已有逐块进度，不需要模拟
- TTA 路径（`processWithTTA`）本身有逐步骤进度，不需要模拟
- 通过 `AIMediaType` 或处理上下文区分，确保仅对纯图单次推理启用模拟

### 步骤4：构建验证与测试

1. 使用 `qt-build-and-fix` 技能构建项目
2. 运行应用并验证：
   - **删除按钮测试**：创建一个包含多个待处理/处理中文件的卡片，确认右上角出现删除按钮；点击删除确认所有文件被清除
   - **图片进度测试**：选择 CPU 模式，提交一张图片任务，观察进度条是否平滑移动而非静止后跳跃
   - **混合任务测试**：同时提交图片+视频，确认两者进度独立正确更新
3. 检查日志无警告/错误

---

## 影响范围评估

| 文件 | 变更类型 | 风险等级 |
|------|----------|----------|
| `qml/components/MessageItem.qml` | 简化可见性条件为 `totalFileCount > 0` | 低 — 纯简化，移除状态判断 |
| `src/core/AIEngine.cpp` | 新增进度模拟逻辑 | 中 — 需仔细处理线程安全 |
| `include/EnhanceVision/core/AIEngine.h` | 新增成员变量 | 低 — 仅添加字段 |

## 回归风险控制

- 删除按钮：从复杂条件简化为单一条件（有文件即显示），不影响任何现有功能
- 进度模拟：仅在 `processSingle()` 路径中生效，`processTiled`/`processWithTTA`/视频处理路径完全不变
- 模拟进度不会超过设定的目标上限（0.80），`runInference()` 返回后会立即设置为真实值 0.85
