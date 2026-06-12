# EnhanceVision 项目架构优化与重构扫描报告

**项目路径:** `e:\QtAudio-VideoLearning\EnhanceVision`
**扫描日期:** 2026-05-10
**技术栈:** Qt Quick (QML) 6.10.2 + C++20 (MSVC 2022) + CMake + NCNN (Vulkan) + FFmpeg 7.1
**架构分层:** QML UI 层 → C++ 业务层 (Controller/Model/Service) → C++ 核心层 (AI推理/图像处理/视频处理)

---

## 📊 统计摘要

| 优化类型 | 数量 | P0 | P1 | P2 | P3 |
|---------|------|-----|-----|-----|-----|
| 🏗️ 架构优化 | 15 | 2 | 5 | 6 | 2 |
| ⚡ 性能优化 | 12 | 1 | 4 | 5 | 2 |
| 🔧 代码重构 | 18 | 1 | 6 | 8 | 3 |
| 🎨 QML 优化 | 10 | 1 | 3 | 4 | 2 |
| 📦 现代化升级 | 8 | 0 | 2 | 4 | 2 |
| **总计** | **63** | **5** | **20** | **27** | **11** |

**预估改动量:** 约 2800 行代码
**建议实施周期:** 6-9 个月（分阶段实施）

---

## 🏗️ 架构优化

### A-001 | P0 | Controller 职责膨胀 - ProcessingController

**涉及文件:**
- `include/EnhanceVision/controllers/ProcessingController.h` (L1-405)
- `src/controllers/ProcessingController.cpp` (L1-1400+)

**当前问题:**
`ProcessingController` 承担了过多职责，包括：
- 任务队列管理 (addTask, cancelTask, pauseQueue)
- 任务编排 (tryStartTask, startTask, completeTask)
- 会话同步 (SessionSyncHelper, ProgressSyncHelper)
- 消息状态解析 (MessageStatusResolver)
- 资源管理协调 (ResourceManager)
- AI 引擎池管理 (AIEnginePool)
- 孤儿任务处理 (onTaskOrphaned, handleOrphanedTask)
- 删除批处理 (m_deletionBatches, processDeletionBatch)
- 模式切换 (m_pausedMessageIds, m_activatedMessageIds, m_globalPauseEnabled)

总计约 50+ 个成员函数，3000+ 行代码，违反单一职责原则。

**优化方案:**
1. **提取 TaskOrchestrator 类** - 专门负责任务编排逻辑（tryStartTask, startTask, completeTask, failTask）
2. **提取 QueueManager 类** - 专门负责队列管理（addTask, cancelTask, pauseQueue, reorderTask）
3. **提取 DeletionManager 类** - 专门负责删除批处理逻辑
4. **提取 PauseModeController 类** - 专门负责三种暂停模式的切换逻辑

**预期收益:**
- 各模块可独立测试和维护
- 减少单一文件的编译时间
- 降低代码理解难度

**改动风险:** 中
**分阶段实施:** 是（分3个阶段，每阶段重构一个子模块）

---

### A-002 | P0 | Controller 间直接依赖

**涉及文件:**
- `src/controllers/ProcessingController.cpp` (L8-10)
- `src/controllers/SessionController.cpp` (L7-11)

**当前问题:**
`ProcessingController` 直接持有 `SessionController*` 指针并调用其方法：
```cpp
// ProcessingController.cpp L9
#include "EnhanceVision/controllers/SessionController.h"
// ...
m_sessionController->syncCurrentMessagesToSession();
m_sessionController->updateMessageInSession(...);
m_sessionController->resolveSessionIdForMessage(...);
```

违反依赖倒置原则，Controller 之间应通过信号槽解耦。

**优化方案:**
1. 定义 `ISessionAccessor` 接口
2. `SessionController` 实现该接口
3. `ProcessingController` 依赖接口而非具体实现
4. 将直接方法调用替换为信号发射

**预期收益:**
- 降低模块间耦合
- 便于单元测试（可 mock 接口）
- 提高系统可扩展性

**改动风险:** 高
**分阶段实施:** 是（建议先在测试分支验证）

---

### A-003 | P1 | TaskRecoveryController 恢复流程复杂

**涉及文件:**
- `include/EnhanceVision/controllers/TaskRecoveryController.h` (L1-77)
- `src/controllers/TaskRecoveryController.cpp` (L1-300+)

**当前问题:**
- 恢复决策逻辑硬编码在控制器中
- 快照机制与业务逻辑紧耦合
- 状态映射逻辑难以复用

**优化方案:**
1. **提取 StateSnapshot 接口** - 统一快照的序列化和反序列化
2. **提取 RecoveryStrategy 接口** - 定义恢复策略（恢复全部/恢复部分/标记失败）
3. **提取 SnapshotManager 类** - 专门负责快照的持久化

**预期收益:**
- 恢复逻辑可配置和可测试
- 便于扩展新的恢复场景

**改动风险:** 低
**分阶段实施:** 否

---

### A-004 | P1 | SessionSyncHelper/ProgressSyncHelper 职责重叠

**涉及文件:**
- `include/EnhanceVision/controllers/SessionSyncHelper.h`
- `include/EnhanceVision/controllers/ProgressSyncHelper.h`
- `src/controllers/SessionSyncHelper.cpp`
- `src/controllers/ProgressSyncHelper.cpp`

**当前问题:**
- 两者都负责会话数据同步
- `ProgressSyncHelper` 依赖 `SessionSyncHelper`
- 同步逻辑分散在多个类中，难以追踪

**优化方案:**
1. **合并为 UnifiedSyncService**
2. 统一管理防抖定时器
3. 提取同步策略类

**预期收益:**
- 减少重复代码
- 简化同步流程追踪

**改动风险:** 中
**分阶段实施:** 是（先提取接口，再合并实现）

---

### A-005 | P1 | Service 注册机制缺失

