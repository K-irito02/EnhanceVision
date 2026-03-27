---
alwaysApply: true
description: '项目总览 - 始终生效，仅维护项目事实基线（技术栈/目录）'
trigger: always_on
---
# 项目总览

## 技术栈

| 层级 | 技术 | 版本 |
|------|------|------|
| UI框架 | Qt Quick (QML) | 6.10.2 |
| 编程语言 | C++20 / QML / JavaScript | MSVC 2022 |
| AI推理 | NCNN (Vulkan加速) | latest |
| 多媒体 | FFmpeg + Qt Multimedia | 7.1 |
| 构建系统 | CMake | 3.20+ |
| 图形API | Vulkan / D3D11 / Metal | Qt RHI |

## 关键路径

- Qt：`E:/Qt/6.10.2/msvc2022_64`
- Vulkan SDK：`E:/Vulkan/VulkanSDK`
- 第三方库：`third_party/ncnn`、`third_party/ffmpeg`、`third_party/opencv`（可选）

## 目录结构

```
EnhanceVision/
├── src/                    # C++ 源码
│   ├── app/               # 应用层（Application、MainWindow）
│   ├── controllers/       # 控制器层（业务编排）
│   ├── core/              # 核心引擎（AI、图像、视频、并发调度）
│   ├── models/            # QML 数据模型
│   ├── providers/         # QML 图像提供者
│   ├── services/          # 服务层（跨模块复用）
│   └── utils/             # 工具类
├── include/EnhanceVision/ # C++ 头文件
├── qml/                   # QML 源码
│   ├── pages/            # 页面
│   ├── components/       # 可复用组件
│   ├── controls/         # 自定义控件
│   ├── shaders/          # ShaderEffect 封装
│   ├── styles/           # 样式定义
│   └── utils/            # QML 工具
├── resources/            # Qt 资源
│   ├── shaders/          # GLSL Shader
│   ├── icons/            # SVG 图标（亮色）
│   ├── icons-dark/       # SVG 图标（暗色）
│   ├── models/           # AI 模型
│   └── i18n/             # 翻译文件
├── tests/                # 测试
├── docs/                 # 文档与开发笔记
└── third_party/          # 第三方库
```

## 本文件边界

- 仅维护"项目事实信息"
- 架构原则见 `02-architecture.md`
- 代码规范见 `03`、`04`
- 领域规范见 `05`~`10`
