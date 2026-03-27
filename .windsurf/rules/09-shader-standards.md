---
alwaysApply: false
globs: ['**/resources/shaders/**', '**/qml/shaders/**', '**/*.vert', '**/*.frag', '**/*.qsb']
description: 'GLSL Shader规范 - 文件组织、参数一致性、编译集成、质量约束'
trigger: glob
---
# GLSL Shader 规范

## 文件组织

- Shader 源文件统一放 `resources/shaders/`
- QML 封装统一放 `qml/shaders/`
- 命名需与效果语义一致，避免同义重复文件

## 参数一致性

- 参数范围、默认值、步长在 UI 与 Shader 端一致
- 禁止使用会吞掉 `0` 的默认值逻辑
- 预览与导出必须使用同一参数来源
- **CPU 实现必须与 GPU Shader 算法完全一致**
- **参数处理顺序必须严格遵循 GPU Shader 顺序**：
  ```
  1. Exposure → 2. Brightness → 3. Contrast → 4. Saturation → 5. Hue
  → 6. Gamma → 7. Temperature → 8. Tint → 9. Highlights → 10. Shadows
  → 11. Vignette → 12. Blur → 13. Denoise → 14. Sharpness
  ```

## 编译集成

- 通过 `qt_add_shaders(...)` 统一纳入构建
- 新增/重命名 Shader 时同步更新 CMake 与引用路径
- 路径错误或编译失败必须可定位

## 实现约束

- 优先减少重复采样与分支
- 计算链路保持可读与可维护
- 新效果需做视觉一致性回归

## 本文件边界

- 仅定义 Shader 相关规范
- UI 使用方式见 `04-qml-standards.mdc`
