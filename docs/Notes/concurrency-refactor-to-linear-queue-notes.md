# 并发架构重构为线性任务队列

## 重构日期
2026-03-27

## 重构目标
将复杂的并发任务处理架构简化为简单的线性任务队列，一次只处理一个任务。

## 删除的文件

### 并发核心组件
- `src/core/ConcurrencyManager.cpp` + `include/EnhanceVision/core/ConcurrencyManager.h`
- `src/core/PriorityTaskQueue.cpp` + `include/EnhanceVision/core/PriorityTaskQueue.h`
- `src/core/TaskRetryPolicy.cpp` + `include/EnhanceVision/core/TaskRetryPolicy.h`
- `src/core/TaskTimeoutWatchdog.cpp` + `include/EnhanceVision/core/TaskTimeoutWatchdog.h`
- `src/core/DeadlockDetector.cpp` + `include/EnhanceVision/core/DeadlockDetector.h`
- `src/core/PriorityAdjuster.cpp` + `include/EnhanceVision/core/PriorityAdjuster.h`
- `src/core/ConcurrencyMonitor.cpp` + `include/EnhanceVision/core/ConcurrencyMonitor.h`
- `tests/cpp/test_concurrency.cpp`

## 修改的文件

### ProcessingController
- 移除 `m_concurrencyManager` 成员和所有相关引用
- 移除 `m_maxConcurrentTasks`、`m_baseMaxConcurrentTasks`、`m_interactionPriorityMaxConcurrentTasks`、`m_importBurstMaxConcurrentTasks` 成员
- 移除 `setMaxConcurrentTasks()`、`setInteractionPriorityMode()`、`setImportBurstMode()`、`refreshEffectiveConcurrencyLimit()`、`setResourceQuota()` 方法
- 移除 `maxConcurrentTasksChanged` 信号
- 简化 `processNextTask()` 为线性队列逻辑（一次只处理一个任务）

### SettingsController
- 移除 `maxConcurrentTasks`、`maxConcurrentSessions`、`maxConcurrentFilesPerMessage` 属性及其 getter/setter
- 移除 `devicePerformanceHint()` 方法
- 保留自动重新处理设置（`autoReprocessShaderEnabled`、`autoReprocessAIEnabled`、`autoReprocessAllEnabled`）

### SettingsPage.qml
- 移除"性能"部分的并发设置 UI（同时处理会话数、每消息并发文件数、设备性能提示）
- 保留缓存管理功能
- 修复 `Theme.colors.info` 为 `Theme.colors.primary`（Colors.qml 中未定义 info 颜色）

### ResourceManager
- 简化 `canStartNewTask()` 为固定检查活动任务数 >= 1
- 简化 `recommendedConcurrency()` 始终返回 1

### CMakeLists.txt
- 移除所有已删除的并发相关源文件和头文件引用
- 移除 `test_concurrency` 测试目标

## 保留的功能

### 自动重新处理
- Shader 模式自动重新处理开关
- AI 推理模式自动重新处理开关
- 全部开启/关闭控制

### 资源管理
- 内存使用监控和压力检测
- GPU 显存监控
- 资源获取/释放跟踪

### 任务协调
- TaskCoordinator 保留用于任务生命周期管理
- 孤儿任务检测和清理
- 取消令牌和暂停令牌支持

## 新架构特点

1. **线性任务队列**：一次只处理一个消息任务
2. **简单调度**：按 FIFO 顺序处理待处理任务
3. **AI 引擎池检查**：AI 推理任务仍需检查引擎池可用性
4. **资源保护**：保留内存/GPU 显存检查防止资源耗尽
5. **线程安全**：使用 Qt 信号槽机制确保线程安全
6. **用户体验**：界面流畅，无并发设置复杂性

## 测试验证

- [x] 构建成功（零错误）
- [x] 程序启动正常
- [x] Shader 视频处理任务线性执行
- [x] 任务完成后自动启动下一个任务
- [x] 无运行时错误或警告
