---
alwaysApply: true
description: '项目总览 - 始终生效，定义技术栈、目录结构和核心约束'
---
# 项目总览

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

## 目录结构

```
EnhanceVision/
├── CMakeLists.txt              # 主构建文件
├── CMakePresets.json           # CMake 构建预设
├── src/                        # C++ 源码
│   ├── main.cpp                # 应用入口
│   ├── app/                    # 应用层
│   │   ├── Application.cpp     # QApplication 封装
│   │   └── MainWindow.cpp      # 主窗口（QQuickWidget 容器）
│   ├── controllers/            # 控制器层
│   │   ├── FileController.cpp      # 文件操作控制器
│   │   ├── ProcessingController.cpp # 处理流程控制器
│   │   ├── SessionController.cpp   # 会话管理控制器
│   │   └── SettingsController.cpp  # 设置控制器
│   ├── core/                   # 核心引擎
│   │   ├── ProcessingEngine.cpp    # 处理引擎调度
│   │   ├── VideoProcessor.cpp      # 视频处理器
│   │   ├── ImageProcessor.cpp      # 图像处理器
│   │   ├── FrameCache.cpp          # 帧缓存管理
│   │   ├── TaskCoordinator.cpp     # 任务协调器
│   │   ├── ResourceManager.cpp     # 资源管理器
│   │   ├── AIEngine.cpp            # NCNN AI 推理引擎
│   │   └── ModelRegistry.cpp       # AI 模型注册表
│   ├── models/                 # QML 数据模型
│   │   ├── SessionModel.cpp    # 会话列表模型
│   │   ├── FileModel.cpp       # 文件列表模型
│   │   ├── ProcessingModel.cpp # 处理状态模型
│   │   └── MessageModel.cpp    # 消息模型
│   ├── providers/              # QML 图像提供者
│   │   ├── PreviewProvider.cpp # 预览图像提供者
│   │   └── ThumbnailProvider.cpp # 缩略图提供者
│   ├── services/               # 服务层
│   │   └── ImageExportService.cpp # 图像导出服务
│   └── utils/                  # 工具类
│       ├── FileUtils.cpp       # 文件工具
│       ├── ImageUtils.cpp      # 图像工具
│       ├── SubWindowHelper.cpp # 子窗口辅助
│       └── WindowHelper.cpp    # 窗口辅助
├── include/EnhanceVision/      # C++ 头文件
│   ├── app/
│   ├── controllers/
│   ├── core/
│   ├── models/
│   ├── providers/
│   ├── services/
│   └── utils/
├── qml/                        # QML 源码
│   ├── main.qml                # QML 入口
│   ├── App.qml                 # 应用根组件
│   ├── pages/                  # 页面
│   │   ├── MainPage.qml        # 主页面
│   │   └── SettingsPage.qml    # 设置页面
│   ├── components/             # 可复用组件
│   │   ├── Sidebar.qml         # 侧边栏
│   │   ├── TitleBar.qml        # 标题栏
│   │   ├── FileList.qml        # 文件列表
│   │   ├── PreviewPane.qml     # 预览面板
│   │   ├── ControlPanel.qml    # 控制面板
│   │   ├── MediaViewer.qml     # 媒体查看器
│   │   ├── MediaViewerWindow.qml # 独立媒体窗口
│   │   ├── EmbeddedMediaViewer.qml # 内嵌媒体查看器
│   │   ├── MinimizedWindowDock.qml # 最小化停靠栏
│   │   ├── SessionList.qml     # 会话列表
│   │   ├── SessionBatchBar.qml # 会话批量操作栏
│   │   ├── MessageList.qml     # 消息列表
│   │   ├── MessageItem.qml     # 消息项
│   │   ├── Toast.qml           # 提示消息
│   │   ├── Dialog.qml          # 对话框
│   │   ├── FullShaderEffect.qml # 完整 Shader 效果
│   │   ├── OffscreenShaderRenderer.qml # 离屏渲染器
│   │   ├── ShaderParamsPanel.qml # Shader 参数面板
│   │   ├── AIModelPanel.qml    # AI 模型面板
│   │   └── AIParamsPanel.qml   # AI 参数面板
│   ├── controls/               # 自定义控件
│   │   ├── IconButton.qml      # 图标按钮
│   │   ├── Button.qml          # 按钮
│   │   ├── Slider.qml          # 滑块
│   │   ├── ComboBox.qml        # 下拉框
│   │   ├── TextField.qml       # 文本框
│   │   ├── Switch.qml          # 开关
│   │   ├── Tooltip.qml         # 工具提示
│   │   ├── ColoredIcon.qml     # 彩色图标
│   │   ├── ParamGroup.qml      # 参数组
│   │   └── ParamSlider.qml     # 参数滑块
│   ├── shaders/                # ShaderEffect 组件
│   │   ├── BrightnessContrast.qml
│   │   ├── SaturationHue.qml
│   │   ├── SharpenBlur.qml
│   │   ├── Exposure.qml
│   │   ├── Gamma.qml
│   │   ├── Temperature.qml
│   │   ├── Tint.qml
│   │   ├── Highlights.qml
│   │   ├── Shadows.qml
│   │   ├── Hue.qml
│   │   └── Vignette.qml
│   ├── styles/                 # 样式定义
│   │   ├── Theme.qml           # 主题单例
│   │   ├── Colors.qml          # 颜色定义
│   │   ├── Typography.qml      # 字体定义
│   │   └── qmldir              # QML 模块定义
│   └── utils/                  # QML 工具
│       ├── Helpers.js          # JS 辅助函数
│       └── NotificationManager.qml # 通知管理器
├── resources/                  # Qt 资源
│   ├── shaders/                # GLSL Shader 文件
│   │   ├── basic.vert          # 基础顶点着色器
│   │   ├── full_shader.vert    # 完整 shader 顶点着色器
│   │   ├── full_shader.frag    # 完整 shader（14种效果）
│   │   ├── brightness.frag
│   │   ├── contrast.frag
│   │   ├── saturation.frag
│   │   ├── exposure.frag
│   │   ├── gamma.frag
│   │   ├── hue.frag
│   │   ├── blur.frag
│   │   └── sharpen.frag
│   ├── icons/                  # SVG 图标（亮色主题）
│   ├── icons-dark/             # SVG 图标（暗色主题）
│   ├── models/                 # AI 模型文件
│   │   ├── RealESRGAN_x4plus.param
│   │   └── RealESRGAN_x4plus.bin
│   └── i18n/                   # 翻译文件
│       ├── app_zh_CN.ts
│       └── app_en_US.ts
├── tests/                      # 测试
│   ├── cpp/                    # C++ 单元测试
│   └── qml/                    # QML 测试
├── third_party/                # 第三方库
│   ├── ncnn/                   # NCNN (add_subdirectory)
│   └── ffmpeg/                 # FFmpeg 预编译库
├── docs/                       # 项目文档
│   └── Notes/                  # 开发笔记
├── build/                      # 构建产物 (gitignore)
└── logs/                       # 日志 (gitignore)
```

