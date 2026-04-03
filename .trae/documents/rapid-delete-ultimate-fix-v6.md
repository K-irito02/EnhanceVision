# 快速连续删除崩溃 — 终极修复方案 v6

> **目标**：彻底修复 Shader 和 AI 推理两种模式在高压连续快速删除时的崩溃问题。

## 1. 根因分析（最终确认）

### 1.1 核心问题：生命周期检查遗漏 `Draining` 状态

经过深入代码分析，发现**大部分回调函数的生命周期检查遗漏了 `Draining` 状态**：

| 文件位置 | 方法/回调 | 是否检查 Draining | 问题 |
|----------|-----------|-------------------|------|
| L168-170 | `cancelTask()` | ❌ | Draining 任务仍会被取消 |
| L559-561 | `ImageProcessor::finished` | ❌ | Draining 图片任务仍会完成 |
| L651-653 | `AIEngine::processFileCompleted` | ❌ | Draining AI任务仍会完成 |
| L693-695 | `AIEngine::modelLoadCompleted` | ❌ | Draining 模型加载仍会完成 |
| **L746-748** | **`completeTask()`** | ❌ | **Draining 任务仍会完成！** |
| **L1024-1026** | **`failTask()`** | ❌ | **Draining 任务仍会失败！** |
| L1763-1765 | `gracefulCancel()` | ❌ | Draining 任务仍会被取消 |
| L1971-1973 | AIEngine 回调 | ❌ | Draining 任务仍会触发 |
| L2080-2082 | `AIVideoProcessor::completed` | ❌ | Draining AI视频仍会完成 |
| L929-932 | `VideoProcessor::finishCb` | ✅ | 正确检查 |

### 1.2 崩溃路径

```
用户快速删除 → terminateTask(Draining) → 任务标记为 Draining
  │
  ├─ 处理器仍在运行（不中断GPU）
  │
  └─ 处理器完成 → completed/finished 信号
       │
       └─ 回调检查生命周期 → ❌ 遗漏 Draining 检查
            │
            ├─ 调用 completeTask() / failTask()
            │    └─ 访问 messageForSession() → 消息已被删除 → 空Message
            │         └─ 遍历空 mediaFiles → fileHandled=false
            │              └─ 调用 failTask() → finalizeTask()
            │                   └─ 访问已释放资源 → 💥 崩溃
            │
            └─ 或者直接调用 finalizeTask() → 同样崩溃
```

### 1.3 为什么之前的修复无效

1. **v2-v4 方案**：添加了 `Draining` 状态，但只在 `terminateTask` 和 `finishCb` 中处理，**其他回调没有检查 Draining**
2. **v5 方案**：修复了 progressCb 泄漏，但**核心的生命周期检查遗漏问题未解决**

---

## 2. 解决方案

### 2.1 核心修复：统一生命周期检查

**原则**：所有可能访问任务资源的方法/回调，都必须检查 `Draining` 状态。

**需要修改的位置（共9处）**：

1. `cancelTask()` (L168-170)
2. `ImageProcessor::finished` (L559-561)
3. `AIEngine::processFileCompleted` (L651-653)
4. `AIEngine::modelLoadCompleted` (L693-695)
5. **`completeTask()` (L746-748)** ← 最关键
6. **`failTask()` (L1024-1026)** ← 最关键
7. `gracefulCancel()` (L1763-1765)
8. AIEngine 回调 (L1971-1973)
9. `AIVideoProcessor::completed` (L2080-2082)

### 2.2 统一检查模式

将所有生命周期检查统一为：

```cpp
TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
if (lifecycle == TaskLifecycle::Canceling ||
    lifecycle == TaskLifecycle::Draining ||    // ← 新增
    lifecycle == TaskLifecycle::Cleaning ||
    lifecycle == TaskLifecycle::Dead) {
    qInfo() << "[ProcessingController] 方法名 ignored for"
            << taskId << "lifecycle:" << static_cast<int>(lifecycle);
    return;
}
```

### 2.3 Draining 状态的正确处理

当检测到 `Draining` 状态时，应该：
1. **静默返回**（不执行任何操作）
2. **不调用** `completeTask`/`failTask`/`finalizeTask`
3. **依赖** `finishCb`/`completed` handler 中的 Draining 分支来清理资源

---

## 3. 实施步骤

### Step 1: 修复 `completeTask()` — 最关键
在 L746-748 添加 `Draining` 检查

### Step 2: 修复 `failTask()` — 最关键
在 L1024-1026 添加 `Draining` 检查

### Step 3: 修复 `cancelTask()`
在 L168-170 添加 `Draining` 检查

### Step 4: 修复 `gracefulCancel()`
在 L1763-1765 添加 `Draining` 检查

### Step 5: 修复 `ImageProcessor::finished` 回调
在 L559-561 添加 `Draining` 检查

### Step 6: 修复 `AIEngine::processFileCompleted` 回调
在 L651-653 添加 `Draining` 检查

### Step 7: 修复 `AIEngine::modelLoadCompleted` 回调
在 L693-695 添加 `Draining` 检查

### Step 8: 修复 `AIVideoProcessor::completed` 回调
在 L2080-2082 添加 `Draining` 检查

### Step 9: 修复 AIEngine 其他回调 (L1971-1973)
添加 `Draining` 检查

### Step 10: 构建验证 + 运行测试

---

## 4. 验证矩阵

| 场景 | 操作 | 预期结果 |
|------|------|----------|
| Shader高压快删 | 快速连续删除多个Shader处理中文件 | 不崩溃，UI即时响应 |
| AI推理高压快删 | 快速连续删除多个AI推理中文件 | 不崩溃，UI即时响应 |
| 混合模式快删 | 图片+视频混合消息快速删除 | 不崩溃 |
| Processing+Pending | 同时有处理中和等待中任务时快删 | 不崩溃 |
| 单文件狂点 | 单个文件X按钮狂点20次 | 只有第一次生效，不崩溃 |

---

## 5. 技术总结

### 5.1 为什么这个方案能彻底解决问题

1. **统一防御**：所有入口都有相同的生命周期检查
2. **Draining 尊重**：Draining 状态的任务不会被任何回调干扰
3. **资源安全**：Draining 任务的资源清理只在 `finishCb`/`completed` 的 Draining 分支中执行，避免竞态

### 5.2 架构改进

```
之前：
  回调 → 检查(Canceling/Cleaning/Dead) → ❌ 遗漏 Draining → 崩溃

之后：
  回调 → 检查(Canceling/Draining/Cleaning/Dead) → ✅ Draining 被拦截 → 安全返回
```
