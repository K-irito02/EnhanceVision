# Windows 关闭残留进程与日志清理说明

## 概述

本次修复围绕 Windows 平台点击主窗口右上角 `X` 后的退出行为收口，并同步清理运行期信息日志，避免把低价值调试信息继续写入日志文件。

**创建日期**: 2026-04-12

---

## 变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/app/Application.cpp` | 关闭链路统一收口，补充 UI 析构阶段收尾，避免退出依赖 Qt 事件循环里的兜底定时器 |
| `src/core/LifecycleSupervisor.cpp` | 强退兜底改为独立 watchdog，并删除生命周期链路中的低价值 `qInfo()` 输出 |
| `src/controllers/ProcessingController.cpp` | 退出期改为限时收敛任务，保留 warning 级别关键告警 |
| `src/core/ProcessingTimeManager.cpp` | 删除任务时长管理中的启动/暂停/恢复信息日志 |
| `src/core/TaskTimeEstimator.cpp` | 删除硬件探测与任务跟踪中的信息级日志 |
| `src/core/ThumbnailDatabase.cpp` | 删除缩略图数据库初始化与清理统计日志 |
| `src/providers/ThumbnailProvider.cpp` | 删除持久化状态与清理结果的运行期信息日志 |
| `src/services/AutoSaveService.cpp` | 删除自动保存路径与成功完成的冗余信息日志，并改为无告警的线程池调用方式 |
| `src/controllers/UIStateController.cpp` | 删除状态保存/加载的常规信息日志 |
| `src/core/AIEnginePool.cpp` | 删除引擎池获取/释放/销毁过程中的低价值信息日志 |
| `src/models/MessageModel.cpp` | 删除性能打点 helper，避免模型层继续输出 `[Perf]` 类噪声 |
| `qml/components/ThumbnailStatusImage.qml` | 保持缩略图状态组件的加载守卫，避免视频资源误入图片加载链 |
| `qml/components/mediaViewer/MediaViewerCanvas.qml` | 为图片源增加格式守卫，避免视频路径进入 `Image`/`Loader` 触发 warning |
| `README.md` / `README_CN.md` | 补充 Windows 关闭清理与日志收敛说明 |
| `docs/Learning/rules/10-testing-logging-rules.md` | 强化运行日志治理与媒体源加载约束 |
| `docs/Learning/rules/12-maintenance-documentation-rules.md` | 补充关闭链路与日志清理的文档同步要求 |
| `docs/ApplicationData/StorageDirectory.md` | 说明运行时日志默认仅保留 warning/error/fatal |

---

## 结果

- Windows 主窗口关闭后，退出链路已按“先清理、再兜底”执行。
- 低价值信息日志已从多个常驻模块移除，避免继续刷屏。
- 媒体查看器的图片加载链增加了更严格的格式守卫，减少视频资源误入 `Image` 的机会。

---

## 测试验证

| 场景 | 结果 |
|------|------|
| Release 构建 | ✅ 通过 |
| 单元测试 `EnhanceVisionMessageStatusResolverTest` | ✅ 通过 |
| 单元测试 `EnhanceVisionSessionPruneUtilsTest` | ✅ 通过 |
| 主窗口关闭回归 | ✅ 通过一次干净关闭 |
| 运行日志审查 | ✅ 已检查，未见新代码错误 |

