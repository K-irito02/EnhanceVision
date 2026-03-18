# 修复 Shader 预览问题

## 问题分析

### 问题 1： 放大窗口大小比例自动变化

**根本原因**：`FullShaderEffect` 使用 `anchors.fill: imageViewSource`，但 `imageViewSource` 的 `fillMode: Image.PreserveAspectFit` 会影响其大小，但 `ShaderEffect` 本身没有 `fillMode` 属性，它会会填充整个区域，但 ShaderEffect 会根据图像实际尺寸来渲染，导致图像变形。

**解决方案**：ShaderEffect 需要继承 Image 的 `fillMode` 行为，或者使用 `width` 和 `height` 属性来保持图像比例。

### 问题 2: 许多 Shader 参数对预览没有变化

**根本原因**：日志显示参数传递正常，但 shader 中的参数名不匹配：
- MediaViewerWindow.qml 传递: `shaderTemperature`, `shaderTint`, `shaderHighlights`, `shaderShadows`
- FullShaderEffect.qml 定义的属性是 `temperature`, `tint`, `highlights`, `shadows`
- Shader 代码中使用的变量名是 `temperature`, `tint`, `highlights`, `shadows`

**解决方案**：确保 QML 属性名与 Shader uniform 变量名完全一致。

## 实施步骤

### 步骤 1：修复 FullShaderEffect.qml 添加 fillMode 和尺寸属性

### 步骤 2：修复 MediaViewerWindow.qml 参数名匹配问题

### 步骤 3：构建并测试