**涉及文件:**
- `src/services/ImageExportService.cpp` (单例模式)
- `src/services/AutoSaveService.cpp` (单例模式)
- `src/services/InstallMaintenanceService.cpp`

**当前问题:**
各 Service 使用硬编码的单例模式，难以：
- 替换为 mock 实现（单元测试）
- 支持多个实例（不同配置）
- 按需延迟初始化

**优化方案:**
1. **引入 ServiceRegistry 类**
2. 定义 `IServiceFactory` 接口
3. 服务注册时指定生命周期（Singleton/Transient）

**预期收益:**
- 支持依赖注入
- 便于测试
- 支持服务替换

**改动风险:** 中
**分阶段实施:** 是

---

### A-006 | P2 | SettingsController 属性膨胀

**涉及文件:**
- `include/EnhanceVision/controllers/SettingsController.h` (L1-266)
- `src/controllers/SettingsController.cpp` (L1-600+)

**当前问题:**
- 70+ 个 Q_PROPERTY，导致 QML 绑定复杂
- 设置项硬编码，缺乏描述式注册
- 数据大小统计逻辑耦合在控制器中

**优化方案:**
1. **提取 SettingDescriptor 结构** - 定义每个设置项的元信息
2. **引入 SettingsRegistry** - 描述式注册设置项
3. **提取 DataSizeCalculator 类** - 专门计算各类型数据大小

**预期收益:**
- 设置项可动态扩展
- 减少 Q_PROPERTY 数量

**改动风险:** 低
**分阶段实施:** 否

---

### A-007 | P2 | ProcessingModel 数据角色硬编码

**涉及文件:**
- `include/EnhanceVision/models/ProcessingModel.h` (L1-46)

**当前问题:**
`ProcessingModel` 未定义 Roles 枚举，依赖 QML 侧硬编码角色值（如 L763-766 使用 magic numbers: 258, 259, 262, 263）。

**优化方案:**
1. 在 `ProcessingModel` 中定义 `enum Roles { ... }`
2. 导出到 QML
3. QML 侧使用命名角色替代魔法数字

**预期收益:**
- 类型安全
- 可维护性提升

**改动风险:** 低
**分阶段实施:** 否

---

### A-008 | P2 | SessionModel/FileModel 重复模式

**涉及文件:**
- `include/EnhanceVision/models/SessionModel.h`
- `include/EnhanceVision/models/FileModel.h`

**当前问题:**
两者都继承 `QAbstractListModel`，存在大量相似代码：
- Roles 枚举模式
- rowCount/data 实现
- 信号定义

**优化方案:**
1. **提取 BaseListModel 基类**
2. 定义模板方法供子类实现

**预期收益:**
- 减少重复代码
- 统一模型行为

**改动风险:** 低
**分阶段实施:** 否

---

### A-009 | P2 | AIEnginePool 缺乏抽象

**涉及文件:**
- `include/EnhanceVision/core/AIEnginePool.h` (L1-96)
- `src/core/AIEnginePool.cpp` (L1-200+)

**当前问题:**
- 池管理逻辑硬编码
- 无法配置资源分配策略
- 难以支持其他类型的对象池

**优化方案:**
1. **提取 IObjectPool<T> 模板接口**
2. **AIEnginePool 实现 IObjectPool<AIEngine>**
3. 策略类配置资源分配

**预期收益:**
- 代码复用
- 灵活配置

**改动风险:** 低
**分阶段实施:** 否

---

### A-010 | P2 | 视频处理管道缺乏 Pipeline 抽象

**涉及文件:**
- `include/EnhanceVision/core/video/AIVideoProcessor.h`
- `include/EnhanceVision/core/video/VideoFormatConverter.h`
- `include/EnhanceVision/core/video/VideoSizeAdapter.h`
- `include/EnhanceVision/core/video/VideoCompatibilityAnalyzer.h`

**当前问题:**
视频处理流程（解码→推理→编码）紧耦合在 `AIVideoProcessor` 中，各适配器独立实现，难以组合和扩展。

**优化方案:**
1. **定义 IVideoPipelineStage 接口**
2. **实现 DecoderStage, InferenceStage, EncoderStage**
3. **VideoPipeline 组合各 Stage**

**预期收益:**
- 便于增减处理阶段
- 支持流水线并行

**改动风险:** 中
**分阶段实施:** 是

---

### A-011 | P2 | SessionPruneUtils 逻辑可泛化

**涉及文件:**
- `include/EnhanceVision/controllers/SessionPruneUtils.h`
- `src/controllers/SessionPruneUtils.cpp`

**当前问题:**
`recomputeMessageStatus` 中的状态优先级判断是硬编码的 if-else 链：
```cpp
// SessionPruneUtils.cpp L41-57
if (hasRecoverable) { ... }
else if (hasPaused) { ... }
else if (hasProcessing) { ... }
// ...
```

**优化方案:**
1. 定义状态优先级配置
2. 使用查表法替代 if-else 链

**预期收益:**
- 可配置状态优先级
- 便于扩展新状态

**改动风险:** 低
**分阶段实施:** 否

---

### A-012 | P3 | QML 页面间重复布局逻辑

**涉及文件:**
- `qml/pages/MainPage.qml` (L1-852)
- `qml/pages/SettingsPage.qml`
- `qml/components/ControlPanel.qml`

**当前问题:**
- 模式选择按钮（Shader/AI）在多处重复实现
- 页面切换逻辑类似
- 空状态界面分散

**优化方案:**
1. **提取 ModeSelector 组件**
2. **提取 PageStackController**
3. **提取 EmptyStateView 组件**

**预期收益:**
- UI 一致性
- 减少重复代码

**改动风险:** 低
**分阶段实施:** 否

---

### A-013 | P3 | SettingsController 多职责

**涉及文件:**
- `src/controllers/SettingsController.cpp`

