# 队列继续处理问题修复计划

## 问题分析

### 现象描述
用户添加多个处理任务，在第一个任务（CPU子模式处理图片文件）处理时点击暂停，队列没有继续处理其他任务。

### 日志分析
从 `runtime_output.log` 中可以看到：
1. 第33行：开始处理第一个任务 `da5b27b8-efe0-4767-87e8-d87a80470448`
2. 第47行：用户点击暂停，调用 `pauseMessageTasks`，模式为 0
3. 第54-55行：减少处理计数，显示 "Mode 0: Single task paused, queue continues"
4. 第56-58行：异步释放AI引擎
5. **关键问题**：之后没有看到下一个任务开始处理的日志

### 根本原因

在 [ProcessingController.cpp:449-452](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp#L449-L452) 中，`processNextTask` 方法会跳过所有属于暂停消息的任务：

```cpp
// 检查消息是否被暂停，跳过已暂停消息的任务
if (m_pausedMessageIds.contains(task.messageId)) {
    hasSkippedPausedTask = true;
    continue;
}
```

**问题**：这个逻辑没有考虑暂停模式的差异：
- **模式0（单任务暂停）**：应该只暂停当前正在处理的任务，队列继续处理该消息的其他待处理任务
- **模式1（顺序暂停）**：暂停整个消息，队列被阻塞
- **模式2（自由选择）**：全局暂停，队列被阻塞

当前代码在所有模式下都会跳过暂停消息的所有任务，导致模式0无法正常工作。

## 修复方案

### 修改位置
[ProcessingController.cpp](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp) 的 `processNextTask` 方法

### 修改内容

在 `processNextTask` 方法中，需要根据暂停模式来决定是否跳过暂停消息的任务：

**修改前**（第448-452行）：
```cpp
// 检查消息是否被暂停，跳过已暂停消息的任务
if (m_pausedMessageIds.contains(task.messageId)) {
    hasSkippedPausedTask = true;
    continue;
}
```

**修改后**：
```cpp
// 检查消息是否被暂停，根据暂停模式决定是否跳过
if (m_pausedMessageIds.contains(task.messageId)) {
    // 模式0（单任务暂停）：不跳过，只跳过状态为 Paused 的任务
    // 模式1和模式2：跳过暂停消息的所有任务
    if (pauseMode != 0) {
        hasSkippedPausedTask = true;
        continue;
    }
    // 模式0：只跳过状态为 Paused 的任务，不跳过 Pending 的任务
    if (task.status == ProcessingStatus::Paused) {
        hasSkippedPausedTask = true;
        continue;
    }
}
```

## 实施步骤

1. **修改 `processNextTask` 方法**
   - 文件：`src/controllers/ProcessingController.cpp`
   - 位置：第448-452行
   - 内容：根据暂停模式调整跳过逻辑

2. **验证修复**
   - 使用 `qt-build-and-fix` 技能构建项目
   - 运行应用程序
   - 测试场景：
     - 添加多个处理任务（至少2个不同消息的任务）
     - 在第一个任务处理时点击暂停
     - 验证队列是否继续处理其他任务
   - 检查日志文件，确认没有错误和警告

## 预期结果

修复后，在模式0下：
1. 点击暂停时，当前正在处理的任务被暂停（状态变为 Paused）
2. 队列继续处理下一个待处理的任务（即使属于同一个消息）
3. 日志中应该看到下一个任务开始处理的记录

## 风险评估

- **影响范围**：仅影响暂停逻辑，不影响其他功能
- **风险等级**：低
- **回滚方案**：如果出现问题，可以快速回滚修改

## 测试要点

1. **模式0测试**
   - 单个消息多个任务：暂停第一个任务，验证队列继续处理其他任务
   - 多个消息：暂停一个消息的任务，验证队列继续处理其他消息的任务

2. **模式1测试**
   - 验证暂停一个消息后，队列被阻塞

3. **模式2测试**
   - 验证全局暂停功能正常工作

4. **边界情况**
   - 队列为空时暂停
   - 所有任务都已暂停时继续处理
   - 暂停后立即恢复
