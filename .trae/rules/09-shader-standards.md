---
alwaysApply: false
globs: ['**/resources/shaders/**', '**/qml/shaders/**', '**/*.vert', '**/*.frag', '**/*.qsb']
description: 'GLSL Shader规范 - 文件组织、参数一致性、编译集成、质量约束'
trigger: glob
---
# GLSL Shader 规范

## 文件组织

| 目录 | 内容 |
|------|------|
| `resources/shaders/` | Shader 源文件（.vert/.frag） |
| `qml/shaders/` | QML 封装组件 |

## Shader 效果

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
| `Hue.qml` | 色相偏移 |

## 参数一致性

- ✅ 参数范围、默认值、步长在 UI 与 Shader 端一致
- ✅ 预览与导出必须使用同一参数来源
- ✅ CPU 实现必须与 GPU Shader 算法完全一致

## 参数处理顺序

```
1. Exposure → 2. Brightness → 3. Contrast → 4. Saturation → 5. Hue
→ 6. Gamma → 7. Temperature → 8. Tint → 9. Highlights → 10. Shadows
→ 11. Vignette → 12. Blur → 13. Denoise → 14. Sharpness
```

## 编译集成

- 通过 `qt_add_shaders(...)` 统一纳入构建
- 新增/重命名 Shader 时同步更新 CMake 与引用路径

## 实现约束

- ✅ 优先减少重复采样与分支
- ✅ 计算链路保持可读与可维护
- ✅ 新效果需做视觉一致性回归

## 本文件边界

- 仅定义 Shader 相关规范
- UI 使用方式见 `04-qml-standards.md`
