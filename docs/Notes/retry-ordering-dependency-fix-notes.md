# 重试顺序依赖修复笔记

## 概述

修复多任务失败时必须按顺序重新处理的问题。当有多个任务处于 Failed 状态时，用户必须从首个失败任务开始按顺序重试，否则重试不起作用。

**创建日期**: 2026-04-02
**作者**: AI Assistant
**相关文件**: ProcessingController

---

## 一、问题描述

### 问题现象

当消息中有多个文件的处理任务全部失败时：
1. 直接点击非首个失败文件的「重新处理」按钮无反应
2. 必须从第一个失败的任务开始依次重试，后续任务才能成功重试
3. 用户体验差，操作不符合直觉

---

## 二、根因分析

### 核心原因：`hasTasksForMessage()` 粗粒度阻塞

**位置**: `ProcessingController.cpp:342-358`

`hasTasksForMessage()` 检查的是 **该消息是否有任何非终止状态的任务**（即不是 Completed/Failed/Cancelled 的任务）。此方法被用作 `retrySingleFailedFile()` 和 `retryFailedFiles()` 的前置守卫。

### 问题场景复现

```
1. 消息有3个文件 A, B, C，全部处理失败
   m_tasks = [taskA(Failed), taskB(Failed), taskC(Failed)]

2. 用户点击重试 B → retrySingleFailedFile(msgId, 1)
   → hasTasksForMessage() → 全部 Failed → 返回 false → ✅ 放行
   → 创建 B_new(Pending)，追加到 m_tasks 末尾
   → processNextTask() → 找到 B_new → 开始处理 (Processing)

3. 用户接着点击重试 C → retrySingleFailedFile(msgId, 2)
   → hasTasksForMessage() → 发现 B_new 是 Processing! → 返回 true → ❌ 被阻塞!
```

先重试的任务进入 `Processing` 状态后，`hasTasksForMessage()` 会误将同消息的其他重试请求阻塞。

### 次要问题：旧失败任务不清理

`retrySingleFailedFile()` 和 `retryFailedFiles()` 创建新任务时不移除 `m_tasks` 中旧的 `Failed` 任务条目，导致多次重试后 `m_tasks` 积累大量已终止的旧条目。

---

## 三、解决方案

### 修改点 1：新增细粒度检查方法

**文件**: `ProcessingController.h` / `ProcessingController.cpp`

新增 `hasActiveTaskForFile(const QString& messageId, const QString& fileId)` 方法：
- 仅检查**指定文件**是否有 `Pending`/`Processing` 状态的任务
- 替代原有的粗粒度 `hasTasksForMessage()` 守卫

### 修改点 2：新增旧任务清理方法

**文件**: `ProcessingController.h` / `ProcessingController.cpp`

新增 `removeStaleTasksForFile(const QString& messageId, const QString& fileId)` 方法：
- 从 `m_tasks` 中移除匹配 `messageId + fileId` 且状态为 `Failed`/`Cancelled`/`Completed` 的旧条目
- 对每个移除的条目调用 `cleanupTask()` 释放关联资源
- 清理后调用 `updateQueuePositions()` 和 `syncModelTasks()` 同步状态

### 修改点 3：修改 `retrySingleFailedFile()`

- 将 `hasTasksForMessage(messageId)` 守卫替换为 `hasActiveTaskForFile(messageId, file.id)`
- 在创建新任务前调用 `removeStaleTasksForFile(messageId, file.id)` 清理旧条目

### 修改点 4：修改 `retryFailedFiles()`

- 移除 `hasTasksForMessage` 整体守卫
- 改为逐文件使用 `hasActiveTaskForFile` 跳过已有活动任务的文件
- 在批量创建新任务前清理所有待重试文件的旧失败条目
- 允许多个失败文件同时入队（`processNextTask` 串行逐个处理）

---

## 四、变更文件清单

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `include/EnhanceVision/controllers/ProcessingController.h` | 修改 | 新增 2 个方法声明 |
| `src/controllers/ProcessingController.cpp` | 修改 | 修改 3 个方法 + 新增 2 个方法实现 |

---

## 五、测试验证

| 测试场景 | 预期结果 | 实际结果 |
|----------|----------|----------|
| 3个文件全部失败，直接重试第2个 | 成功重试第2个 | ✅ 通过 |
| 3个文件全部失败，直接重试第3个 | 成功重试第3个 | ✅ 通过 |
| 先重试第2个，再重试第1个 | 均可成功重试 | ✅ 通过 |
| 批量重试所有失败文件 | 所有文件依次入队处理 | ✅ 通过 |
| 单个文件重复重试 | 第2次被细粒度守卫拦截 | ✅ 通过 |
| 正常处理流程（无失败） | 不受影响 | ✅ 通过 |

---

## 六、影响范围评估

- **风险等级**: 低
- **影响范围**: 仅限重试入站逻辑，不影响正在运行的任务处理管道
- **向后兼容**: 功能超集兼容（原行为是串行重试，修复后支持并行入队+串行执行）
