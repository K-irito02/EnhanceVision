# AI 模型集成功能优化与重构 Spec

## Why

当前 AI 模型集成存在以下问题：
1. **models.json 中大部分模型的 `supportedParams` 为空**，导致控制面板无法显示模型特定参数
2. **RIFE 视频插帧模型未注册**，虽然模型文件存在但无法通过 UI 使用
3. **AIParamsPanel 参数显示逻辑不完整**，部分参数类型（如 bool）未处理
4. **缺少模型参数验证和默认值处理**，可能导致推理异常

## What Changes

- 完善 models.json 中所有模型的 `supportedParams` 配置
- 添加 RIFE 视频插帧模型到 models.json
- 增强 AIParamsPanel 支持更多参数类型（bool、string）
- 添加模型参数验证和默认值处理逻辑
- 优化 AIEngine 参数传递机制

## Impact

- Affected specs: AI 推理功能、模型配置系统
- Affected code: 
  - `resources/models/models.json`
  - `qml/components/AIParamsPanel.qml`
  - `src/core/AIEngine.cpp`
  - `src/core/ModelRegistry.cpp`

## ADDED Requirements

### Requirement: 模型参数配置完善

系统应为每个 AI 模型提供完整的 `supportedParams` 配置，确保用户可以通过控制面板调整所有可用参数。

#### Scenario: 超分辨率模型参数配置
- **WHEN** 用户选择 Real-ESRGAN 模型
- **THEN** 控制面板应显示 `outscale`（输出放大倍数）参数，范围为 1-4，默认为模型定义的 scaleFactor

#### Scenario: Waifu2x 模型参数配置
- **WHEN** 用户选择 Waifu2x 模型
- **THEN** 控制面板应显示去噪等级选择（0-3）

#### Scenario: SRMD 模型参数配置
- **WHEN** 用户选择 SRMD 模型
- **THEN** 控制面板应显示噪声级别参数（-1 到 10）

### Requirement: RIFE 视频插帧模型集成

系统应支持 RIFE 视频插帧模型，允许用户选择不同版本进行帧插值处理。

#### Scenario: RIFE 模型选择
- **WHEN** 用户选择视频插帧类别
- **THEN** 系统应显示可用的 RIFE 模型版本（v4.6、HD、UHD、anime）

#### Scenario: RIFE 参数配置
- **WHEN** 用户选择 RIFE 模型
- **THEN** 控制面板应显示：
  - `time_step`（时间步，0-1，默认 0.5）
  - `uhd_mode`（UHD 模式开关）
  - `tta_mode`（TTA 模式开关）

### Requirement: 参数类型扩展支持

AIParamsPanel 应支持更多参数类型，确保所有模型参数都能正确显示和配置。

#### Scenario: 布尔类型参数
- **WHEN** 模型参数类型为 `bool`
- **THEN** 控制面板应显示开关控件

#### Scenario: 枚举类型参数
- **WHEN** 模型参数类型为 `enum`
- **THEN** 控制面板应显示下拉选择框

#### Scenario: 数值类型参数
- **WHEN** 模型参数类型为 `int` 或 `float`
- **THEN** 控制面板应显示滑块控件，并显示当前值

### Requirement: 参数验证与默认值

系统应在推理前验证参数有效性，并为未设置的参数提供默认值。

#### Scenario: 参数验证
- **WHEN** 用户设置的参数超出允许范围
- **THEN** 系统应自动将参数限制在有效范围内

#### Scenario: 默认值应用
- **WHEN** 用户未设置某个参数
- **THEN** 系统应使用模型配置中定义的默认值

## MODIFIED Requirements

### Requirement: AIParamsPanel 参数显示逻辑

增强 AIParamsPanel 以支持：
1. 布尔类型参数（Switch 控件）
2. 字符串类型参数（TextField 控件）
3. 参数标签国际化（支持 label/label_en）
4. 参数描述提示

### Requirement: AIEngine 参数传递

优化 AIEngine 的参数传递机制：
1. 在推理前合并用户参数和模型默认参数
2. 验证参数范围
3. 支持模型特定参数处理

## REMOVED Requirements

无移除的需求。
