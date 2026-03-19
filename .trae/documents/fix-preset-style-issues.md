# 修复 Shader 模式下快速预设风格问题

## 问题描述

在 Shader 模式下，当用户点击"快速预设"中的某个"风格"时，存在以下问题：

1. **风格按钮没有持久显示蓝色背景**：点击风格后，虽然参数调整区域能够自动变化，也能影响预览区域的图片，但风格按钮没有持续显示蓝色背景（当参数与风格参数匹配时应该显示）

2. **消息发送后风格效果未应用**：

   * 消息展示区域的缩略图没有应用风格效果

   * 放大查看时的图片没有应用风格效果

   * 窗口底部的缩略图没有应用风格效果

## 问题根因分析

### 问题1：风格按钮蓝色背景不持久

**相关文件**: [qml/components/ShaderParamsPanel.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/ShaderParamsPanel.qml)

风格按钮的 `isActive` 属性定义（第464-465行）：

```qml
property bool isActive: currentPreset && currentPreset.name === modelData.name && isCurrentPresetActive()
```

`isCurrentPresetActive()` 函数（第58-74行）检查当前参数是否与预设参数匹配。

**根因**：代码逻辑本身是正确的，但需要验证以下几点：

1. `currentPreset` 是否在点击时被正确设置
2. `isCurrentPresetActive()` 函数中的参数比较是否正确
3. 参数变化时是否正确触发了重新计算

### 问题2：消息发送后风格效果未应用

**相关文件**: [src/models/MessageModel.cpp](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/models/MessageModel.cpp)

在 `MessageModel::data()` 函数（第47-50行）中，`hasShaderModifications` 的判断不完整：

```cpp
bool hasShaderModifications = 
    qAbs(message.shaderParams.brightness) > 0.001f ||
    qAbs(message.shaderParams.contrast - 1.0f) > 0.001f ||
    qAbs(message.shaderParams.saturation - 1.0f) > 0.001f;
```

这只检查了 3 个参数，但 `ShaderParams` 结构体包含 14 个参数。而在 `MessageModel::getMediaFiles()` 函数中，同样的判断包含了所有 14 个参数。

**根因**：`MessageModel::data()` 中的 `hasShaderModifications` 判断不完整，导致某些情况下 `processedThumbnailId` 没有被正确设置，缩略图显示的是原图而非处理后的图片。

## 解决方案

### 修复1：完善 MessageModel::data() 中的参数检查

**文件**: `src/models/MessageModel.cpp`

**位置**: 第47-50行

**修改内容**: 将 `hasShaderModifications` 的判断扩展为检查所有 14 个 Shader 参数，与 `getMediaFiles()` 函数中的判断保持一致。

```cpp
bool hasShaderModifications = 
    qAbs(message.shaderParams.brightness) > 0.001f ||
    qAbs(message.shaderParams.contrast - 1.0f) > 0.001f ||
    qAbs(message.shaderParams.saturation - 1.0f) > 0.001f ||
    qAbs(message.shaderParams.hue) > 0.001f ||
    qAbs(message.shaderParams.sharpness) > 0.001f ||
    qAbs(message.shaderParams.blur) > 0.001f ||
    qAbs(message.shaderParams.denoise) > 0.001f ||
    qAbs(message.shaderParams.exposure) > 0.001f ||
    qAbs(message.shaderParams.gamma - 1.0f) > 0.001f ||
    qAbs(message.shaderParams.temperature) > 0.001f ||
    qAbs(message.shaderParams.tint) > 0.001f ||
    qAbs(message.shaderParams.vignette) > 0.001f ||
    qAbs(message.shaderParams.highlights) > 0.001f ||
    qAbs(message.shaderParams.shadows) > 0.001f;
```

### 修复2：验证风格按钮的蓝色背景逻辑

**文件**: `qml/components/ShaderParamsPanel.qml`

风格按钮的 `isActive` 属性依赖于：

1. `currentPreset` 是否被正确设置
2. `isCurrentPresetActive()` 函数的返回值

需要确保：

1. `applyPreset()` 函数正确设置 `currentPreset`
2. `isCurrentPresetActive()` 函数正确比较所有参数
3. 参数变化时触发属性重新计算

**当前代码已正确实现**，但可能需要添加调试日志来验证问题是否由其他原因引起。

## 实施步骤

1. **修改 MessageModel.cpp**

   * 更新 `data()` 函数中的 `hasShaderModifications` 判断

   * 确保与 `getMediaFiles()` 函数中的判断一致

2. **验证 ShaderParamsPanel.qml**

   * 检查 `applyPreset()` 函数是否正确设置 `currentPreset`

   * 检查 `isCurrentPresetActive()` 函数是否正确比较参数

   * 如有必要，添加调试日志

3. **测试验证**

   * 构建项目

   * 测试风格按钮点击后是否显示蓝色背景

   * 测试发送消息后缩略图是否显示处理后的图片

   * 测试放大查看时是否显示处理后的图片

## 影响范围

* `src/models/MessageModel.cpp` - 修改 `data()` 函数

* `qml/components/ShaderParamsPanel.qml` - 可能需要调试/验证

## 预期结果

1. 点击风格按钮后，按钮显示蓝色背景
2. 当参数与风格参数匹配时，蓝色背景持续显示
3. 当参数被修改且不再匹配风格参数时，蓝色背景消失
4. 发送消息后，消息展示区域的缩略图显示处理后的图片
5. 放大查看时，显示处理后的图片
6. 窗口底部的缩略图显示处理后的图片

