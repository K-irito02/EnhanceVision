# Shader 效果一致性修复笔记

## 概述

修复了 shader 效果在预览区和消息区显示不一致的问题，以及风格按钮状态显示不正确的问题。

**创建日期**: 2026-03-19
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 创建的新组件

| 组件 | 文件路径 | 功能描述 |
|------|---------|---------|
| FullShaderEffect | qml/components/FullShaderEffect.qml | 封装 ShaderEffect 的组件，支持多参数 shader |
| OffscreenShaderRenderer | qml/components/OffscreenShaderRenderer.qml | 离屏 shader 渲染器，用于 GPU 导出处理后的图片 |

### 2. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| qml/components/MediaViewerWindow.qml | 简化 shader 应用逻辑，添加 _shouldApplyShader 属性 |
| qml/components/MessageList.qml | 移除重复的 _openViewer 函数，改进 shader 参数传递 |
| qml/components/ShaderParamsPanel.qml | 修复 isCurrentPresetActive 函数，删除调试日志 |
| qml/components/ControlPanel.qml | 删除参数变化调试日志 |
| src/models/MessageModel.cpp | 删除 MediaFilesRole 相关的 qDebug 输出 |
| resources/shaders/full_shader.frag | 新增完整 shader 实现 |
| resources/shaders/full_shader.vert | 新增 vertex shader |
| include/EnhanceVision/services/ | 新增 ImageExportService 头文件 |
| src/services/ | 新增 ImageExportService 实现文件 |
| tests/cpp/ | 新增 shader 一致性测试 |

### 3. 实现的功能特性

- ✅ 修复 shader 参数默认值处理问题
- ✅ 解决 GPU 导出与预览效果不一致问题
- ✅ 修复风格按钮状态持久显示问题
- ✅ 清理所有调试日志输出
- ✅ 简化 shader 应用逻辑

---

## 二、遇到的问题及解决方案

### 问题 1：Shader 效果在消息区重复叠加

**现象**：消息区放大查看时，shader 效果被重复应用，导致视觉效果异常。

**原因**：
1. `MessageList.qml` 中有重复的 `_openViewer` 函数定义
2. `MediaViewerWindow` 中的 shader 应用逻辑复杂，容易出错
3. GPU 导出时使用 `||` 操作符设置默认值，导致值为 0 的参数被错误替换

**解决方案**：
1. 移除 `MediaViewerWindow` 内部重复的 `_openViewer` 函数
2. 简化 `_shouldApplyShader` 逻辑：消息模式下永远不应用 shader
3. 使用 `!== undefined` 检查替代 `||` 操作符

### 问题 2：风格按钮状态不持久

**现象**：点击风格按钮后，蓝色背景不能持久显示。

**原因**：`isCurrentPresetActive()` 函数使用 `||` 操作符，导致值为 0 的参数被错误判断。

**解决方案**：改用 `!== undefined` 检查，确保所有参数值（包括 0）都能正确比较。

---

## 三、技术实现细节

### 1. Shader 参数默认值处理

**修复前**：
```qml
contrast: root.currentShaderParams.contrast || 1.0  // 如果 contrast = 0，会变成 1.0
```

**修复后**：
```qml
contrast: root.currentShaderParams.contrast !== undefined ? root.currentShaderParams.contrast : 1.0
```

### 2. MediaViewerWindow Shader 应用逻辑

```qml
// 修复后的简化逻辑
property bool _shouldApplyShader: {
    if (!shaderEnabled) return false
    // 消息模式下永远不应用 shader
    if (messageMode) return false
    return true
}
```

### 3. 风格按钮状态判断

```qml
function isCurrentPresetActive() {
    if (!currentPreset) return false
    
    // 使用 !== undefined 检查，避免值为 0 时被错误替换
    var presetContrast = currentPreset.contrast !== undefined ? currentPreset.contrast : 1.0
    var presetSaturation = currentPreset.saturation !== undefined ? currentPreset.saturation : 1.0
    
    return Math.abs(contrast - presetContrast) < 0.001 &&
           Math.abs(saturation - presetSaturation) < 0.001
    // ... 其他参数比较
}
```

---

## 四、参考资料

- [Qt ShaderEffect 文档](https://doc.qt.io/qt-6/qml-qtgraphicaleffects-shadereffect.html)
- [Qt QML 属性绑定](https://doc.qt.io/qt-6/qtqml-syntax-propertybinding.html)
- [JavaScript undefined vs null](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/undefined)