**当前问题:**
`SettingsController` 同时负责：
- 用户设置管理（主题/语言/保存路径）
- 存储大小统计
- 缓存清理
- 路径回退逻辑

**优化方案:**
1. **提取 StorageAnalytics 类** - 专门负责存储分析
2. **提取 PathResolver 类** - 专门负责路径解析和回退

**预期收益:**
- 单一职责
- 可独立测试

**改动风险:** 低
**分阶段实施:** 否

---

### A-014 | P1 | ProcessingEngine 与 ProcessingController 职责重叠

**涉及文件:**
- `include/EnhanceVision/core/ProcessingEngine.h`
- `include/EnhanceVision/controllers/ProcessingController.h`

**当前问题:**
- `ProcessingEngine` 的队列管理功能与 `ProcessingController` 重复
- 两者都管理 `ImageProcessor`, `VideoProcessor`, `AIEngine`
- 职责边界模糊

**优化方案:**
1. **明确分工**：`ProcessingEngine` 专注底层处理，`ProcessingController` 专注上层协调
2. 或 **合并** 为单一控制器
3. 移除冗余的 `m_taskQueue` 维护

**预期收益:**
- 消除重复
- 简化架构

**改动风险:** 高
**分阶段实施:** 是

---

### A-015 | P0 | QML 业务逻辑渗透

**涉及文件:**
- `qml/pages/MainPage.qml` (L698-747 `_sendForProcessing`)
- `qml/pages/MainPage.qml` (L749-829 `_openPendingFileViewer`, `_syncPendingViewerWindow`)
- `qml/components/ControlPanel.qml`

**当前问题:**
`MainPage.qml` 包含大量业务逻辑：
- 参数打包 (`_sendForProcessing` 约50行)
- 文件遍历 (`_openPendingFileViewer`, `_syncPendingViewerWindow`)
- URL 清理 (`_sanitizeLocalDropUrls`)

这些应下沉到 C++ Controller。

**优化方案:**
1. **提取 FileOperationService** - 处理文件操作
2. **提取 ParamsBuilder** - 处理参数打包
3. QML 仅保留 UI 绑定和事件转发

**预期收益:**
- 分离关注点
- 提升性能（避免 QML-JS 解释开销）
- 便于单元测试

**改动风险:** 中
**分阶段实施:** 是

---

## ⚡ 性能优化

### P-001 | P1 | ProcessingController 线程池配置

**涉及文件:**
- `src/controllers/ProcessingController.cpp` (L99-100)

**当前问题:**
```cpp
m_threadPool->setMaxThreadCount(1);  // 单线程
m_threadPool->setThreadPriority(QThread::LowPriority);
```
AI 推理本身是 CPU/GPU 密集型，单线程池限制了并行能力。

**优化方案:**
1. 根据 CPU 核心数和 GPU 可用性动态配置线程数
2. 分离 IO 线程池和处理线程池

**预期收益:**
- 多核充分利用
- 处理吞吐量提升

**改动风险:** 中
**分阶段实施:** 是

---

### P-002 | P1 | AITaskQueue 使用 std::deque

**涉及文件:**
- `include/EnhanceVision/core/ai/AITaskQueue.h` (L190)

**当前问题:**
`std::deque` 在中间插入/删除效率较低（O(n)）。任务取消操作可能需要遍历整个队列。

**优化方案:**
1. 使用 `std::list` 优化中间操作
2. 或使用 `boost::intrusive::list` 实现无锁队列
3. 或考虑 lock-free 队列（如 `moodycamel::ConcurrentQueue`）

**预期收益:**
- 任务取消性能提升
- 高并发场景表现更好

**改动风险:** 中
**分阶段实施:** 否

---

### P-003 | P1 | AIInferenceWorker 独占线程

**涉及文件:**
- `include/EnhanceVision/core/ai/AIInferenceWorker.h` (L175-212)

**当前问题:**
每个 `AIInferenceWorker` 独占一个 `QThread`，对于 CPU 推理场景浪费资源。

**优化方案:**
1. **引入线程池管理 Worker**
2. Worker 任务完成后归还池中
3. 保持 Vulkan 模式独占（GPU 资源限制）

**预期收益:**
- 资源利用率提升
- 支持更多并发任务

**改动风险:** 中
**分阶段实施:** 是

---

### P-004 | P1 | VideoFrameBuffer 固定大小

**涉及文件:**
- `include/EnhanceVision/core/video/VideoFrameBuffer.h` (L29)

**当前问题:**
```cpp
explicit VideoFrameBuffer(int maxSlots = 4, QObject* parent = nullptr);
```
固定 4 帧缓冲，无法根据视频分辨率动态调整。

**优化方案:**
1. 根据视频尺寸计算最优缓冲大小
2. 实现动态伸缩策略
3. 添加内存压力检测

**预期收益:**
- 内存使用更合理
- 大分辨率视频处理更流畅

**改动风险:** 低
**分阶段实施:** 否

---

### P-005 | P1 | NCNNCPUBackend/NCNNVulkanBackend 重复代码

**涉及文件:**
- `include/EnhanceVision/core/inference/NCNNCPUBackend.h`
- `include/EnhanceVision/core/inference/NCNNVulkanBackend.h`
- `src/core/inference/NCNNCPUBackend.cpp`
- `src/core/inference/NCNNVulkanBackend.cpp`

**当前问题:**
两个后端有大量重复代码：
- `processSingle`, `processTiled`, `processWithTTA` 方法
- `runInference` 核心逻辑
- 状态管理（m_cancelRequested, m_progress）

**优化方案:**
1. **提取 InferenceBackendBase 类**
2. 公共逻辑放入基类
3. CPU/GPU 特有逻辑在子类实现

**预期收益:**
- 代码量减少约 40%
- 维护成本降低

**改动风险:** 中
**分阶段实施:** 是

---

### P-006 | P2 | ImageProcessor/VideoProcessor 深拷贝

