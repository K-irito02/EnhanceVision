# Shader 模式性能优化与删除确认对话框开发笔记

## 日期：2026-05-09

## 一、Shader 模式性能优化（第二轮）

### 问题
3.5GB 视频在 Shader 模式下处理时间超过 30 分钟，ETA 估算不准确。

### 优化措施

#### 1. 并行行处理（核心优化，预计 4-8x 加速）
- 使用 `QtConcurrent::blockingMap` 将图像按行范围分配到所有 CPU 线程并行处理
- 涉及函数：`applyShader()`、`applyBlur()`、`applyDenoiseWithNeighbors()`、`applySharpenWithNeighbors()`
- 调用 `result.detach()` 确保 QImage 数据不共享，避免并行写入时的 detach 竞争
- 当输入已是 `Format_RGB32` 时，使用 `input.copy()` 替代 `convertToFormat()`

#### 2. 修复 Shader 模式 ETA 估算不准确
- `estimateShaderTime()` 基准每帧时间从 60ms 调整为 100ms
- 加入并行加速因子：`parallelSpeedup = min(cpuThreadCount, 8)`
- 加入 CPU 频率校准：`freqRatio = 3.3GHz / 实际频率`

#### 3. 修复动态时间修正上限过低
- `calculateCorrectedRemainingTime()` 修正上限从 `1.2x` 提升到 `5.0x`
- 加入指数移动平均（EMA，α=0.3）平滑修正，避免预测时间跳动

## 二、消息卡片删除功能完善

### 问题
1. `removeSelectedMessages()` 不清理结果文件和缩略图缓存
2. `clear()` 不清理结果文件和缩略图缓存
3. 所有删除路径均不触发缓存管理刷新
4. 删除操作无确认对话框，用户无法选择删除内容

### 修复措施

#### 1. 提取公共资源清理方法
- 新增 `cleanupMessageResources(const Message&)` 统一处理结果文件删除和缩略图缓存清理
- 新增 `scheduleCacheRefresh()` 延迟 500ms 调用 `SettingsController::refreshDataSize()`
- `removeMessage()`、`removeSelectedMessages()`、`clear()`、`removeMediaFile()` 均调用

#### 2. 删除确认对话框
- 新增 `DeleteConfirmDialog.qml` 组件
- 三分类勾选：任务记录（必删）、处理结果文件（可选）、缩略图缓存（可选）
- 可折叠文件列表：显示每个文件的完整路径和大小
- 处理中任务显示黄色警告

#### 3. C++ API
- `getDeletionPreview(messageId)` — 获取单条消息删除预览
- `getBatchDeletionPreview(messageIds)` — 获取批量删除预览
- `removeMessageWithOptions(messageId, options)` — 按选项删除单条消息
- `removeSelectedMessagesWithOptions(options)` — 按选项批量删除

### 修改文件清单
- `src/core/ImageProcessor.cpp` — 并行行处理
- `src/core/TaskTimeEstimator.cpp` — ETA 估算修复
- `src/models/MessageModel.h` — 新增删除预览和条件删除 API
- `src/models/MessageModel.cpp` — 实现删除预览、条件删除、资源清理
- `qml/components/DeleteConfirmDialog.qml` — 新增删除确认对话框
- `qml/components/MessageList.qml` — 集成确认对话框
- `CMakeLists.txt` — 添加新 QML 文件
