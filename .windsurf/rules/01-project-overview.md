---
alwaysApply: true
description: '项目总览 - 始终生效，仅维护项目事实基线（技术栈/目录）'
trigger: always_on
---
# 项目总览（事实基线）

## 技术栈

- UI：Qt Quick (QML)
- 语言：C++20 / QML / JavaScript
- 构建：CMake + CMake Presets
- 推理：NCNN（Vulkan）
- 多媒体：FFmpeg + Qt Multimedia
- 主要平台：Windows + MSVC 2022

## 关键版本与路径

- Qt：`6.10.2`
- CMake：`3.20+`
- Generator：`Visual Studio 17 2022`（x64）
- Qt 路径：`E:/Qt/6.10.2/msvc2022_64`

## 目录结构（稳定约定）

- `src/`：C++ 源码（app/controllers/core/models/providers/services/utils）
- `include/EnhanceVision/`：C++ 头文件
- `qml/`：QML 页面、组件、控件、样式、Shader 封装
- `resources/`：icons/shaders/i18n 等资源
- `tests/`：测试
- `docs/`：文档与开发笔记

## 本文件边界

- 仅维护“项目事实信息”
- 架构原则见 `02-architecture.mdc`
- 代码规范见 `03/04`
- 领域规范见 `05~10`
