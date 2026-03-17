# Shader 模式实时预览与查看原图功能修复计划

## 问题概述

### 问题 1：Shader 模式控制面板参数不影响放大查看时的多媒体文件
**现状：** `MediaViewerWindow` 是独立的窗口组件，只显示原始图片/视频，没有应用任何 Shader 效果。控制面板的参数（亮度、对比度、饱和度等）与查看器窗口完全隔离。

**根本原因：**
- `MediaViewerWindow.qml` 没有定义 Shader 参数属性
- 没有使用 `MultiEffect` 或 `ShaderEffect` 对媒体内容进行实时处理
- 查看器窗口与控制面板之间没有建立参数绑定

### 问题 2："查看原图/查看处理后"按钮功能不起作用
**现状：** 点击按钮后，显示的内容没有变化，都是原图。

**根本原因：**
在 `MessageList.qml` 第 248-259 行构建文件数据时：
```qml
var displayPath = resultPath !== "" ? resultPath : filePath
files.push({
    "filePath":  displayPath,  // 问题：filePath 被设置为 resultPath
    "resultPath": resultPath,
    "originalPath": filePath
})
```
这导致 `filePath` 和 `resultPath` 可能相同，切换 `showOriginal` 时显示相同内容。

---

## 实施方案

### 修复 1：为 MediaViewerWindow 添加 Shader 实时预览功能

#### 1.1 修改 `MediaViewerWindow.qml` - 添加 Shader 参数属性
```qml
// 在属性定义区域添加
property real shaderBrightness: 0.0
property real shaderContrast: 1.0
property real shaderSaturation: 1.0
property real shaderHue: 0.0
property real shaderSharpness: 0.0
property real shaderBlur: 0.0
property real shaderExposure: 0.0
property real shaderGamma: 1.0
property real shaderTemperature: 0.0
property real shaderTint: 0.0
property real shaderVignette: 0.0
property real shaderHighlights: 0.0
property real shaderShadows: 0.0
property bool shaderEnabled: false
```

#### 1.2 修改 `MediaViewerWindow.qml` - 为图片添加 MultiEffect 处理
将原来的 `Image` 组件包装为带 Shader 效果的版本：
```qml
// 使用 MultiEffect 对图片进行实时处理
Image {
    id: imageViewSource
    visible: false  // 源图片不可见
    // ... 原有属性
}

MultiEffect {
    id: imageViewEffect
    anchors.fill: imageViewSource
    source: imageViewSource
    visible: !isVideo && currentSource !== "" && shaderEnabled
    
    // 绑定 Shader 参数
    brightness: shaderBrightness
    contrast: shaderContrast
    saturation: shaderSaturation
    // ... 其他参数
}

// 当 shaderEnabled 为 false 时显示原图
Image {
    id: imageViewOriginal
    visible: !isVideo && currentSource !== "" && !shaderEnabled
    // ... 原有属性
}
```

#### 1.3 修改 `MainPage.qml` - 传递 Shader 参数到查看器
在 `pendingViewerWindow` 组件中添加属性绑定：
```qml
MediaViewerWindow {
    id: pendingViewerWindow
    messageMode: false
    
    // 添加 Shader 参数绑定
    shaderEnabled: root.processingMode === 0
    shaderBrightness: controlPanel.brightness
    shaderContrast: controlPanel.contrast
    shaderSaturation: controlPanel.saturation
    // ... 其他参数
}
```

#### 1.4 修改 `MessageList.qml` - 为消息模式查看器传递参数
需要通过信号或属性将 ControlPanel 的参数传递到 MessageList 的 viewerWindow。

### 修复 2：修复"查看原图/查看处理后"按钮功能

#### 2.1 修改 `MessageList.qml` - 修复文件数据构建逻辑
```qml
// 修改前（有问题）
var displayPath = resultPath !== "" ? resultPath : filePath
files.push({
    "filePath":  displayPath,
    "resultPath": resultPath,
    "originalPath": filePath
})

// 修改后（正确）
files.push({
    "filePath":  filePath,      // 始终是原始路径
    "resultPath": resultPath,   // 处理后的路径
    "originalPath": filePath
})
```