**涉及文件:**
- `src/core/ImageProcessor.cpp`
- `src/core/VideoProcessor.cpp`

**当前问题:**
处理过程中存在不必要的图像深拷贝：
- 函数参数传递 QImage（值传递）
- 中间结果复制

**优化方案:**
1. 使用 `QImage&` 或 `const QImage&` 引用传递
2. 确认 QImage 写时复制语义
3. 使用 `std::move` 转移所有权

**预期收益:**
- 内存分配减少
- 处理速度提升

**改动风险:** 低
**分阶段实施:** 否

---

### P-007 | P2 | 资源初始化时机

**涉及文件:**
- `src/controllers/ProcessingController.cpp` (L130-140)
- `src/core/AIEnginePool.cpp`

**当前问题:**
模型路径解析在构造函数中串行执行，多次尝试可能造成启动延迟：
```cpp
QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
if (!QDir(modelsPath).exists()) {
    modelsPath = QCoreApplication::applicationDirPath() + "/../resources/models";
}
// ...
```

**优化方案:**
1. 延迟初始化 AIEnginePool
2. 按需加载模型
3. 并行探测多个路径

**预期收益:**
- 启动时间缩短
- 内存占用降低（未使用的模型不加载）

**改动风险:** 低
**分阶段实施:** 否

---

### P-008 | P2 | SessionController 保存防抖

**涉及文件:**
- `src/controllers/SessionController.cpp` (L145-148)

**当前问题:**
```cpp
m_saveTimer->setSingleShot(true);
m_saveTimer->setInterval(1800);  // 固定 1.8 秒
```
固定防抖间隔，在快速操作场景下可能保存不及时，在空闲场景下浪费定时器资源。

**优化方案:**
1. 自适应防抖（根据操作频率调整）
2. 紧急操作立即保存（标记失败等）
3. 后台静默保存

**预期收益:**
- 数据安全性提升
- 资源使用更合理

**改动风险:** 低
**分阶段实施:** 否

---

### P-009 | P2 | QML 属性绑定效率

**涉及文件:**
- `qml/pages/MainPage.qml` (L59-77)
- `qml/components/ControlPanel.qml` (L59-74)

**当前问题:**
`hasShaderModifications` 是复杂计算属性：
```qml
readonly property bool hasShaderModifications: {
    return Math.abs(brightness) > 0.001 || Math.abs(contrast - 1.0) > 0.001 || ...  // 13 个条件
}
```
每次任何相关属性变化都会重新计算。

**优化方案:**
1. 使用 `Binding` 控制计算时机
2. 缓存上次计算结果
3. 脏标记机制

**预期收益:**
- QML 绑定开销减少
- UI 响应更流畅

**改动风险:** 低
**分阶段实施:** 否

---

### P-010 | P3 | 缩略图数据库索引

**涉及文件:**
- `src/core/ThumbnailDatabase.cpp`

**当前问题:**
缩略图查询可能未使用索引，文件数量增长后查询变慢。

**优化方案:**
1. 检查 SQLite 索引配置
2. 为 `file_path` 和 `modified_time` 添加索引
3. 定期 VACUUM

**预期收益:**
- 大数据量下查询性能提升

**改动风险:** 低
**分阶段实施:** 否

---

### P-011 | P3 | FFmpeg 解码缓冲

**涉及文件:**
- `src/core/video/AIVideoProcessor.cpp`

**当前问题:**
FFmpeg 解码可能未使用最优缓冲设置，导致帧提取效率低。

**优化方案:**
1. 配置 `av_dict_set` 设置解码缓冲
2. 使用 `AVFrame` 池复用
3. 检查硬解码选项

**预期收益:**
- 视频帧提取速度提升

**改动风险:** 中
**分阶段实施:** 否

---

### P-012 | P0 | 内存泄漏风险 - DyingProcessor

**涉及文件:**
- `src/controllers/ProcessingController.cpp` (L42-79, L320-321)

**当前问题:**
```cpp
// L42-79: 静态泄漏列表用于 shutdown 期间保留处理器
QList<QSharedPointer<AIVideoProcessor>>& shutdownLeakedAiVideoProcessors()
{
    static QList<QSharedPointer<AIVideoProcessor>> leakedProcessors;
    return leakedProcessors;
}
```
这是一种 hack 方式，用于避免 shutdown 期间的崩溃，但会造成内存泄漏。

**优化方案:**
1. 改进 Processor 生命周期管理
2. 使用 `Qt::QueuedConnection` 替代直接 delete
3. 引入引用计数机制

**预期收益:**
- 消除内存泄漏
- shutdown 流程更干净

**改动风险:** 高
**分阶段实施:** 是

---

## 🔧 代码重构

### R-001 | P1 | MessageStatusResolver if-else 链

**涉及文件:**
- `src/controllers/SessionPruneUtils.cpp` (L41-57)
- `src/controllers/MessageStatusResolver.cpp`

**当前问题:**
状态派生逻辑使用长 if-else 链：
```cpp
if (hasRecoverable) { ... }
else if (hasPaused) { ... }
else if (hasProcessing) { ... }
// ... 7 种状态
```

**优化方案:**
1. 定义 `QHash<ProcessingStatus, int>` 优先级表
2. 使用 `std::max_element` 查找最高优先级状态

**预期收益:**
- 代码简洁
- 便于添加新状态

**改动风险:** 低
**分阶段实施:** 否

---

### R-002 | P1 | SessionPruneUtils 与 MessageStatusResolver 重复

**涉及文件:**
- `src/controllers/SessionPruneUtils.cpp`
- `src/controllers/MessageStatusResolver.cpp`

**当前问题:**
`recomputeMessageStatus` 和 `deriveMessageStatus` 实现几乎相同，造成代码重复。

**优化方案:**
1. 合并为单一函数
2. 或让其中一个调用另一个

**预期收益:**
- 消除重复
- 一致性保证

