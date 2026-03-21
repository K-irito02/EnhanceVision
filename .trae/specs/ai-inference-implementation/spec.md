# EnhanceVision AI 推理功能完整实现 - Product Requirement Document

## Overview
- **Summary**: 实现完整的 AI 推理功能，包括后端 NCNN 推理引擎、模型管理系统、前端 AI 模型选择界面，以及与现有处理流程的集成。
- **Purpose**: 为用户提供基于 AI 模型的图像/视频画质增强能力，支持超分辨率、去噪、去模糊、去雾、上色、低光增强、视频插帧等多种处理场景。
- **Target Users**: 需要对图像和视频进行专业级画质增强处理的桌面端用户。

## Why
当前项目已有完整的 Shader 滤镜处理流程，但 AI 推理功能仅为空壳实现。根据技术方案文档，需要完整实现 NCNN 推理引擎，支持已部署的多种 AI 模型，为用户提供更强大的画质增强能力。

## What Changes
- 完整实现 AIEngine 类（NCNN 推理引擎核心）
- 实现模型注册与发现系统（ModelRegistry）
- 实现模型参数配置系统
- 实现异步推理与进度回调机制
- 实现大图分块处理（Tile-based Processing）
- 重构 ControlPanel.qml 的 AI 模式界面
- 新增 AIModelPanel.qml 模型选择面板
- 新增 AIParamsPanel.qml 参数配置面板
- 集成 AIEngine 与 ProcessingEngine
- 实现推理进度与状态反馈

## Impact
- **Affected specs**: media-upload-send-processing (Task 10 需要更新)
- **Affected code**:
  - `src/core/AIEngine.cpp` - 完整重写
  - `include/EnhanceVision/core/AIEngine.h` - 完整重写
  - `src/core/ModelRegistry.cpp` - 新增
  - `include/EnhanceVision/core/ModelRegistry.h` - 新增
  - `src/core/ProcessingEngine.cpp` - 修改集成 AIEngine
  - `qml/components/ControlPanel.qml` - 重构 AI 模式
  - `qml/components/AIModelPanel.qml` - 新增
  - `qml/components/AIParamsPanel.qml` - 新增

## ADDED Requirements

### Requirement: AI 推理引擎核心
系统 SHALL 提供基于 NCNN 的 AI 推理引擎，支持 Vulkan GPU 加速。

#### Scenario: Vulkan 初始化成功
- **GIVEN** 系统支持 Vulkan
- **WHEN** AIEngine 初始化
- **THEN** 启用 Vulkan GPU 加速，记录 GPU 信息

#### Scenario: Vulkan 初始化失败回退
- **GIVEN** 系统不支持 Vulkan 或初始化失败
- **WHEN** AIEngine 初始化
- **THEN** 自动回退到 CPU 模式，记录警告日志

#### Scenario: 模型加载成功
- **GIVEN** 模型文件存在且格式正确
- **WHEN** 调用 loadModel(modelId)
- **THEN** 模型加载到内存，返回 true，发射 modelLoaded 信号

#### Scenario: 模型加载失败
- **GIVEN** 模型文件不存在或格式错误
- **WHEN** 调用 loadModel(modelId)
- **THEN** 返回 false，发射 processError 信号，包含错误信息

### Requirement: 模型注册与发现
系统 SHALL 自动发现并注册 resources/models 目录下的所有可用模型。

#### Scenario: 自动发现模型
- **GIVEN** resources/models 目录下存在模型文件
- **WHEN** ModelRegistry 初始化
- **THEN** 扫描并注册所有有效模型，生成模型元数据

#### Scenario: 按类别获取模型列表
- **GIVEN** 已注册多个模型
- **WHEN** 调用 getModelsByCategory(category)
- **THEN** 返回指定类别的模型列表

#### Scenario: 获取模型详细信息
- **GIVEN** 模型已注册
- **WHEN** 调用 getModelInfo(modelId)
- **THEN** 返回模型名称、描述、大小、支持的参数等信息

### Requirement: 异步推理处理
系统 SHALL 支持异步推理处理，不阻塞 UI 线程。

#### Scenario: 异步推理成功
- **GIVEN** 模型已加载
- **WHEN** 调用 processAsync(inputImage)
- **THEN** 在工作线程执行推理，发射进度信号，完成后发射 processCompleted 信号

#### Scenario: 取消推理
- **GIVEN** 推理正在进行
- **WHEN** 调用 cancelProcess()
- **THEN** 停止推理，发射 processError 信号，包含"已取消"信息

#### Scenario: 推理进度更新
- **GIVEN** 推理正在进行
- **WHEN** 推理进度变化
- **THEN** 发射 progressChanged 信号，进度值 0.0-1.0

### Requirement: 大图分块处理
系统 SHALL 支持大图分块处理，避免内存溢出。

#### Scenario: 自动分块
- **GIVEN** 输入图像尺寸超过阈值（如 1024x1024）
- **WHEN** 执行推理
- **THEN** 自动分割为多个块，分别处理后合并

#### Scenario: 分块边界处理
- **GIVEN** 图像被分块处理
- **WHEN** 合并结果
- **THEN** 使用重叠区域（overlap）消除边界痕迹

