# EnhanceVision

EnhanceVision 是一个基于 **Qt 6.10.2 + QML** 的桌面端图像与视频画质增强工具。采用原生 Qt Quick 架构，结合 GPU Shader 处理与 NCNN AI 推理引擎，提供兼顾性能与质量的画质增强方案。

## 核心特性

- **双模式处理**：
  - **Shader 模式**：基于 Qt RHI / GLSL Shader，实时调整曝光、亮度、对比度、饱和度、色相、伽马、色温、色调、高光、阴影、晕影、模糊、降噪、锐化等 14 种效果
  - **AI 推理模式**：基于 NCNN 引擎（Vulkan 加速），集成 Real-ESRGAN 等模型实现超分辨率增强
  - GPU OOM 自动恢复：检测显存不足时自动降级分块重试
  - 智能图像质量：动态padding + 镜像填充，消除边界伪影
  - **算法一致性**：CPU 视频导出与 GPU 预览使用相同算法，确保效果 100% 一致
- **并发调度系统**：
  - 多级优先级任务队列（Critical/High/Normal/Low）
  - 动态优先级调整，防止饥饿问题
  - 死锁检测与自动恢复机制
  - 任务超时监控与重试策略
  - AI引擎池支持2个并发推理任务
  - 实时并发监控与异常检测
- **GPU 视频导出**：视频缩略图使用 GPU Shader 处理，确保与播放效果完全一致
- **现代化 UI**：Qt Quick (QML) 声明式 UI，深色/浅色主题，中英双语
- **会话式工作流**：多会话管理，支持固定、重排序、批量文件处理
- **媒体预览**：内嵌式媒体查看器（EmbeddedMediaViewer），支持全屏覆盖、拖拽脱离、智能吸附、原图对比
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
│  │  · Controllers (QObject + Q_PROPERTY)                     │  │
│  │  · Models (QAbstractListModel)                            │  │
│  │  · Providers (QQuickImageProvider)                        │  │
│  │  · Services (业务服务)                                     │  │
│  └───────────────────────────────────────────────────────────┘  │
│                           ↕ 并发调度系统                          │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              并发调度系统                                   │  │
│  │  · PriorityTaskQueue (多级优先级队列)                      │  │
│  │  · DeadlockDetector (死锁检测)                            │  │
│  │  · TaskTimeoutWatchdog (超时监控)                         │  │
│  │  · AIEnginePool (AI引擎池)                                │  │
│  │  · ConcurrencyManager (统一管理)                          │  │
│  └───────────────────────────────────────────────────────────┘  │
│                           ↕ 任务调度                             │
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
- **效果一致性保证**：视频缩略图和播放使用相同的 GPU Shader 管线

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
│   ├── controllers/            # 控制器层
│   ├── core/                   # 核心引擎
│   ├── models/                 # QML 数据模型
│   ├── providers/              # QML 图像提供者
│   ├── services/               # 服务层
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
│   ├── icons/                  # SVG 图标（亮色主题）
│   ├── icons-dark/             # SVG 图标（暗色主题）
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
.\build\msvc2022\Debug\Debug\EnhanceVision.exe
# 或 Release 版本
.\build\msvc2022\Release\Release\EnhanceVision.exe
```

## 使用指南

### 基本操作

1. **添加文件**：点击左侧"添加文件"按钮或拖拽文件到列表
2. **选择处理模式**：
   - **Shader 模式**：实时调整图像参数
   - **AI 模式**：选择 AI 模型进行超分辨率增强
3. **调整参数**：在右侧控制面板调整各项参数
4. **预览效果**：实时预览处理效果
5. **导出结果**：点击"导出"按钮保存处理结果

### 支持的文件格式

| 类型 | 格式 |
|------|------|
| 图像 | JPG, PNG, BMP, WebP |
| 视频 | MP4, MKV, AVI |

### Shader 效果参数

| 效果 | 参数范围 | 说明 |
|------|----------|------|
| 亮度 | -1.0 ~ 1.0 | 调整图像明暗 |
| 对比度 | 0.0 ~ 2.0 | 调整明暗对比 |
| 饱和度 | 0.0 ~ 2.0 | 调整色彩饱和度 |
| 色相 | -180 ~ 180 | 调整色相偏移 |
| 曝光 | -2.0 ~ 2.0 | 调整曝光补偿 |
| 伽马 | 0.1 ~ 3.0 | 调整伽马曲线 |
| 色温 | -1.0 ~ 1.0 | 调整色温（冷/暖） |
| 色调 | -1.0 ~ 1.0 | 调整色调（绿/品红） |
| 高光 | -1.0 ~ 1.0 | 调整高光区域 |
| 阴影 | -1.0 ~ 1.0 | 调整阴影区域 |
| 锐化 | 0.0 ~ 2.0 | 增强边缘锐度 |
| 模糊 | 0.0 ~ 10.0 | 高斯模糊强度 |
| 暗角 | 0.0 ~ 1.0 | 添加暗角效果 |

### AI 模型

| 模型 | 缩放倍数 | 说明 |
|------|----------|------|
| RealESRGAN_x4plus | 4x | 通用超分辨率模型 |

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
| `08-ui-design.md` | UI 设计规范 |
| `09-shader-standards.md` | GLSL Shader 规范 |
| `10-skills-and-mcp.md` | 技能和 MCP 服务指南 |

## 贡献指南

### 开发流程

1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/your-feature`)
3. 提交更改 (`git commit -m 'feat(scope): add some feature'`)
4. 推送到分支 (`git push origin feature/your-feature`)
5. 创建 Pull Request

### 提交规范

遵循语义化提交格式：`<type>(scope): <subject>`

| type | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `refactor` | 重构 |
| `perf` | 性能优化 |
| `style` | 代码格式 |
| `docs` | 文档更新 |
| `test` | 测试相关 |
| `chore` | 构建/工具 |

### 代码规范

- C++ 遵循 `.clang-format` 配置
- QML 遵循 `.trae/rules/04-qml-standards.md`
- 所有用户可见字符串使用 `qsTr()` 包裹

## 许可证

[待定]

## 联系方式

如有问题或建议，请提交 Issue。
