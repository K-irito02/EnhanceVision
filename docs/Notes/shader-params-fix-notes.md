# Shader 模式参数修复与优化

## 概述

修复 Shader 模式下参数不生效、缩略图显示原图、放大查看图像全黑等问题。

**创建日期**: 2026-03-18
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `resources/shaders/fullshader.frag` | 调整执行顺序、优化效果系数、增强降噪 |
| `qml/components/ShaderParamsPanel.qml` | 更新参数范围配置 |
| `qml/components/MessageList.qml` | 修复缩略图数据传递、添加 shaderAlreadyApplied 检查 |
| `qml/components/MediaViewerWindow.qml` | 修复底部缩略图显示逻辑 |
| `qml/components/MediaThumbnailStrip.qml` | 修复 undefined 警告 |
| `qml/components/FullShaderEffect.qml` | 删除调试日志 |
| `include/EnhanceVision/models/DataTypes.h` | 更新参数范围文档注释 |
| `src/models/MessageModel.cpp` | 修复 hasShaderModifications 检查、添加 shaderAlreadyApplied 字段 |
| `src/controllers/ProcessingController.cpp` | 删除调试日志 |
| `src/providers/ThumbnailProvider.cpp` | 删除调试日志 |

### 2. 实现的功能特性

- ✅ 降噪参数在放大查看窗口中生效
- ✅ 所有参数在消息预览区域生效
- ✅ 参数之间相互独立，互不影响
- ✅ 底部缩略图显示处理后的效果图
- ✅ 放大查看时图像正确显示（不再全黑）
- ✅ 参数范围专业级配置

---

## 二、遇到的问题及解决方案

### 问题 1：降噪参数不起作用

**现象**：调整降噪参数后，放大查看窗口中的图像没有变化。

**原因**：
1. Shader 执行顺序问题：锐化在降噪后执行，重新引入噪点
2. 混合系数过小：`denoise * 0.5` 使效果减半
3. 阈值过高：`denoise > 0.01` 导致小值被跳过

**解决方案**：
1. 调整执行顺序为：模糊 → 降噪 → 锐化
2. 增加混合系数到 `0.8`
3. 降低阈值到 `0.001`

### 问题 2：底部缩略图显示原图而非效果图

**现象**：放大查看时，底部缩略图显示原图而非处理后的效果图。

**原因**：
1. `MessageList._openViewer` 中构建 `files` 数组时缺少 `status` 和 `processedThumbnailId` 字段
2. `hasShaderModifications` 只检查了三个参数，没有检查所有 Shader 参数
3. `thumbnail` 字段使用原始文件路径，而不是处理后的缩略图

**解决方案**：
1. 添加 `status` 和 `processedThumbnailId` 字段到 `files` 数组
2. 修复 `hasShaderModifications` 检查所有 14 个 Shader 参数
3. 优先使用处理后的缩略图源

### 问题 3：放大查看时图像全黑

**现象**：亮度调低后，放大查看时图像全黑。

**原因**：
1. `MessageList.qml` 中有两个 `_openViewer` 函数定义
2. 第二个函数没有使用 `shaderAlreadyApplied` 字段
3. Shader 效果被重复应用（导出的图像已包含效果，又应用了一次）

**解决方案**：
1. 在 `MessageModel::getShaderParams` 中添加 `shaderAlreadyApplied` 字段
2. 在两个 `_openViewer` 函数中都添加 `shaderAlreadyApplied` 检查
3. 当 `shaderAlreadyApplied` 为 `true` 时，禁用 Shader 效果

---

## 三、技术实现细节

### Shader 执行顺序优化

```
原顺序：曝光 → 亮度 → 对比度 → 饱和度 → 色相 → 伽马 → 色温 → 色调 → 高光 → 阴影 → 晕影 → 降噪 → 模糊 → 锐化

优化后：曝光 → 亮度 → 对比度 → 饱和度 → 色相 → 伽马 → 色温 → 色调 → 高光 → 阴影 → 晕影 → 模糊 → 降噪 → 锐化
```

### 参数效果系数优化

| 参数 | 原系数 | 新系数 | 原因 |
|------|--------|--------|------|
| 降噪混合 | 0.5 | 0.8 | 效果太弱 |
| 色温偏移 | 0.1 | 0.2 | 效果不明显 |
| 色调偏移 | 0.1 | 0.2 | 效果不明显 |
| 高光调整 | 0.2 | 0.3 | 效果偏弱 |
| 阴影调整 | 0.2 | 0.3 | 效果偏弱 |
| 模糊混合 | 0.5 | 0.7 | 效果偏弱 |

### 参数范围专业级配置

| 参数 | 原范围 | 新范围 |
|------|--------|--------|
| 对比度 | 0.0 ~ 2.0 | 0.0 ~ 3.0 |
| 饱和度 | 0.0 ~ 2.0 | 0.0 ~ 3.0 |
| 曝光 | -1.0 ~ 1.0 | -2.0 ~ 2.0 |
| 伽马 | 0.5 ~ 2.0 | 0.3 ~ 3.0 |
| 色温 | -0.5 ~ 0.5 | -1.0 ~ 1.0 |
| 色调 | -0.5 ~ 0.5 | -1.0 ~ 1.0 |

### shaderAlreadyApplied 逻辑

```cpp
// MessageModel::getShaderParams
bool isShaderMode = (msg.mode == ProcessingMode::Shader);
bool hasCompletedFiles = false;
for (const MediaFile &file : msg.mediaFiles) {
    if (file.status == ProcessingStatus::Completed) {
        hasCompletedFiles = true;
        break;
    }
}
result["shaderAlreadyApplied"] = isShaderMode && hasShaderModifications && hasCompletedFiles;
```

```qml
// MessageList._openViewer
var shaderAlreadyApplied = shaderParams.shaderAlreadyApplied === true
viewerWindow.shaderEnabled = !shaderAlreadyApplied
```

---

## 四、参考资料

- [Qt ShaderEffect 文档](https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html)
- [GLSL Shader 编程指南](https://www.khronos.org/opengl/wiki/OpenGL_Shading_Language)
