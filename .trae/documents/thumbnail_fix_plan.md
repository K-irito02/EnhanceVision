# 缩略图不显示问题排查与修复计划

## 问题概述
用户反馈："(2) 上传多媒体文件预览区"和"(3) 已发送消息展示区"没有显示缩略图。

## 分析结果

通过检查代码，发现以下可能的问题：

### 1. 路径处理问题
- `MainPage.qml` 中的 `_addDemoFiles` 函数直接将 URL 作为 thumbnail 保存
- `MediaThumbnailStrip.qml` 中，Image 组件直接使用 itemData.thumbnail 或拼接 "image://thumbnail/" + filePath
- Windows 路径可能包含反斜杠，需要正确处理

### 2. URL 编码问题
- QML 传入的路径可能是 `file:///E:/path` 格式
- ThumbnailProvider 虽然有部分处理，但可能不够完善

### 3. 缺少调试信息
- 关键位置没有日志输出，难以定位问题

---

## 任务分解

### [x] Task 1: 分析问题根源（已完成）
- **Priority**: P0
- **Depends On**: None
- **Description**: 检查 URL 编码和路径处理逻辑
- **Success Criteria**: 明确问题所在
- **Test Requirements**:
  - `programmatic` TR-1.1: 分析代码路径处理逻辑
  - `human-judgement` TR-1.2: 确定修复方案

---

### [x] Task 2: 添加调试日志（已完成）
- **Priority**: P0
- **Depends On**: Task 1
- **Description**: 在关键位置添加调试日志，包括：
  - ThumbnailProvider::requestImage() - 输出接收到的 id 和处理后的路径
  - ThumbnailProvider - 输出缩略图生成结果
  - MediaThumbnailStrip - 输出 thumbnail source 值
  - ImageUtils - 输出文件加载/生成结果
- **Success Criteria**: 所有关键路径都有日志输出
- **Test Requirements**:
  - `programmatic` TR-2.1: 编译通过 ✅
  - `human-judgement` TR-2.2: 日志包含足够信息定位问题 ✅

---

### [x] Task 3: 修复路径处理问题（已完成）
- **Priority**: P0
- **Depends On**: Task 2
- **Description**:
  - 完善 ThumbnailProvider 的路径处理逻辑
  - 确保正确处理 file:///、file:///E:/ 等格式
  - 确保正确处理 Windows 路径的反斜杠
  - 在 MediaThumbnailStrip 中，确保路径正确传递给 Image
  - **修复 Connections 信号监听问题**（关键修复）
- **Success Criteria**: 路径处理逻辑正确且健壮
- **Test Requirements**:
  - `programmatic` TR-3.1: 编译通过 ✅
  - `human-judgement` TR-3.2: 代码逻辑清晰，处理多种路径格式 ✅

---

### [x] Task 4: 构建项目并检查错误（已完成）
- **Priority**: P1
- **Depends On**: Task 3
- **Description**: 构建项目，检查是否有编译错误或警告，解决发现的问题
- **Success Criteria**: 项目编译无错误
- **Test Requirements**:
  - `programmatic` TR-4.1: CMake 配置成功 ✅
  - `programmatic` TR-4.2: 编译无错误 ✅
  - `programmatic` TR-4.3: 无严重警告 ✅

---

### [x] Task 5: 测试缩略图显示功能（已完成）
- **Priority**: P1
- **Depends On**: Task 4
- **Description**: 运行项目，添加图片/视频文件，测试缩略图是否正确显示
- **Success Criteria**: 缩略图正常显示
- **Test Requirements**:
  - `human-judgement` TR-5.1: 上传文件预览区显示缩略图 ✅
  - `human-judgement` TR-5.2: 消息展示区显示缩略图 ✅
  - `programmatic` TR-5.3: 控制台日志输出正常 ✅

---

## 风险与注意事项

1. **路径格式差异**：Windows 和 Linux/Mac 的路径格式不同，需要同时支持
2. **FFmpeg 依赖**：视频缩略图生成依赖 FFmpeg，确保库正确链接
3. **性能考虑**：缩略图生成可能耗时，确保不会阻塞 UI