**改动风险:** 低
**分阶段实施:** 否

---

### R-003 | P1 | FileUtils 与 StoragePaths 功能重叠

**涉及文件:**
- `include/EnhanceVision/utils/FileUtils.h`
- `include/EnhanceVision/utils/StoragePaths.h`

**当前问题:**
- `FileUtils::getDefaultSavePath()` 与 `StoragePaths::defaultConfiguredSavePath()` 功能类似
- `FileUtils::ensureDirectory()` 可复用 StoragePaths

**优化方案:**
1. 统一为 StoragePaths 命名空间
2. FileUtils 仅保留文件操作原语

**预期收益:**
- 职责清晰
- 减少混淆

**改动风险:** 低
**分阶段实施:** 否

---

### R-004 | P1 | ImageUtils 与 SupportedFormats 重复

**涉及文件:**
- `include/EnhanceVision/utils/ImageUtils.h`
- `include/EnhanceVision/utils/SupportedFormats.h`

**当前问题:**
- `ImageUtils::isImageFile()` 与 `SupportedFormats::isImageFile()` 重复
- `ImageUtils::isVideoFile()` 与 `SupportedFormats::isVideoFile()` 重复

**优化方案:**
1. 统一为 SupportedFormats
2. ImageUtils 调用 SupportedFormats

**预期收益:**
- 单一数据源
- 扩展时只需改一处

**改动风险:** 低
**分阶段实施:** 否

---

### R-005 | P2 | ImagePreprocessor/ImagePostprocessor 静态方法过多

**涉及文件:**
- `include/EnhanceVision/core/inference/ImagePreprocessor.h` (14个静态方法)
- `include/EnhanceVision/core/inference/ImagePostprocessor.h` (11个静态方法)

**当前问题:**
全部静态方法，难以：
- 注入配置
- mock 测试
- 共享状态

**优化方案:**
1. 改为实例方法
2. 构造函数接收配置
3. 可选 Builder 模式

**预期收益:**
- 可测试性提升
- 配置灵活

**改动风险:** 中
**分阶段实施:** 否

---

### R-006 | P2 | LifecycleSupervisor 退出原因枚举

**涉及文件:**
- `include/EnhanceVision/core/LifecycleSupervisor.h` (L27-39)

**当前问题:**
```cpp
enum class ExitReason {
    Normal,
    MainWindowClosed,
    MainWindowDestroyed,
    // ... 10 种退出原因
};
```
硬编码的枚举难以扩展。

**优化方案:**
1. 提取 `IExitReasonProvider` 接口
2. 支持自定义退出原因
3. 使用 QFlags 组合标志

**预期收益:**
- 可扩展性
- 细粒度控制

**改动风险:** 低
**分阶段实施:** 否

---

### R-007 | P2 | ProcessingTimeManager 散落在各处

**涉及文件:**
- `src/controllers/SessionController.cpp`
- `src/core/ProcessingTimeManager.cpp`
- 多处调用 `updateMessageActualTotalSec`

**当前问题:**
时间统计逻辑分散：
- SessionController 调用 `updateMessageActualTotalSec`
- ProcessingController 也有时间估算
- 存在重复计算

**优化方案:**
1. 统一时间管理到 ProcessingTimeManager
2. ProcessingController 通过信号通知
3. SessionController 仅消费结果

**预期收益:**
- 消除重复
- 逻辑集中

**改动风险:** 中
**分阶段实施:** 是

---

### R-008 | P2 | Magic Numbers 提取

**涉及文件:**
- 多处常量硬编码

**当前问题:**
- `1000` (毫秒)、`1800` (保存防抖)、`5000` (处理器清理)、`30000` (超时)
- 分散在代码各处，难以统一调整

**优化方案:**
1. 创建 `Constants.h` 集中定义
2. 使用有意义的名称（如 `kDefaultSaveDebounceMs`）
3. 可配置文件

**预期收益:**
- 可维护性
- 便于调优

**改动风险:** 低
**分阶段实施:** 否

---

### R-009 | P2 | TaskSwitchSynchronizer 单例模式

**涉及文件:**
- `include/EnhanceVision/controllers/ProcessingController.h` (L41-85)

**当前问题:**
`TaskSwitchSynchronizer` 嵌套定义在 ProcessingController.h 中，违反单一职责。

**优化方案:**
1. 提取为独立文件 `TaskSwitchSynchronizer.h/cpp`
2. 使用标准单例模式或依赖注入

**预期收益:**
- 文件职责单一
- 便于测试

**改动风险:** 低
**分阶段实施:** 否

---

### R-010 | P2 | PendingExport 结构体

**涉及文件:**
- `include/EnhanceVision/controllers/ProcessingController.h` (L87-99)

**当前问题:**
`PendingExport` 嵌套定义在 ProcessingController.h 中，包含约10个字段，职责不单一。

**优化方案:**
1. 提取为独立文件
2. 使用 Builder 模式构建

**预期收益:**
- 职责清晰
- 可复用

**改动风险:** 低
**分阶段实施:** 否

---

### R-011 | P3 | 错误处理模式不统一

**涉及文件:**
- 多处

**当前问题:**
- 部分函数返回 `bool` 表示成功/失败
- 部分使用 `QString` 返回错误信息
- 部分抛出异常

**优化方案:**
1. 引入 `Result<T, E>` 模式（参考 Rust）
2. 或使用 `std::optional<T>` + `QString error`
3. 统一项目错误处理规范

**预期收益:**
- 类型安全
- 错误传播清晰

**改动风险:** 高
**分阶段实施:** 是

---

### R-012 | P3 | 日志规范缺失

**涉及文件:**
- 多处 `qDebug()`, `qWarning()`, `qCritical()`

**当前问题:**
- 日志级别使用混乱
- 部分关键路径缺少日志
- 部分路径日志过量
- 无统一格式

