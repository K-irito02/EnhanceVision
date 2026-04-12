# 应用程序关闭后进程残留问题修复计划

## 问题描述

用户报告：在 Windows 上点击应用程序右上角 `X` 关闭按钮后，应用程序进程仍在后台运行，即使手动结束后仍会重新出现。

## 问题根因分析

### 1. 主要原因：子窗口阻止应用退出

在 `EmbeddedMediaViewer.qml` 中存在多个 Window 组件：

- `dragPreviewWindow` (第 359-411 行)
- `detachedWindow` (第 477-556 行)

**关键问题代码** (`detachedWindow` 的 `onClosing` 处理，第 509-512 行)：
```qml
onClosing: function(close) {
    close.accepted = false  // 阻止窗口关闭！
    root.close()            // 只是隐藏窗口，不会销毁
}
```

`close.accepted = false` 会阻止窗口真正关闭，导致：
1. 窗口只是被隐藏，而不是被销毁
2. `setQuitOnLastWindowClosed(true)` 无法正常工作
3. 主窗口关闭后，子窗口仍然存在，应用进程不会退出

### 2. 次要问题：关闭流程中的资源清理

在 `LifecycleSupervisor::executeShutdownPipeline()` 中：
- 调用 `QCoreApplication::quit()` 后，如果有未完成的异步操作，进程可能不会立即退出
- `forceTerminate()` 需要等待 500ms 才会执行

### 3. 潜在问题：线程池清理

`ProcessingController` 的析构函数中调用 `m_threadPool->waitForDone()`，但如果线程池中有任务正在取消中（Draining 状态），可能会导致等待超时。

## 修复方案

### 修复 1：修复 `EmbeddedMediaViewer.qml` 中的窗口关闭逻辑

**文件**: `qml/components/EmbeddedMediaViewer.qml`

修改 `detachedWindow` 的 `onClosing` 处理：
```qml
// 修改前
onClosing: function(close) {
    close.accepted = false
    root.close()
}

// 修改后
onClosing: function(close) {
    close.accepted = true  // 允许窗口关闭
    root.close()
}
```

同时确保 `dragPreviewWindow` 在应用关闭时被正确销毁。

### 修复 2：改进 `LifecycleSupervisor` 的关闭流程

**文件**: `src/core/LifecycleSupervisor.cpp`

改进 `executeShutdownPipeline()` 方法：
1. 增加对所有子窗口的显式关闭
2. 减少强制终止的超时时间
3. 确保所有线程池任务被取消

### 修复 3：改进 `ProcessingController` 的任务取消

**文件**: `src/controllers/ProcessingController.cpp`

改进 `forceCancelAllTasks()` 方法：
1. 确保所有正在处理的任务被立即终止
2. 不依赖异步清理（`QTimer::singleShot`）
3. 同步清理所有资源

### 修复 4：在 `Application` 类中添加关闭清理

**文件**: `src/app/Application.cpp`

在 `Application` 析构函数或关闭流程中：
1. 显式关闭所有 QML 窗口
2. 确保所有子窗口被销毁

## 实施步骤

1. **修复 `EmbeddedMediaViewer.qml`**
   - 修改 `detachedWindow.onClosing` 处理逻辑
   - 添加应用关闭时的窗口清理

2. **改进 `LifecycleSupervisor.cpp`**
   - 优化关闭流程的超时处理
   - 添加子窗口清理逻辑

3. **改进 `ProcessingController.cpp`**
   - 同步化 `forceCancelAllTasks()` 的清理过程

4. **构建验证**
   - 使用 `qt-build-and-fix` 技能构建项目
   - 测试关闭功能

5. **测试验证**
   - 启动应用程序
   - 打开媒体查看器（创建子窗口）
   - 关闭主窗口
   - 验证进程是否正确退出

## 风险评估

- **低风险**：修改 `onClosing` 处理逻辑，只是允许窗口正常关闭
- **中风险**：修改关闭流程可能影响正常关闭时的数据保存

## 预期结果

修复后，点击关闭按钮时：
1. 所有窗口被正确关闭
2. 应用进程立即退出
3. 不会出现进程残留