#### 2.2 修改 `MediaViewerWindow.qml` - 修改按钮文字
```qml
// 修改前
text: showOriginal ? qsTr("查看处理后") : qsTr("查看原图")

// 修改后
text: showOriginal ? qsTr("查看修改") : qsTr("查看原图")
```

---

## 详细修改文件清单

### 文件 1: `qml/components/MediaViewerWindow.qml`
**修改内容：**
1. 添加 Shader 参数属性（约 14 个属性）
2. 添加 `shaderEnabled` 属性控制是否启用 Shader
3. 重构图片显示区域，使用 `MultiEffect` 组件
4. 为视频添加 `VideoOutput` 的 `filters` 属性（如果支持）
5. 修改按钮文字从 "查看处理后" 改为 "查看修改"

### 文件 2: `qml/pages/MainPage.qml`
**修改内容：**
1. 为 `pendingViewerWindow` 添加 Shader 参数绑定
2. 需要从 App.qml 获取 ControlPanel 的引用或通过信号传递参数

### 文件 3: `qml/components/MessageList.qml`
**修改内容：**
1. 修复 `_openViewer` 函数中的文件数据构建逻辑
2. 修复 `_syncViewerWindow` 函数中的文件数据构建逻辑
3. 为 `viewerWindow` 添加 Shader 参数绑定（如果需要在消息模式也支持 Shader）

### 文件 4: `qml/App.qml`
**修改内容：**
1. 暴露 `controlPanel` 的引用或创建信号/属性供子组件访问 Shader 参数

---

## 实施步骤

### 步骤 1：修复问题 2（查看原图按钮）
1. 修改 `MessageList.qml` 中的文件数据构建逻辑
2. 修改 `MediaViewerWindow.qml` 中的按钮文字

### 步骤 2：为 MediaViewerWindow 添加 Shader 支持
1. 在 `MediaViewerWindow.qml` 中添加 Shader 参数属性
2. 重构图片显示区域，使用 `MultiEffect` 组件

### 步骤 3：建立参数绑定
1. 修改 `App.qml` 暴露 ControlPanel 参数
2. 修改 `MainPage.qml` 为 pendingViewerWindow 添加参数绑定
3. 修改 `MessageList.qml` 为 viewerWindow 添加参数绑定（可选）

### 步骤 4：测试验证
1. 测试 Shader 模式下参数实时预览
2. 测试"查看原图/查看修改"按钮切换功能
3. 测试视频文件的 Shader 效果（如果支持）

---

## 技术说明

### MultiEffect 支持的效果
Qt Quick 的 `MultiEffect` 组件支持以下效果：
- `brightness`: 亮度调整 (-1.0 到 1.0)
- `contrast`: 对比度调整 (0.0 到 2.0)
- `saturation`: 饱和度调整 (0.0 到 2.0)
- `colorization`: 颜色化
- `blur`: 模糊效果
- `blurEnabled`: 是否启用模糊

对于 `MultiEffect` 不支持的效果（如 hue、temperature、vignette 等），需要：
1. 使用自定义 `ShaderEffect`
2. 或暂时只支持 MultiEffect 原生支持的效果

### 视频处理限制
Qt Multimedia 的 `VideoOutput` 对实时 Shader 效果支持有限：
- 可以通过 `filters` 属性添加 `VideoFilter` 
- 或使用 `ShaderEffect` 包装 `VideoOutput`
- 建议优先确保图片效果正常，视频作为后续优化

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| MultiEffect 不支持所有 Shader 参数 | 部分效果无法实时预览 | 分阶段实现，先支持核心效果 |
| 视频实时处理性能问题 | 可能导致卡顿 | 添加性能开关，允许禁用视频 Shader |
| 参数绑定复杂度 | 代码维护难度增加 | 使用统一的参数对象传递 |

---

## 预期结果

1. **Shader 模式实时预览**：在放大查看窗口中，调整控制面板的 Shader 参数能实时反映到图片显示上
2. **查看原图/查看修改按钮**：点击按钮能正确切换显示原始图片和处理后的图片
3. **按钮文字正确**：按钮显示为"查看原图"和"查看修改"
