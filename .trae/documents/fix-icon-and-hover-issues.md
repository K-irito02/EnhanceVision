# 问题修复计划

## 问题分析

### 问题 1: 亮色主题下图标不显示

**根本原因**: `qml.qrc` 资源文件中缺少亮色主题图标注册。

从日志中可以看到多个图标加载失败：
```
Cannot open: qrc:/icons/volume.svg
Cannot open: qrc:/icons/color-palette.svg
Cannot open: qrc:/icons/light-shadow.svg
Cannot open: qrc:/icons/detail-enhance.svg
Cannot open: qrc:/icons/adjust-basic.svg
Cannot open: qrc:/icons/eraser.svg
Cannot open: qrc:/icons/list-select.svg
```

**分析**:
- `icons/` 目录下这些图标文件存在，但未在 `qml.qrc` 中注册
- `icons-dark/` 目录下的对应图标已在 `qml.qrc` 中注册
- 需要在 `qml.qrc` 的亮色主题图标资源部分添加缺失的图标注册

### 问题 2: 导航按钮悬停效果异常和自动消失

**根本原因**: 按钮悬停状态管理和定时器逻辑存在问题。

**分析**:
1. `EmbeddedMediaViewer.qml` 中的导航按钮（prevBtn/nextBtn）opacity 依赖 `showNavButtons` 状态
2. 当鼠标悬停在按钮上时，`prevButtonHovered`/`nextButtonHovered` 应保持为 true
3. 但定时器 `navButtonHideTimer` 可能在错误时机触发
4. 需要添加调试信息来追踪状态变化

### 问题 3: 视频文件路径编码问题

**根本原因**: 含中文/特殊字符的视频文件路径编码处理问题。

**分析**:
- 日志显示 `???????????????????????????????????.mp4` 表示路径编码错误
- Image 组件尝试将视频文件作为图片加载导致错误
- 需要检查文件路径处理逻辑

## 修复步骤

### 步骤 1: 修复亮色主题图标缺失问题

**文件**: `e:\QtAudio-VideoLearning\EnhanceVision\resources\qml.qrc`

在 `<qresource prefix="/">` 亮色主题图标部分添加缺失的图标注册：
- `icons/volume.svg`
- `icons/color-palette.svg`
- `icons/light-shadow.svg`
- `icons/detail-enhance.svg`
- `icons/adjust-basic.svg`
- `icons/eraser.svg`
- `icons/list-select.svg`

### 步骤 2: 添加调试信息追踪导航按钮问题

**文件**: `e:\QtAudio-VideoLearning\EnhanceVision\qml\components\EmbeddedMediaViewer.qml`

在 `MediaContentArea` 组件中添加调试日志：
- 在 `navButtonHideTimer.onTriggered` 中添加日志
- 在 `prevBtn` 和 `nextBtn` 的 MouseArea 事件中添加日志
- 追踪 `showNavButtons`、`prevButtonHovered`、`nextButtonHovered`、`_mouseInArea` 状态

### 步骤 3: 根据调试信息修复导航按钮问题

根据调试日志输出分析问题原因并修复。

### 步骤 4: 检查视频文件路径编码问题

**文件**: `e:\QtAudio-VideoLearning\EnhanceVision\qml\components\EmbeddedMediaViewer.qml`

检查 `_getSource` 函数和视频源设置逻辑，确保正确处理含中文/特殊字符的文件路径。

## 验证方法

1. 构建并运行项目
2. 切换到亮色主题，检查批量操作按钮图标是否显示
3. 在媒体查看器中测试导航按钮悬停效果
4. 打开含中文文件名的视频文件测试

## 预期结果

1. 亮色主题下所有图标正常显示
2. 导航按钮悬停效果正常，不会自动消失
3. 含中文文件名的视频文件正常加载
