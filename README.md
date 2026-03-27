# EnhanceVision

基于 **Qt 6.10.2 + QML** 的桌面端图像与视频画质增强工具。采用原生 Qt Quick 架构，结合 GPU Shader 处理与 NCNN AI 推理引擎，提供兼顾性能与质量的画质增强方案。

## 核心特性

### 双模式处理

| 模式 | 技术 | 效果 |
|------|------|------|
| **Shader 模式** | Qt RHI / GLSL | 14 种实时参数调整（曝光、亮度、对比度、饱和度、色相、伽马、色温、色调、高光、阴影、晕影、模糊、降噪、锐化） |
| **AI 推理模式** | NCNN + Vulkan | Real-ESRGAN 超分辨率增强（4x） |

### 并发调度系统

- 多级优先级任务队列（Critical/High/Normal/Low）
- AI 引擎池支持 2 个并发推理任务
- 死锁检测与自动恢复机制
- 任务超时监控与重试策略

### 性能与体验

- **零拷贝图像传输**：QQuickImageProvider 直接访问 C++ 图像数据
- **GPU 加速渲染**：Qt Scene Graph 自动使用 GPU 渲染
- **算法一致性**：CPU 视频导出与 GPU 预览使用相同算法
- **GPU OOM 自动恢复**：检测显存不足时自动降级分块重试

### 现代化 UI

- Qt Quick (QML) 声明式 UI
- 深色/浅色主题
- 中英双语
- 会话式工作流，支持固定、重排序、批量文件处理
- 内嵌式媒体查看器，支持全屏、拖拽脱离、智能吸附

## 技术栈

| 层级 | 技术 | 版本 |
|------|------|------|
| UI 框架 | Qt Quick (QML) | 6.10.2 |
| 图形渲染 | Qt Scene Graph + RHI | Vulkan/D3D11/Metal |
| 应用框架 | Qt Widgets | 6.10.2 |
| 编程语言 | C++20 / QML | MSVC 2022 |
| AI 推理 | NCNN (Vulkan 加速) | latest |
| 多媒体 | FFmpeg + Qt Multimedia | 7.1 |
| 国际化 | Qt Linguist | 6.10.2 |
| 构建系统 | CMake | 3.20+ |

## 项目结构

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

## 快速开始

### 环境依赖

- **Qt 6.10.2**（Quick、QuickControls2、Multimedia、ShaderTools、LinguistTools）
- **CMake 3.20+**
- **MSVC 2022** (Visual Studio 17)
- **Vulkan SDK**
- 第三方库：ncnn、ffmpeg、opencv（可选）

### 构建

```powershell
# Debug 构建
cmake --preset windows-msvc-2022-debug
cmake --build build/msvc2022/Debug --config Debug -j 8

# Release 构建
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### 运行

```powershell
.\build\msvc2022\Debug\Debug\EnhanceVision.exe
```

## 使用指南

### 基本操作

1. **添加文件**：点击左侧"添加文件"按钮或拖拽文件
2. **选择模式**：Shader 模式或 AI 模式
3. **调整参数**：在右侧控制面板调整
4. **预览效果**：实时预览处理效果
5. **导出结果**：点击"导出"按钮保存

### 支持格式

| 类型 | 格式 |
|------|------|
| 图像 | JPG, PNG, BMP, WebP |
| 视频 | MP4, MKV, AVI |

### Shader 参数

| 效果 | 范围 | 说明 |
|------|------|------|
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
| `10-skills-and-tools.md` | 技能和工具指南 |

## 许可证

[待定]