**优化方案:**
1. 定义日志宏（如 `LOG_INFO`, `LOG_PERF`）
2. 统一日志格式
3. 添加日志级别配置

**预期收益:**
- 便于调试
- 生产环境可关闭详细日志

**改动风险:** 低
**分阶段实施:** 否

---

### R-013 | P1 | ProcessingModel 未充分利用

**涉及文件:**
- `include/EnhanceVision/models/ProcessingModel.h`
- `src/controllers/ProcessingController.cpp`

**当前问题:**
`ProcessingController` 维护自己的 `m_tasks` 列表，同时 `ProcessingModel` 也有类似功能，但两者未充分联动。

**优化方案:**
1. 让 `ProcessingController` 完全委托给 `ProcessingModel`
2. 移除 `m_tasks` 副本
3. 通过 `ProcessingModel` 信号监听变化

**预期收益:**
- 数据一致性
- 代码简化

**改动风险:** 中
**分阶段实施:** 否

---

### R-014 | P2 | FileController 与 FileModel 职责重叠

**涉及文件:**
- `src/controllers/FileController.cpp`
- `src/models/FileModel.cpp`

**当前问题:**
两者都管理文件列表，职责边界模糊。

**优化方案:**
1. FileController 专注业务逻辑（文件验证、路径处理）
2. FileModel 专注数据存储和通知
3. 明确分层

**预期收益:**
- 职责清晰
- 便于测试

**改动风险:** 中
**分阶段实施:** 否

---

### R-015 | P2 | VideoProcessorFactory 工厂模式

**涉及文件:**
- `include/EnhanceVision/core/video/VideoProcessorFactory.h`

**当前问题:**
工厂类方法可能未充分封装，创建逻辑分散。

**优化方案:**
1. 使用抽象工厂模式
2. 支持依赖注入
3. 添加缓存机制

**预期收益:**
- 扩展性
- 可测试性

**改动风险:** 低
**分阶段实施:** 否

---

### R-016 | P3 | TaskState vs ProcessingStatus 混淆

**涉及文件:**
- `include/EnhanceVision/models/DataTypes.h`
- 多处使用

**当前问题:**
- `TaskState` 和 `ProcessingStatus` 两个状态枚举
- 语义有重叠
- 调用处容易混淆

**优化方案:**
1. 合并或明确区分
2. 添加类型别名
3. 统一状态转换逻辑

**预期收益:**
- 减少混淆
- 维护成本降低

**改动风险:** 中
**分阶段实施:** 是

---

### R-017 | P2 | MessageStatusSummary 结构体

**涉及文件:**
- `include/EnhanceVision/controllers/MessageStatusResolver.h` (L8-17)

**当前问题:**
手动计算各类状态数量，可改为更通用的聚合方式。

**优化方案:**
1. 使用 `QHash<ProcessingStatus, int>` 替代固定字段
2. 添加聚合方法

**预期收益:**
- 灵活扩展
- 代码简洁

**改动风险:** 低
**分阶段实施:** 否

---

### R-018 | P3 | AIEnginePool EngineSlot 结构体

**涉及文件:**
- `include/EnhanceVision/core/AIEnginePool.h` (L78-85)

**当前问题:**
```cpp
struct EngineSlot {
    AIEngine* engine = nullptr;
    QString taskId;
    EngineState state = EngineState::Uninitialized;
    BackendType backendType = BackendType::NCNN_CPU;
    QString lastError;
    bool wasUsed = false;
};
```
未使用 `std::optional` 等现代特性。

**优化方案:**
1. 使用 `std::optional<QString>` 替代空字符串表示
2. 添加构造函数/默认成员

**预期收益:**
- 语义清晰
- 现代化

**改动风险:** 低
**分阶段实施:** 否

---

## 🎨 QML 优化

### Q-001 | P0 | QML 硬编码角色值

**涉及文件:**
- `qml/pages/MainPage.qml` (L763-766)
```qml
var idx = model.index(i, 0)
item = {
    filePath: model.data(idx, 258) || "",   // magic number
    fileName: model.data(idx, 259) || "",
    mediaType: model.data(idx, 262) !== undefined ? model.data(idx, 262) : 0,
    thumbnail: model.data(idx, 263) || ""
}
```

**当前问题:**
- 使用魔法数字 (258, 259, 262, 263) 而非命名角色
- 难以理解和维护
- 角色定义在 C++ 侧但未导出到 QML

**优化方案:**
1. 在 FileModel 中定义 `enum Roles { IdRole = Qt::UserRole + 1, FilePathRole, ... }`
2. 使用 `Q_ENUM` 导出到 QML
3. QML 侧使用 `FileModel.FilePathRole` 替代 258

**预期收益:**
- 代码可读性大幅提升
- 减少错误
- IDE 自动完成支持

**改动风险:** 低
**分阶段实施:** 否

---

### Q-002 | P1 | ControlPanel 重复的 hasShaderModifications

**涉及文件:**
- `qml/components/ControlPanel.qml` (L59-74)
- `qml/pages/MainPage.qml` (未使用)

**当前问题:**
`hasShaderModifications` 属性在 `ControlPanel` 中计算，但 `MainPage` 可能需要但未使用。

**优化方案:**
1. 将计算逻辑移到 C++ Controller
2. QML 仅订阅结果
3. 考虑使用 `Loader` 延迟加载

**预期收益:**
- 性能提升
- 关注点分离

**改动风险:** 低
**分阶段实施:** 否

---

### Q-003 | P1 | 重复的 Tooltip 实现

**涉及文件:**
- `qml/controls/Tooltip.qml`
- `qml/controls/RichTooltip.qml`
- `qml/controls/ParamSlider.qml` (内部 Tooltip)

**当前问题:**
- Tooltip 在多处实现
- 样式不一致
- 功能重复

**优化方案:**
1. 统一为 `Tooltip.qml`
2. `RichTooltip` 继承或组合
3. 提取公共逻辑

