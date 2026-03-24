# AI推理模式全链路功能修复 Spec

## Why
AI推理模式存在三个关键功能性问题：多媒体文件处理异常、缩略图显示错误、对比功能失效，严重影响用户体验和功能可用性。

## What Changes
- 修复AI推理模式下多媒体文件处理流程，确保文件正确进入处理队列并完成AI推理
- 修复缩略图生成逻辑，确保显示AI处理后的结果而非原图
- 修复图像渲染中的黑灰色痕迹问题
- 修复对比功能，确保"源件"和"结果"按钮正确展示原始文件和AI处理后的文件

## Impact
- Affected specs: AI推理引擎、缩略图提供者、消息模型、媒体查看器
- Affected code:
  - `src/controllers/ProcessingController.cpp` - 任务调度和处理流程
  - `src/core/AIEngine.cpp` - AI推理执行
  - `src/providers/ThumbnailProvider.cpp` - 缩略图生成
  - `src/models/MessageModel.cpp` - 消息和文件状态管理
  - `qml/components/MediaViewerWindow.qml` - 对比功能实现
  - `qml/components/MediaThumbnailStrip.qml` - 缩略图显示

## ADDED Requirements

### Requirement: AI推理多媒体文件处理
系统应正确处理AI推理模式下的多媒体文件，包括图片和视频帧。

#### Scenario: 图片文件AI处理成功
- **WHEN** 用户在消息预览区域发送图片文件并选择AI推理模式
- **THEN** 系统应将文件加入处理队列，调用AI模型进行处理，并返回处理结果

#### Scenario: 视频文件AI处理成功
- **WHEN** 用户在消息预览区域发送视频文件并选择AI推理模式
- **THEN** 系统应提取视频帧，逐帧进行AI处理，并合成处理后的视频

#### Scenario: 处理失败时提供明确错误信息
- **WHEN** AI处理过程中发生错误
- **THEN** 系统应显示具体的错误信息，并提供重试选项

### Requirement: 缩略图正确显示AI处理结果
系统应在消息卡片的缩略图区域显示AI处理后的结果，而非原始文件。

#### Scenario: 处理完成后缩略图更新
- **WHEN** AI处理完成并生成结果文件
- **THEN** 缩略图应自动更新为AI处理后的图像

#### Scenario: 缩略图无黑灰色痕迹
- **WHEN** 用户查看AI处理后的缩略图或放大图像
- **THEN** 图像应完整显示，无异常的黑灰色修改痕迹

### Requirement: 对比功能正确展示源件与结果
系统应正确实现"源件"和"结果"对比功能，让用户清晰看到AI处理前后的差异。

#### Scenario: 查看源件
- **WHEN** 用户点击"源件"按钮
- **THEN** 系统应显示原始未处理的文件

#### Scenario: 查看结果
- **WHEN** 用户点击"结果"按钮
- **THEN** 系统应显示AI处理后的文件

#### Scenario: 对比按钮可见性
- **WHEN** AI处理成功完成
- **THEN** 对比按钮应可见且可点击

## MODIFIED Requirements

### Requirement: ProcessingController任务调度
ProcessingController应正确管理AI推理任务的生命周期，确保任务状态与文件状态同步更新。

**修改内容：**
- 确保AI处理完成后正确更新MediaFile的resultPath
- 确保originalPath在处理开始时被正确设置
- 确保缩略图版本号在处理完成后正确递增

### Requirement: ThumbnailProvider缩略图生成
ThumbnailProvider应根据文件处理状态选择正确的图像源。

**修改内容：**
- 处理完成后应使用resultPath生成缩略图
- 支持异步更新缩略图并通知QML刷新

### Requirement: MediaViewerWindow对比功能
MediaViewerWindow应正确读取和显示originalPath与resultPath。

**修改内容：**
- 修复对比按钮的显示条件判断
- 确保图像源路径正确切换

## REMOVED Requirements
无移除的需求。
