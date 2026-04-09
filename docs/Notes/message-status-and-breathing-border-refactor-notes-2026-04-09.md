# 消息状态与呼吸边框重构笔记

## 概述

本次重构统一了消息卡片状态来源，修复了“处理中卡片仅静态蓝框、不呼吸”和“全部完成后仍残留呼吸蓝框”的问题，并清理了任务调度链路中的高频冗余日志。

**创建日期**: 2026-04-09

---

## 变更概述

### 新增文件

| 文件路径 | 功能描述 |
|----------|----------|
| `include/EnhanceVision/controllers/MessageStatusResolver.h` | 统一消息状态派生接口（统计 + 状态决策 + 去重判定） |
| `src/controllers/MessageStatusResolver.cpp` | 消息状态派生实现 |
| `tests/MessageStatusResolverTest.cpp` | 消息状态派生与状态同步去重回归测试 |

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/controllers/ProcessingController.cpp` | 收口消息状态刷新入口，去除多处直接写状态；清理循环级日志噪声 |
| `src/controllers/ProgressSyncHelper.cpp` | 移除状态同步时间防抖，改为“仅重复值抑制” |
| `src/models/MessageModel.cpp` | 文件统计扩展为 6 维（含 paused/recoverable）并统一发信号 |
| `qml/components/MessageItem.qml` | 呼吸边框与等待动画改为声明式状态驱动，退出态强制复位 |
| `qml/components/MessageList.qml` | 使用完整 6 维统计信号，移除本地残值拼接 |
| `CMakeLists.txt` | 接入 `MessageStatusResolver` 及对应 QtTest |

---

## 关键设计决策

1. 消息状态只由 C++ 派生链路产出，QML 不再推导业务状态。
2. 状态同步即时发送，不再使用 300ms 防抖，避免 `Processing -> Completed` 尾状态丢失。
3. 呼吸边框动画只在 `status == Processing && processingFileCount > 0` 时运行，其他状态显式复位动画属性。

---

## 日志与告警审视结论

当前运行日志未发现新的崩溃级错误。可见告警主要为：

- `SettingsController` 异常退出检测告警：属于上次异常退出后的预期提示。
- `AIVideoProcessor` 大尺寸视频分块处理告警：属于信息性提示，不是功能错误。

---

## 测试验证

| 场景 | 结果 |
|------|------|
| Debug 全量构建 | ✅ 通过 |
| `EnhanceVisionSessionPruneUtilsTest` | ✅ 通过 |
| `EnhanceVisionMessageStatusResolverTest` | ✅ 通过 |

