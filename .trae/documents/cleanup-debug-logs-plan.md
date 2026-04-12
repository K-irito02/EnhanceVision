# 生产环境调试日志清理计划

## 一、现状分析

### 日志分布统计

| 类型 | 数量 | 分布文件 | 性质 |
|------|------|----------|------|
| `qInfo()` | ~30+ 处 | AIEngine, AIInferenceService, NCNNVulkanBackend, ProcessingController 等 | 流程跟踪日志 |
| `qWarning()` | ~30+ 处 | NCNNVulkanBackend, AIVideoProcessor 等 | 错误/警告日志 |
| `console.warn()` | 1 处 | ColoredIcon.qml | 错误日志 |

### 日志分类原则

| 类型 | 处理策略 | 原因 |
|------|----------|------|
| **qWarning()** | ✅ 保留 | 错误和异常信息，生产环境需要 |
| **qInfo() 流程跟踪** | ❌ 删除 | 初始化完成、任务状态变化等调试信息 |
| **qInfo() 关键操作** | ⚠️ 评估保留 | 部分关键操作可能需要保留 |
| **console.warn()** | ✅ 保留 | QML 错误提示 |

## 二、清理范围

### 需要清理的文件（按日志数量排序）

1. **src/core/AIEngine.cpp** - 93 处日志
2. **src/controllers/ProcessingController.cpp** - 129 处日志
3. **src/core/ai/AIInferenceService.cpp** - 28 处日志
4. **src/core/inference/NCNNVulkanBackend.cpp** - 21 处日志
5. **src/core/ai/AIInferenceWorker.cpp** - 22 处日志
6. **src/core/ai/AITaskQueue.cpp** - 13 处日志
7. **src/core/video/VideoSizeAdapter.cpp** - 13 处日志
8. **src/core/video/AIVideoProcessor.cpp** - 27 处日志
9. **其他文件** - 少量日志

### 具体清理内容

#### 1. 删除的 qInfo() 日志类型

- `[XXX] Initializing...` - 初始化开始
- `[XXX] Initialization complete` - 初始化完成
- `[XXX] Shutting down` - 关闭开始
- `[XXX] Shutdown complete` - 关闭完成
- `[XXX] Instance created/destroyed` - 实例创建/销毁
- `[XXX] Mode X: ...` - 模式状态变化
- `[XXX] New message added/staged` - 消息状态
- `[XXX] finished signal ignored` - 信号忽略
- `[XXX] GPU device: X` - GPU 设备信息
- `[XXX] Model loaded on GPU` - 模型加载信息

#### 2. 保留的日志类型

- 所有 `qWarning()` - 错误和异常
- `console.warn()` - QML 错误提示

## 三、执行步骤

### 步骤 1：清理 AIEngine.cpp
- 删除初始化、关闭、GPU 探测相关的 qInfo()
- 保留所有 qWarning()

### 步骤 2：清理 ProcessingController.cpp
- 删除任务状态变化、消息处理的 qInfo()
- 保留所有 qWarning()

### 步骤 3：清理 AIInferenceService.cpp
- 删除实例创建、初始化、关闭的 qInfo()
- 保留所有 qWarning()

### 步骤 4：清理 NCNNVulkanBackend.cpp
- 删除 GPU 初始化、资源管理的 qInfo()
- 保留所有 qWarning()

### 步骤 5：清理 AIInferenceWorker.cpp
- 删除工作线程相关的 qInfo()
- 保留所有 qWarning()

### 步骤 6：清理 AITaskQueue.cpp
- 删除任务队列操作的 qInfo()
- 保留所有 qWarning()

### 步骤 7：清理 VideoSizeAdapter.cpp
- 删除视频尺寸适配的 qInfo()
- 保留所有 qWarning()

### 步骤 8：清理 AIVideoProcessor.cpp
- 删除视频处理流程的 qInfo()
- 保留所有 qWarning()

### 步骤 9：清理其他文件
- 按相同原则清理剩余文件

### 步骤 10：构建验证
- 使用 qt-build-and-fix 技能构建项目
- 确保编译通过，无功能破坏

## 四、安全措施

1. **不删除 qWarning()** - 所有错误日志保留
2. **不修改日志相关的条件逻辑** - 仅删除日志语句本身
3. **构建验证** - 每次修改后构建验证
4. **功能测试** - 确保核心功能正常运行

## 五、预期结果

- 删除约 200+ 条调试日志
- 保留约 50+ 条错误/警告日志
- 代码更加简洁，适合生产环境
- 不影响任何现有功能
