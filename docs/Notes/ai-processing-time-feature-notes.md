# AI处理时间功能优化

## 概述

**创建日期**: 2026-04-05  
**相关功能**: AI处理任务的时间显示与持久化

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/MessageItem.qml` | 修复总耗时显示逻辑，使用taskId替代messageId |
| `qml/components/AIModelPanel.qml` | 优化模型性能提示，使用estimateMessageTotalTime模拟处理 |
| `qml/controls/RichTooltip.qml` | 优化弹窗UI布局，添加footerNote支持，统一蓝色配色 |
| `src/models/MessageModel.cpp` | 添加actualTotalSecUpdated信号触发会话保存 |
| `src/controllers/SessionController.cpp` | 连接actualTotalSecUpdated信号，实现总耗时持久化 |
| `include/EnhanceVision/models/MessageModel.h` | 添加actualTotalSecUpdated信号声明 |

---

## 二、实现的功能特性

- ✅ **总耗时显示**: 任务完成后在消息卡片右下角显示实际总耗时
- ✅ **总耗时持久化**: 重启应用后保留已完成任务的总耗时数据
- ✅ **模型性能提示**: 鼠标悬停AI模型时显示CPU/GPU处理时间预估
- ✅ **性能提示UI优化**: 统一蓝色配色，添加说明文字，优化布局间距

---

## 三、技术实现细节

### 总耗时计算与持久化流程

1. **计算时机**: `onAllFilesSettledChanged` 触发时，如果 `_processingStartTime > 0`
2. **计算方式**: `_actualTotalSec = (Date.now() - _processingStartTime) / 1000`
3. **持久化**: 调用 `messageModel.updateActualTotalSec(taskId, totalSec)`
4. **信号传递**: `MessageModel` 发出 `actualTotalSecUpdated` 信号
5. **会话保存**: `SessionController` 接收信号后调用 `syncCurrentMessagesToSession()` 和 `saveSessions()`
6. **JSON序列化**: `messageToJson` 保存 `actualTotalSec` 字段
7. **恢复**: `jsonToMessage` 读取 `actualTotalSec`，QML通过 `persistedActualTotalSec` 绑定恢复

### 关键修复

- **taskId vs messageId**: `MessageItem.qml` 中使用 `taskId` 属性而非不存在的 `messageId`
- **显示条件简化**: `visible: displayTotalSec > 0` 直接基于数据显示

### 性能提示计算

使用 `taskTimeEstimator.estimateMessageTotalTime()` 模拟处理标准文件：
- 图片: 1920×1080
- 视频: 10秒@30fps

---

## 四、遇到的问题及解决方案

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 总耗时不显示 | 使用了不存在的 `messageId` 属性 | 改用 `taskId` 属性 |
| 总耗时不持久化 | 缺少信号连接触发保存 | 添加 `actualTotalSecUpdated` 信号 |
| 性能提示不准确 | 静态硬编码值 | 使用 `estimateMessageTotalTime` 动态计算 |

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 处理任务完成 | 显示总耗时 | ✅ 通过 |
| 重启应用 | 保留总耗时 | ✅ 通过 |
| 悬停AI模型 | 显示性能预估 | ✅ 通过 |
| 性能提示UI | 蓝色统一，布局美观 | ✅ 通过 |

---

## 六、后续工作

- [ ] 考虑基于历史处理记录动态学习性能预估参数
- [ ] 添加更多分辨率的性能预估参考
