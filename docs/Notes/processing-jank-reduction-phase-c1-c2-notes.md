# processing-jank-reduction-phase-c1-c2-notes

## 概述

**创建日期**: 2026-03-24  
**相关计划**: `docs/Plan/多线程改进计划.md`

---

## 一、变更概述

### 修改文件


| 文件路径                                                | 修改内容                                                                 |
| --------------------------------------------------- | -------------------------------------------------------------------- |
| `include/EnhanceVision/models/ProcessingModel.h`    | 为 `tasksChanged` 增加定时节流字段与调度函数声明                                     |
| `src/models/ProcessingModel.cpp`                    | 将高频 `tasksChanged` 改为 66ms 合并触发，避免处理期 UI 频繁重绘                        |
| `include/EnhanceVision/controllers/ProcessingController.h` | 新增交互优先模式与导入削峰模式接口（`setInteractionPriorityMode/setImportBurstMode`） |
| `src/controllers/ProcessingController.cpp`          | 落地会话切换优先级让渡与批量导入并发削峰，并引入动态并发上限刷新逻辑                                 |
| `src/controllers/SessionController.cpp`             | 在会话切换路径中统一触发 `onSessionChanging`，保证任务优先级切换与会话切换动作同步                |


---

## 二、实现的功能特性

- ✅ **处理队列 UI 刷新节流**：高频任务状态更新不再每次立即触发 `tasksChanged`。
- ✅ **会话列表变更批处理**：会话消息数量/修改时间的通知由即时改为短周期合并。
- ✅ **会话同步写回频率下降**：处理中拖动窗口时，session 同步与持久化对主线程的抢占降低。

---

## 三、技术实现细节

### 1) ProcessingModel 节流

- 新增 `m_tasksChangedTimer`（singleShot, 66ms）与 `scheduleTasksChanged()`。
- `addTask/updateTasks/updateTask/updateTaskProgress/updateTaskStatus/removeTask` 全部改为调用 `scheduleTasksChanged()`。

### 2) SessionController 批量通知

- `updateMessageInSession()` 不再直接调用 `notifySessionDataChanged()`。
- 改为将 `sessionId` 写入 `m_pendingNotifySessionIds`，由 `m_sessionNotifyTimer`（120ms）统一触发。

### 3) 防抖参数调整

- `ProcessingController::m_sessionSyncTimer`：`300ms -> 500ms`。
- `SessionController::m_saveTimer`：`1200ms -> 1800ms`。

---

## 四、测试验证


| 场景                | 预期结果        | 实际结果             |
| ----------------- | ----------- | ---------------- |
| Release 全量构建      | 编译通过        | ✅ 通过             |
| Processing 高频状态更新 | 不出现编译/运行时错误 | ✅ 通过（日志与构建正常）    |
| 会话同步触发            | 防抖周期更新生效    | ✅ 已生效（日志触发文案已同步） |


---

## 五、后续工作（按计划继续）

### 已完成的续推进（2026-03-24 同日）

- ✅ Phase B2：AI 模型后台预加载已落地（`requestModelPreload`），减少任务启动瞬时阻塞。
- ✅ Phase B1：视频缩略图与 Shader 相关处理链路已异步化（`processShaderVideoThumbnailAsync`）。
- ✅ 线程优先级治理：工作线程池优先级已下调（`QThread::LowPriority`），确保 UI 线程抢占优先。
- ✅ Phase E1：会话切换时旧会话任务降级后台优先、新会话任务提升交互优先。
- ✅ Phase E2：批量导入/重传触发并发削峰（`N -> max(1, N-1)`），队列清空后自动恢复。

### Phase D 预验收快照（当前日志）

- 日志窗口：`logs/runtime_output.log`（当前采样）
- 统计结果：
  - `[Perf][ProcessingController]`：`12` 次
  - `[Perf][MessageModel]`：`1` 次
  - `requestSessionSync debounce trigger 500ms`：`12` 次
- 当前结论：未见 `CRIT/ERROR`，会话同步触发频率已进入 500ms 防抖口径；但仍缺少渲染层 95/99 分位帧时长统计，需按计划补齐后完成最终验收。

### 仍待完成

- ⏳ Phase C3：QML 重型 delegate 的 `Loader` 化与绑定瘦身逐页排查。
- ⏳ Phase E3：不可见会话写回批处理窗口补齐（当前已做到仅活跃会话回传到 `MessageModel`）。
- ⏳ Phase D：按 10/30/50 文件场景执行拖动窗口回归，并记录 95/99 分位帧时长数据。