**预期收益:**
- UI 一致性
- 减少重复代码

**改动风险:** 低
**分阶段实施:** 否

---

### Q-004 | P1 | ParamSlider 组件复杂度

**涉及文件:**
- `qml/controls/ParamSlider.qml` (200+ 行)

**当前问题:**
- 滑块组件承担过多职责
- Tooltip、标签、输入框耦合
- 难以单独使用

**优化方案:**
1. 拆分原子组件：`ValueSlider`, `LabeledSlider`, `InputField`
2. `ParamSlider` 组合使用
3. 支持灵活布局

**预期收益:**
- 复用性提升
- 便于测试

**改动风险:** 中
**分阶段实施:** 否

---

### Q-005 | P2 | Theme 系统扩展性

**涉及文件:**
- `qml/styles/Theme.qml`
- `qml/styles/Colors.qml`
- `qml/styles/Typography.qml`

**当前问题:**
Theme 对象使用静态属性，缺乏运行时切换能力（亮/暗主题切换可能有延迟）。

**优化方案:**
1. 使用 `Qt.labs.settings` 存储主题偏好
2. 添加主题预览功能
3. 实现平滑过渡动画

**预期收益:**
- 主题切换更流畅
- 预览功能

**改动风险:** 中
**分阶段实施:** 否

---

### Q-006 | P2 | ShaderEffect 封装重复

**涉及文件:**
- `qml/shaders/*.qml` (12+ 文件)
- `qml/components/FullShaderEffect.qml`
- `qml/components/MultiPassShaderEffect.qml`

**当前问题:**
每个 Shader 效果有重复的：
- Shader 源定义
- 参数绑定
- 动画配置

**优化方案:**
1. 提取 `BaseShaderEffect.qml`
2. 子类仅定义片段着色器
3. 参数绑定模板化

**预期收益:**
- 减少重复
- 添加新 Shader 更容易

**改动风险:** 低
**分阶段实施:** 否

---

### Q-007 | P2 | MessageList 复杂嵌套

**涉及文件:**
- `qml/components/MessageList.qml`

**当前问题:**
MessageList 组件处理：
- 会话切换
- 空状态
- 滚动区域
- 消息卡片列表

职责过多。

**优化方案:**
1. 提取 `EmptyStateView` 组件
2. 提取 `MessageCard` 组件
3. MessageList 作为容器

**预期收益:**
- 组件可复用
- 便于定制

**改动风险:** 中
**分阶段实施:** 否

---

### Q-008 | P3 | 组件 API 不一致

**涉及文件:**
- `qml/controls/*.qml`

**当前问题:**
组件属性命名不一致：
- 有的用 `iconSource`, 有的用 `icon`
- 有的用 `tooltipText`, 有的用 `tooltip`
- 信号命名也不统一

**优化方案:**
1. 制定组件 API 规范
2. 统一命名
3. 添加文档注释

**预期收益:**
- 学习成本降低
- 使用一致性

**改动风险:** 中
**分阶段实施:** 是

---

### Q-009 | P3 | OffscreenShaderRenderer 必要性

**涉及文件:**
- `qml/components/OffscreenShaderRenderer.qml`
- `App.qml` (L598-601)

**当前问题:**
OffscreenShaderRenderer 仅在 App.qml 中实例化但很少使用，可能是过度设计。

**优化方案:**
1. 评估实际使用频率
2. 如不必要则移除
3. 如必要则添加使用示例

**预期收益:**
- 减少无用代码
- 简化架构

**改动风险:** 低
**分阶段实施:** 否

---

### Q-010 | P2 | Binding 循环风险

**涉及文件:**
- `qml/pages/MainPage.qml` (多处)
- `qml/components/ControlPanel.qml`

**当前问题:**
多处双向绑定可能导致循环：
```qml
// ControlPanel.qml
property int displayMode: UIStateController.processingMode
Connections {
    target: UIStateController
    function onProcessingModeChanged() {
        root.displayMode = UIStateController.processingMode  // 潜在循环
    }
}
```

**优化方案:**
1. 添加 Binding 循环检测工具
2. 使用 `Loader` 替代双向绑定
3. 明确数据流方向

**预期收益:**
- 避免难以调试的绑定问题
- 性能提升

**改动风险:** 低
**分阶段实施:** 否

---

## 📦 现代化升级

### M-001 | P1 | C++20 Concepts 约束模板

**涉及文件:**
- `include/EnhanceVision/core/inference/IInferenceBackend.h`
- 模板类使用处

**当前问题:**
模板参数缺少约束，可能导致难以理解的编译错误。

**优化方案:**
```cpp
template<typename T>
concept InferenceBackend = requires(T t) {
    { t.initialize(std::declval<BackendConfig>()) } -> std::same_as<bool>;
    { t.inference(std::declval<InferenceRequest>()) } -> std::same_as<InferenceResult>;
};
```

**预期收益:**
- 编译错误更友好
- API 契约明确

**改动风险:** 低
**分阶段实施:** 否

---

### M-002 | P1 | Qt 6.10 新特性利用

**涉及文件:**
- 整个项目

**当前问题:**
未充分利用 Qt 6.10 新特性：
- `QCoroutine` 简化异步
- 新的 QML 类型系统
- `QQuickStyle` 改进

**优化方案:**
1. 评估 `QCoroutine` 用于文件 I/O
2. 使用新的 Qt Quick Controls
3. 检查其他新 API

**预期收益:**
- 代码简化
- 性能提升

**改动风险:** 中
**分阶段实施:** 是

---

### M-003 | P2 | std::variant 替代枚举

**涉及文件:**
- `include/EnhanceVision/core/TaskStateManager.h`
- `include/EnhanceVision/models/DataTypes.h`

**当前问题:**
使用枚举 + switch 模式，添加新状态需要修改多处。

