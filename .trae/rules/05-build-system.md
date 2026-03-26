---
alwaysApply: false
description: '构建系统 - CMake预设、构建流程、发布部署与构建可复现性'
---
# 构建系统

## 构建基线

- 使用 `CMakePresets.json` 统一配置
- Windows 生成器固定：`Visual Studio 17 2022` + `x64`
- C++ 标准：`C++20`

## 日常流程

1. 配置：`cmake --preset windows-msvc-2022-release`
2. 编译：`cmake --build build/msvc2022/Release --config Release -j 8`
3. 必要时 Debug：对应 debug preset

## 构建原则

- 优先增量构建，避免无故清理 `build/`
- 变更 CMake 后必须重新配置
- 构建失败先修复根因，不绕过警告/错误

## 部署原则

- 发布前必须完成可运行验证
- Qt 运行时依赖通过 `windeployqt` 部署
- 打包路径、参数、步骤可复现

## 本文件边界

- 仅定义“构建与部署”
- 第三方依赖治理见 `10-skills-and-tools.mdc` 的流程约定与项目文档