---

## 六、Phase D - S1 验收记录（10 文件 + 持续拖动 30s）

**执行时间**: 2026-03-24 07:13（本地）

### 日志统计

- `[Perf][ProcessingController]`：5 次
- `[Perf][MessageModel]`：1 次
- `requestSessionSync debounce trigger 500ms`：4 次
- `[CRIT]`：0
- `[ERROR]`：0

### 结果结论（S1）

- ✅ 程序稳定运行，日志无错误级别输出。
- ✅ 会话同步触发维持在 500ms 防抖节奏内，未见异常刷屏。
- ⏳ 帧时长 95/99 分位仍未接入自动统计，S1 暂按“临时口径”通过（日志 + 体感 + 功能一致性）。

### 下一步

- 继续执行 S2（30 文件：拖动 + 会话切换 + 返回）。
- 执行 S3（50 文件：拖动 + 批量重传/导入 + 页面切换）。

---

## 七、Phase D - S2 验收记录（30 文件 + 拖动 + 会话切换 + 返回）

**执行时间**: 2026-03-24 07:15（本地）

### 日志统计

- `[Perf][ProcessingController]`：6 次
- `[Perf][MessageModel]`：0 次
- `requestSessionSync debounce trigger 500ms`：6 次
- `[CRIT]`：0
- `[ERROR]`：0

### 结果结论（S2）

- ✅ 会话切换与返回过程无错误级日志，稳定性通过。
- ✅ `requestSessionSync` 触发频率维持防抖预期，未见异常高频抖动。
- ✅ E1（会话切换优先级让渡）在本轮场景下未出现回退迹象。
- ⏳ 与 S1 同口径：渲染层 95/99 分位帧时长尚未接入自动统计，S2 暂按“临时口径”通过。

### 下一步

- ✅ S3 已执行完成（50 文件：拖动 + 批量重传/导入 + 页面切换）。

---

## 八、Phase D - S3 验收记录（50 文件 + 拖动 + 批量重传/导入 + 页面切换）

**执行时间**: 2026-03-24 07:17（本地）

### 日志统计

- `[Perf][ProcessingController]`：11 次
- `[Perf][MessageModel]`：5 次
- `requestSessionSync debounce trigger 500ms`：11 次
- `[CRIT]`：0
- `[ERROR]`：0

### 结果结论（S3）

- ✅ 批量重传/导入叠加页面切换场景下无错误级日志，稳定性通过。
- ✅ E2（导入削峰）场景下未见异常回退或明显同步风暴日志。
- ✅ 会话同步仍维持 500ms 防抖节奏，未出现失控高频写回迹象。
- ⏳ 与 S1/S2 同口径：95/99 分位帧时长尚未自动采集，S3 暂按“临时口径”通过。

---

## 九、Phase D 阶段性汇总（S1+S2+S3）

### 阶段结论

- ✅ 三个场景（S1/S2/S3）均无 `CRIT/ERROR`，功能一致性与稳定性满足当前阶段目标。
- ✅ 会话同步防抖（500ms）在三组场景均保持可控。
- ✅ 会话切换优先级让渡（E1）与导入削峰（E2）未观察到回退。
- ⏳ 最终“严格验收”仍需补齐渲染层帧统计（95/99 分位）。

### 建议收尾动作

1. 增加渲染层帧时长采集（QML 帧回调或渲染统计）。
2. 在相同三场景下补跑一次“严格口径”验收。
3. 若严格口径通过，可将 Phase D 标记为完成。

---

## 十、补帧统计（Phase D 严格口径）

**完成时间**: 2026-03-24

### 已落地内容

- ✅ 在 `Application` 启动后挂载 `FrameTimeProbe`（基于 `QQuickWindow::frameSwapped`）。
- ✅ 新增 10 秒窗口统计日志，统一输出：
  - `samples`
  - `avg`
  - `p50`
  - `p95`
  - `p99`
  - `max`
  - `>16.7ms` / `>33.3ms` / `>50ms` / `>100ms`
- ✅ 程序退出时增加一次 `[Final]` 汇总输出，避免最后一个采样窗口丢失。

### 当前结论

- Phase D 的帧统计采集链路已补齐，可直接用于 S1/S2/S3 的“严格口径”复验。
- 后续仅需按既定三场景回归并记录上述指标，即可完成 Phase D 收尾。

