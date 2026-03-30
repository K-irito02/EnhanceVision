---
alwaysApply: false
globs: ['**/resources/shaders/**', '**/qml/shaders/**', '**/*.vert', '**/*.frag', '**/*.qsb']
description: 'GLSL Shader规范 - 文件组织、参数一致性、编译集成'
trigger: glob
---
# GLSL Shader 规范

## 文件组织

| 目录 | 内容 |
|------|------|
| `resources/shaders/` | Shader 源文件（.vert/.frag） |
| `qml/shaders/` | QML 封装组件 |

## Shader 效果列表

| 文件 | 效果 |
|------|------|
| `BrightnessContrast.qml` | 亮度/对比度 |
| `SaturationHue.qml` | 饱和度/色相 |
| `SharpenBlur.qml` | 锐化/模糊 |
| `Exposure.qml` | 曝光 |
| `Gamma.qml` | 伽马 |
| `Temperature.qml` | 色温 |
| `Tint.qml` | 色调 |
| `Highlights.qml` | 高光 |
| `Shadows.qml` | 阴影 |
| `Vignette.qml` | 暗角 |

## 参数与处理顺序一致性

1. **参数范围、默认值、步长在 UI 与 Shader 端一致**
2. **预览与导出使用同一参数来源**
3. **CPU 实现与 GPU Shader 算法完全一致**
4. **CPU参数处理顺序与Shader处理顺序一致**

## 编译集成

1. **通过 `qt_add_shaders()` 纳入构建**
2. **新增 Shader 同步更新 CMake 与引用路径**

## 实现约束

1. **减少重复采样与分支**
2. **计算链路保持可读可维护**
3. **新效果需做视觉一致性回归**
