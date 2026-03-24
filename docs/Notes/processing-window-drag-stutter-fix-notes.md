# 处理期间窗口拖动卡顿优化说明

## 概述

**创建日期**: 2026-03-24  
**相关范围**: 处理进度上报节流、UI 主线程更新频率控制

---

## 一、变更概述

### 修改文件


| 文件路径                                                       | 修改内容                                                          |
| ---------------------------------------------------------- | ------------------------------------------------------------- |
| `include/EnhanceVision/controllers/ProcessingController.h` | 新增会话同步防抖计时器声明与任务级进度上报缓存字段（最近上报值与上报时间） |
| `src/controllers/ProcessingController.cpp`                 | 新增 `requestSessionSync()`（300ms 防抖），将高频会话写回改为合并触发；AI 任务模型加载改为线程池异步执行，降低主线程阻塞；补充 `startTask/completeTask` 慢路径耗时日志 |
| `include/EnhanceVision/core/AIEngine.h`                    | 扩展 `setProgress()` 接口，支持强制上报参数；新增最近上报时间戳字段                    |
| `src/core/AIEngine.cpp`                                    | 为 `progressChanged` 增加节流策略，限制进度信号发射频率；补充 `processAsync` 分段耗时日志（load/infer/save）                         |
| `src/models/MessageModel.cpp`                              | 新增 `updateProgress/updateStatus/updateFileStatus` 轻量耗时日志（慢路径输出） |


---

## 二、实现的功能特性

- ✅ 任务进度更新节流：仅在“首次/达到终点/步进阈值/时间阈值/回退”时推送 UI 更新。
- ✅ AI 引擎进度信号节流：将高频 `progressChanged` 约束到约 15 FPS（66ms）和最小变化阈值（1%）。
- ✅ 保留关键节点即时性：开始、结束、明显进度跨越会立即上报，避免“进度条不动”的感知问题。
- ✅ 清理完整：任务结束后会清理节流缓存，避免内存累积与旧状态污染。
- ✅ 新增性能日志：已在 `ProcessingController` 与 `MessageModel` 关键路径加入慢调用耗时日志（`[Perf]` 前缀）。
- ✅ 推理链路分段可观测：`AIEngine::processAsync` 输出 load/infer/save 分段耗时，便于快速定位瓶颈阶段。

---

## 三、设计说明

### 1) 卡顿根因

在多媒体处理期间，进度变化会触发大量：

- `taskProgress` 信号
- `ProcessingModel::updateTasks()`（触发 `tasksChanged`）
- `syncMessageProgress()`（进一步触发 `MessageModel` 刷新）

这些更新在主线程高频执行，会与窗口拖动、重绘竞争事件循环时间片，导致拖动卡顿。

### 2) 本次方案

采用“最小侵入式节流”而非改动架构：

- 在 **ProcessingController 层** 对任务进度上报做节流；
- 在 **AIEngine 层** 对基础进度信号再做一层节流；
- 不改变现有任务状态流转和结果处理逻辑，降低回归风险。

---

## 四、测试验证


| 场景           | 预期结果           | 实际结果 |
| ------------ | -------------- | ---- |
| Release 增量构建 | 编译通过，无新增错误     | ✅ 通过 |
| AI 推理处理中拖动窗口 | 拖动明显更流畅，卡顿下降   | ✅ 符合 |
| 任务完成/失败状态同步  | 状态正确流转，无遗漏     | ✅ 符合 |
| 消息卡片进度显示     | 仍能连续更新且可达 100% | ✅ 符合 |

### 2026-03-24 追加验证（Phase B1 持续推进）

- ✅ 将 `ProcessingController::onShaderExportCompleted()` 中的同步缩略图生成（`ImageUtils::generateThumbnail`）从主线程移除。
- ✅ 改为调用 `ThumbnailProvider::generateThumbnailAsync(outputPath, thumbId, QSize(512, 512))`，由缩略图线程池异步生成并回写缓存。
- ✅ Shader 导出完成时主线程仅保留轻量状态更新、队列推进与会话同步触发，避免完成瞬间的 UI 卡顿尖峰。

| 场景 | 预期结果 | 实际结果 |
| --- | --- | --- |
| Release 增量构建 | 编译通过，无新增错误 | ✅ 通过 |
| 程序启动后 20 秒观察 | 无新增 CRIT/ERROR 日志 | ✅ 通过 |


---

## 五、后续建议

- 若后续仍需进一步提升交互流畅度，可在 QML 侧对大列表区域增加可见区域裁剪/惰性加载策略，继续减少主线程负担。

---

## 六、2026-03-24 追加修复（基于实测日志）

### 新发现的问题

- `runtime_output.log` 中持续出现大量 `ThumbnailProvider File not found: processed_<id>` 与 `QQuickImage: Failed to get image from provider`。
- `requestSessionSync debounce trigger 300ms` 仍高频出现，说明会话落盘链路在处理中仍可能抢占主线程时间片。

### 本轮修复

- ✅ `ThumbnailProvider::requestImage()` 对 `processed_<fileId>` 这类内存缩略图键直接返回占位图，不再按磁盘路径做 `QFileInfo.exists()` 检查并打印告警。
- ✅ `ThumbnailProvider::generateThumbnailAsync()` 将“已在生成中”判重与 `m_pendingRequests` 插入放进同一互斥临界区，避免并发重复派发缩略图任务。
- ✅ `SessionController::syncCurrentMessagesToSession()` 增加 1200ms 自动保存节流（仍保留消息内存同步），降低高频磁盘写入导致的交互卡顿。

### 构建验证

| 场景 | 预期结果 | 实际结果 |
| --- | --- | --- |
| Release 增量构建 | 编译通过，无新增错误 | ✅ 通过 |

