---
alwaysApply: false
description: '项目总览 - 技术栈、目录结构、关键路径'
---
# 项目总览

## 技术栈

| 层级 | 技术 | 版本 |
|------|------|------|
| UI框架 | Qt Quick (QML) | 6.10.2 |
| 编程语言 | C++20 / QML / JavaScript | MSVC 2022 |
| 数据库 | SQLite (Qt6::Sql) | 3.x |
| AI推理 | NCNN (Vulkan加速) | latest |
| 多媒体 | FFmpeg + Qt Multimedia | 7.1 |
| 构建系统 | CMake | 3.20+ |
| 图形API | Vulkan / D3D11 / Metal | Qt RHI |

## 关键路径

| 资源 | 路径 |
|------|------|
| Qt | `E:/Qt/6.10.2/msvc2022_64` |
| Vulkan SDK | `E:/Vulkan/VulkanSDK` |
| NCNN | `third_party/ncnn` |
| FFmpeg | `third_party/ffmpeg` |

## 目录结构

```
EnhanceVision/
├── src/                    # C++ 源码
│   ├── app/               # 应用层
│   ├── controllers/       # 控制器层
│   ├── core/              # 核心引擎
│   ├── models/            # QML 数据模型
│   ├── providers/         # QML 图像提供者
│   ├── services/          # 服务层
│   └── utils/             # 工具类
├── include/EnhanceVision/ # C++ 头文件
├── qml/                   # QML 源码
│   ├── pages/            # 页面
│   ├── components/       # 可复用组件
│   ├── controls/         # 自定义控件
│   ├── shaders/          # ShaderEffect 封装
│   └── styles/           # 样式定义
├── resources/            # Qt 资源
│   ├── shaders/          # GLSL Shader
│   ├── icons/            # SVG 图标
│   ├── models/           # AI 模型
│   └── i18n/             # 翻译文件
├── tests/                # 测试
├── docs/                 # 文档
└── third_party/          # 第三方库
```