### Requirement: AI 模型选择界面
系统 SHALL 提供直观的 AI 模型选择界面。

#### Scenario: 显示模型类别
- **GIVEN** 用户进入 AI 模式
- **WHEN** 查看模型选择面板
- **THEN** 显示所有模型类别（超分辨率、去噪、去模糊等）

#### Scenario: 选择模型类别
- **GIVEN** 显示模型类别列表
- **WHEN** 用户点击某个类别
- **THEN** 显示该类别下的所有可用模型

#### Scenario: 选择模型
- **GIVEN** 显示模型列表
- **WHEN** 用户点击某个模型
- **THEN** 高亮选中，显示模型详细信息

#### Scenario: 模型信息展示
- **GIVEN** 用户选中某个模型
- **WHEN** 查看模型详情
- **THEN** 显示模型名称、描述、大小、预估处理时间

### Requirement: AI 参数配置
系统 SHALL 支持模型特定参数配置。

#### Scenario: 显示模型参数
- **GIVEN** 选中支持参数配置的模型
- **WHEN** 查看参数面板
- **THEN** 显示该模型支持的参数及其范围

#### Scenario: 调整参数
- **GIVEN** 参数面板显示参数
- **WHEN** 用户调整参数值
- **THEN** 实时更新参数值，预览效果（如适用）

### Requirement: 处理引擎集成
系统 SHALL 将 AIEngine 集成到 ProcessingEngine。

#### Scenario: AI 模式任务处理
- **GIVEN** 用户选择 AI 模式并发送处理
- **WHEN** ProcessingEngine 处理任务
- **THEN** 调用 AIEngine 执行推理，返回结果

#### Scenario: 推理进度同步
- **GIVEN** AI 推理正在进行
- **WHEN** 进度更新
- **THEN** 同步更新任务进度到 UI

## MODIFIED Requirements

### Requirement: ProcessingEngine 任务处理
ProcessingEngine SHALL 支持 Shader 和 AI 两种处理模式。

#### Scenario: 模式路由
- **GIVEN** 任务指定处理模式
- **WHEN** 开始处理
- **THEN** 根据模式调用 ImageProcessor 或 AIEngine

## REMOVED Requirements
无移除的需求。

## Non-Functional Requirements
- **NFR-1**: 推理响应时间：GPU 模式下 512x512 图像处理 < 500ms
- **NFR-2**: 内存管理：大图处理时内存增量 < 500MB
- **NFR-3**: UI 响应：推理过程中 UI 保持流畅（≥ 30fps）
- **NFR-4**: 错误恢复：推理失败时优雅降级，不崩溃

## Constraints
- **Technical**: 必须使用 NCNN、Vulkan、Qt 6.10.2、C++20
- **Business**: 遵循现有架构设计和 UI 设计规范
- **Dependencies**: 依赖已部署的模型文件

## Assumptions
- 用户设备支持 Vulkan 或有足够的 CPU 性能
- 模型文件格式正确且与 NCNN 兼容
- 用户了解不同模型的适用场景

## Acceptance Criteria

### AC-1: 模型自动发现
- **Given**: resources/models 目录下有模型文件
- **When**: 应用启动
- **Then**: 所有模型被正确注册和分类
- **Verification**: `programmatic`

### AC-2: 模型加载
- **Given**: 模型已注册
- **When**: 用户选择模型
- **Then**: 模型成功加载到内存
- **Verification**: `programmatic`

### AC-3: GPU 加速
- **Given**: 系统支持 Vulkan
- **When**: 执行推理
- **Then**: 使用 GPU 加速
- **Verification**: `programmatic`

### AC-4: 推理执行
- **Given**: 模型已加载，输入图像有效
- **When**: 执行推理
- **Then**: 输出正确的处理结果
- **Verification**: `human-judgment`

### AC-5: 进度显示
- **Given**: 推理正在进行
- **When**: 查看进度
- **Then**: 显示准确的进度百分比
- **Verification**: `programmatic`

### AC-6: 取消推理
- **Given**: 推理正在进行
- **When**: 用户取消
- **Then**: 推理停止，资源释放
- **Verification**: `programmatic`

### AC-7: 大图处理
- **Given**: 输入图像 > 1024x1024
- **When**: 执行推理
- **Then**: 自动分块处理，结果正确
- **Verification**: `human-judgment`

### AC-8: AI 模式 UI
- **Given**: 用户切换到 AI 模式
- **When**: 查看控制面板
- **Then**: 显示模型选择和参数配置界面
- **Verification**: `human-judgment`

### AC-9: 模型切换
- **Given**: 已加载模型 A
- **When**: 切换到模型 B
- **Then**: 模型 A 卸载，模型 B 加载
- **Verification**: `programmatic`

### AC-10: 错误处理
- **Given**: 推理发生错误
- **When**: 错误发生
- **Then**: 显示清晰的错误信息，不崩溃
- **Verification**: `programmatic`

## Open Questions
- [ ] 是否需要支持模型热更新（运行时添加新模型）？
- [ ] 是否需要保存用户的模型偏好设置？
- [ ] 视频插帧模型（RIFE）是否需要单独处理流程？
