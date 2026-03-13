# EnhanceVision

EnhanceVision 是一个基于 **Qt 6.10.2 + QML** 的桌面端图像与视频画质增强工具。采用原生 Qt Quick 架构，结合 GPU Shader 处理与 NCNN AI 推理引擎，提供兼顾性能与质量的画质增强方案。

## 核心特性

- **双模式处理**：
  - **Shader 模式**：基于 Qt RHI / GLSL Shader，实时调整亮度、对比度、饱和度、色相、模糊、锐化
  - **AI 推理模式**：基于 NCNN 引擎（Vulkan 加速），集成 Real-ESRGAN 等模型实现超分辨率增强
- **现代化 UI**：Qt Quick (QML) 声明式 UI，深色/浅色主题，中英双语
- **会话式工作流**：多会话管理，支持固定、重排序、批量文件处理
- **媒体预览**：内置图片/视频查看器，支持全屏、拖拽、键盘导航、原图对比
- **完全离线**：所有处理在本地完成，无需网络连接
- **高性能**：零拷贝图像传输、GPU 加速渲染、启动快（<1秒）、内存占用低（~100MB）

## 架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                    EnhanceVision.exe (单个进程)                  │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              Qt Quick (QML) UI 层                          │  │
│  │  · 声明式 UI 定义                                          │  │
│  │  · 属性绑定与动画                                          │  │
│  │  · ShaderEffect 实时滤镜                                   │  │
│  └───────────────────────────────────────────────────────────┘  │
│                           ↕ Q_PROPERTY / 信号槽                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              C++ 业务层                                    │  │
│  │  · Models (QAbstractListModel)                            │  │
│  │  · Providers (QQuickImageProvider)                        │  │
│  │  · Controllers (QObject + Q_PROPERTY)                     │  │
│  └───────────────────────────────────────────────────────────┘  │
│                           ↕ 直接调用                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              C++ 核心引擎层                                │  │
│  │  · AI 推理 (NCNN + Vulkan)                                │  │
│  │  · 图像处理 (Qt RHI Shader)                               │  │
│  │  · 视频处理 (FFmpeg + Qt Multimedia)                      │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  所有处理都在本地完成，无需网络连接                              │
│  QML 与 C++ 通过属性绑定和信号槽通信，零序列化开销               │
└─────────────────────────────────────────────────────────────────┘
```

**关键优势**：
- **零拷贝图像传输**：通过 QQuickImageProvider 直接访问 C++ 图像数据
- **GPU 加速渲染**：Qt Scene Graph 自动使用 GPU 渲染
- **实时 Shader 滤镜**：ShaderEffect 直接在 GPU 上处理图像

## 技术栈

| 层级 | 技术 | 版本 |
|------|------|------|
| **UI 框架** | Qt Quick (QML) | Qt 6.10.2 |
| **图形渲染** | Qt Scene Graph + RHI | Vulkan/D3D11/Metal |
| **应用框架** | Qt Widgets (主窗口容器) | Qt 6.10.2 |
| **编程语言** | C++20 / QML | MSVC 2022 |
| **AI 推理** | NCNN (Vulkan 加速) | latest |
| **多媒体** | FFmpeg 7.1 + Qt Multimedia | 预编译库 |
| **国际化** | Qt Linguist | Qt 6.10.2 |
| **构建系统** | CMake | 3.20+ |

## 项目结构

```
EnhanceVision/
├── CMakeLists.txt              # 主构建文件
├── CMakePresets.json           # CMake 构建预设
├── src/                        # C++ 源码
│   ├── main.cpp                # 应用入口
│   ├── app/                    # 应用层
│   ├── core/                   # 核心引擎
│   ├── models/                 # QML 数据模型
│   ├── providers/              # QML 图像提供者
│   └── utils/                  # 工具类
├── include/EnhanceVision/      # C++ 头文件
├── qml/                        # QML 源码
│   ├── main.qml                # QML 入口
│   ├── pages/                  # 页面
│   ├── components/             # 可复用组件
│   ├── controls/               # 自定义控件
│   ├── shaders/                # ShaderEffect 组件
│   └── styles/                 # 样式定义
├── resources/                  # Qt 资源
│   ├── shaders/                # GLSL Shader 文件
│   ├── icons/                  # SVG 图标
│   ├── models/                 # AI 模型文件
│   └── i18n/                   # 翻译文件
├── tests/                      # 测试
├── third_party/                # 第三方库
│   ├── ncnn/                   # NCNN
│   └── ffmpeg/                 # FFmpeg 预编译库
└── .trae/rules/                # 开发规范文档
```

## 快速开始

### 环境依赖

- **Qt 6.10.2**（包含 Quick、QuickControls2、Multimedia、ShaderTools、LinguistTools 模块）
- **CMake 3.20+**
- **MSVC 2022** (Visual Studio 17)
- 第三方库（`third_party/`）：ncnn、ffmpeg

### 构建

```powershell
# Debug 构建（开发调试）
cmake --preset windows-msvc-2022-debug
cmake --build --preset windows-msvc-2022-debug

# Release 构建（性能测试，速度提升 2-10x）
cmake --preset windows-msvc-2022-release
cmake --build --preset windows-msvc-2022-release
```

### 运行

```powershell
.\build\msvc2022\Debug\EnhanceVision.exe
# 或 Release 版本
.\build\msvc2022\Release\EnhanceVision.exe
```

## 开发规范

详见 `.trae/rules/` 目录：

| 文件 | 内容 |
|------|------|
| `01-project-overview.md` | 技术栈与目录结构 |
| `02-architecture.md` | QML + C++ 分层架构 |
| `03-cpp-standards.md` | C++ 代码规范 |
| `04-qml-standards.md` | QML 代码规范 |
| `05-build-system.md` | CMake 构建系统 |
| `06-i18n.md` | 国际化与翻译 |
| `07-performance.md` | 性能优化规则 |
| `08-ui-design.md` | UI 设计规范（主题、颜色、字体） |
| `09-shader-standards.md` | GLSL Shader 规范 |

## 许可证

[待定]
# EnhanceVision