## 核心约束

### 架构约束
- **QML 负责 UI**：所有界面元素、动画、交互逻辑在 QML 中实现
- **C++ 负责业务**：文件 IO、AI 推理、图像处理、视频处理在 C++ 中实现
- **数据绑定优先**：C++ 通过 Q_PROPERTY / QAbstractListModel 暴露数据，QML 通过绑定自动更新

### 性能约束
- **UI 线程不阻塞**：所有耗时操作（AI 推理、视频处理）在工作线程执行
- **图像传输零拷贝**：通过 QQuickImageProvider 直接访问 C++ 图像数据
- **Shader 实时处理**：图像滤镜使用 ShaderEffect，GPU 直接渲染

### 开发约束
- **编译器**：Windows 使用 MSVC 2022 (VS 17)，CMake Generator 为 `Visual Studio 17 2022`
- **Qt 安装路径**：`E:\Qt\6.10.2\msvc2022_64`
- **C++ 标准**：C++20，使用智能指针、RAII、结构化绑定
- **QML 版本**：Qt Quick 2.x，使用 Qt Quick Controls 2

### 文件约束
- **QML 文件**：使用 `.qml` 后缀，文件名 PascalCase
- **C++ 文件**：`.cpp` / `.h`，文件名与类名一致
- **资源文件**：通过 `.qrc` 管理，使用 `qrc://` 路径访问

## 分层架构概览

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
│                           ↕ 直接调用                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              C++ 核心引擎层                                │  │
│  │  · AI 推理 (NCNN + Vulkan)                                │  │
│  │  · 图像处理 (Qt RHI Shader)                               │  │
│  │  · 视频处理 (FFmpeg + Qt Multimedia)                      │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```