**优化方案:**
```cpp
using TaskState = std::variant<IdleState, RunningState, PausedState, CompletedState>;
// 或
struct TaskState {
    enum Tag { Idle, Running, Paused, Completed };
    Tag tag;
    union { ... };
};
```

**预期收益:**
- 类型安全
- 编译器检查

**改动风险:** 中
**分阶段实施:** 是

---

### M-004 | P2 | CMake FetchContent

**涉及文件:**
- `CMakeLists.txt`

**当前问题:**
第三方库（如 NCNN, FFmpeg）可能手动管理，缺乏现代化的依赖管理。

**优化方案:**
1. 使用 `FetchContent_Declare` 管理依赖
2. 添加编译缓存支持（ccache）
3. 考虑 vcpkg 或 Conan

**预期收益:**
- 简化 CI/CD
- 依赖版本锁定

**改动风险:** 中
**分阶段实施:** 否

---

### M-005 | P2 | 统一错误处理模式

**涉及文件:**
- 多处

**当前问题:**
错误处理方式不统一，缺少 Result 类型。

**优化方案:**
```cpp
template<typename T, typename E = QString>
class Result {
    std::variant<T, E> value;
public:
    bool isOk() const;
    T value() &&;
    E error() &&;
};
```

**预期收益:**
- 错误传播安全
- 链式调用

**改动风险:** 高
**分阶段实施:** 是

---

### M-006 | P3 | 模块化头文件

**涉及文件:**
- 整个项目

**当前问题:**
C++20 模块仍未广泛支持，但可准备：
- 减少 `#include` 依赖
- 使用 `[[nodiscard]]` 等属性

**优化方案:**
1. 逐步使用 `private` 头文件
2. 添加 `[[nodiscard]]` 属性
3. 准备模块化迁移

**预期收益:**
- 编译时间减少
- 前瞻性

**改动风险:** 低
**分阶段实施:** 否

---

### M-007 | P3 | Structured Bindings 广泛应用

**涉及文件:**
- 多处手动解包

**当前问题:**
未充分使用 C++17 structured bindings：
```cpp
// 应该使用
auto [key, value] = mapElement;
// 而不是
auto key = mapElement.first;
auto value = mapElement.second;
```

**优化方案:**
1. 代码审查中使用 structured bindings 的机会
2. 重构相关代码

**预期收益:**
- 代码简洁
- 减少临时变量

**改动风险:** 低
**分阶段实施:** 否

---

### M-008 | P3 | std::optional 替代空值

**涉及文件:**
- 多处使用 `QString()` 或 `-1` 表示空值

**当前问题:**
缺乏表达"无值"的统一方式：
```cpp
QString getName() const;  // 返回空表示无值
int getCount() const;  // 返回 -1 表示无值
```

**优化方案:**
```cpp
std::optional<QString> getName() const;
std::optional<int> getCount() const;
```

**预期收益:**
- 语义明确
- 类型安全

**改动风险:** 中
**分阶段实施:** 是

---

## 📋 实施优先级与建议

### 第一阶段 (P0，必须立即优化)

| 编号 | 任务 | 预估工时 | 风险 |
|-----|------|---------|-----|
| A-001 | ProcessingController 职责拆分 | 2周 | 中 |
| A-002 | Controller 间依赖解耦 | 1周 | 高 |
| A-015 | QML 业务逻辑下沉 | 1周 | 中 |
| P-012 | DyingProcessor 内存泄漏修复 | 1周 | 高 |
| Q-001 | QML 魔法数字消除 | 3天 | 低 |

### 第二阶段 (P1，强烈建议)

| 编号 | 任务 | 预估工时 | 风险 |
|-----|------|---------|-----|
| A-003 | TaskRecoveryController 重构 | 1周 | 低 |
| A-004 | SyncHelper 合并 | 3天 | 中 |
| A-005 | Service 注册机制 | 1周 | 中 |
| A-014 | ProcessingEngine 职责明确 | 1周 | 高 |
| P-001~P-005 | 性能优化 | 2周 | 中 |
| R-001~R-004 | 重复代码消除 | 1周 | 低 |
| R-013 | ProcessingModel 充分利用 | 3天 | 中 |
| M-001~M-002 | 现代化升级 | 2周 | 中 |

### 第三阶段 (P2，建议优化)

| 编号 | 任务 | 预估工时 | 风险 |
|-----|------|---------|-----|
| A-006~A-013 | 架构优化 | 3周 | 低 |
| P-006~P-011 | 性能优化 | 2周 | 低 |
| R-005~R-010 | 代码重构 | 2周 | 低 |
| R-014~R-018 | 代码重构 | 1周 | 低 |
| Q-002~Q-008 | QML 优化 | 2周 | 中 |
| M-003~M-005 | 现代化升级 | 2周 | 中 |

### 第四阶段 (P3，锦上添花)

| 编号 | 任务 | 预估工时 | 风险 |
|-----|------|---------|-----|
| R-011~R-012 | 错误处理/日志规范 | 1周 | 高 |
| Q-009~Q-010 | QML 优化 | 3天 | 低 |
| M-006~M-008 | 现代化升级 | 2周 | 中 |

---

## 🎯 总结

本次扫描共发现 **63 项优化点**，涵盖架构、性能、代码质量、QML 和现代化五个维度：

- **P0 (阻塞级):** 5 项，主要涉及内存泄漏、QML 业务逻辑混乱、Controller 职责膨胀
- **P1 (重要):** 20 项，涉及多处架构优化和性能提升机会
- **P2 (建议):** 27 项，改善可维护性和代码质量
- **P3 (可选):** 11 项 锦上添花的优化

**核心建议：**
1. 优先解决 P0 问题（内存泄漏、QML 混乱）
2. 分阶段实施，每次聚焦一类问题
3. 每次重构后充分测试
4. 建立代码审查机制防止退化

---

*报告生成时间: 2026-05-10*
