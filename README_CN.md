# EnhanceVision

[English](README.md) | 简体中文

基于 **Qt 6.10.2 + QML** 的桌面端图像处理与AI推理画质增强工具。采用原生 Qt Quick 架构，结合 GPU Shader 处理与 NCNN AI 推理引擎，提供兼顾性能与质量的画质增强方案。

## 核心特性

### 双模式处理

| 模式 | 技术 | 效果 |
|------|------|------|
| **Shader 模式** | Qt RHI / GLSL | 14 种实时参数调整（曝光、亮度、对比度、饱和度、色相、伽马、色温、色调、高光、阴影、晕影、模糊、降噪、锐化） |
| **AI 推理模式** | NCNN + Vulkan | Real-ESRGAN 超分辨率增强（4x） |

### 任务管理

- 任务状态跟踪与协调
- 死锁检测与自动恢复机制
- 任务超时监控与重试策略
- **增强的Vulkan同步机制**：确保GPU操作完全同步，防止崩溃
- 启动恢复机制：重启后可恢复未完成任务或统一标记失败（见 `docs/Plan/任务控制模式详解.md`）

### 性能与体验

- **零拷贝图像传输**：QQuickImageProvider 直接访问 C++ 图像数据
- **GPU 加速渲染**：Qt Scene Graph 自动使用 GPU 渲染
- **算法一致性**：CPU 视频导出与 GPU 预览使用相同算法
- **GPU OOM 自动恢复**：检测显存不足时自动降级分块重试
- **稳定性优化**：通过增加延迟换取完全的推理稳定性
- 主窗口改为原生 `QQuickWindow` 承载，降低 Windows 下无边框缩放闪动和黑缝问题
- Windows 关闭链路已做限时收敛与兜底，点击主窗口 `X` 后不会再残留后台进程

### 现代化 UI

- Qt Quick (QML) 声明式 UI
- 深色/浅色主题
- 中英双语
- 主窗口几何状态通过统一的 UI 状态持久化在重启后恢复
- 会话式工作流，支持固定、重排序、批量文件处理
- 内嵌式媒体查看器，支持全屏、拖拽脱离、智能吸附
- 媒体查看器共享内核（`qml/components/mediaViewer/`）：统一画布、控制栏、缩略图适配与上下文菜单
- 查看器缩略图与消息卡片在文件陆续完成、删除与清理时保持实时同步
- 缓存清理结果会明确展示残留文件与后续处理建议，避免模糊失败提示
- 消息卡片运行状态统一由 C++ 派生，呼吸蓝框仅在真实处理中触发，避免暂停/完成后的动画残留
- Theme SVG 图标统一通过 `Theme.icon()` 和 `ColoredIcon` 管理，位图和品牌 Logo 继续使用 `Image`
- 运行日志默认只保留 warning/error/fatal，常规信息与调试噪声已从启动和关闭路径收敛
- 首次启动在 `settings.ini` 尚未写入语言时，会自动继承安装器所选语言
- Windows 下即使从安装器完成页以提权状态启动，拖拽导入也保持可用

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
│   │   ├── ai/           # AI 推理服务
│   │   ├── inference/    # 推理后端
│   │   └── video/        # 视频处理
│   ├── models/            # QML 数据模型
│   ├── providers/         # QML 图像提供者
│   ├── services/          # 服务层
│   └── utils/             # 工具类
├── include/EnhanceVision/ # C++ 头文件
├── qml/                   # QML 源码
│   ├── pages/            # 页面
│   ├── components/       # 可复用组件
│   │   └── mediaViewer/  # 媒体查看器共享内核组件
│   ├── controls/         # 自定义控件
│   ├── shaders/          # ShaderEffect 封装
│   ├── stores/           # 状态存储
│   ├── utils/            # QML 工具与单例
│   └── styles/           # 样式定义
├── resources/            # Qt 资源
│   ├── shaders/          # GLSL Shader
│   ├── icons/            # SVG 图标
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

## 下载安装

### Windows

| 版本 | 文件 | 说明 |
|------|------|------|
| v0.1.0 | [安装程序](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-installer.exe) | 标准安装，支持卸载 |
| v0.1.0 | [便携版](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-portable.zip) | 绿色版，解压即用 |

### 系统要求

- Windows 10/11 64-bit
- Vulkan 兼容 GPU（可选，用于 AI 加速）
- 4GB+ RAM
- 500MB+ 磁盘空间

### 安装说明

#### 安装版
1. 下载安装程序
2. 双击运行安装程序
3. 按向导完成安装
4. 从开始菜单启动 EnhanceVision

安装说明补充：
- 安装器会在同一步同时配置安装目录和默认导出路径
- 应用运行时数据统一写入 `安装目录\data`
- 升级安装时可选择继续使用旧数据目录、迁移到新目录，或删除旧数据后全新开始
- 升级后的首次启动会按安装器维护结果选择真实生效数据目录，缓存管理与会话内容会随之正确显示
- 若安装目录位于 `Program Files` 等受保护目录，安装器会提示风险但不强制修改
- 首次启动时若本地尚无语言设置，界面语言会跟随安装器所选语言
- 打包流程会为安装包和便携包额外生成 `*.sha256` 校验文件，分发前建议先校验

#### 便携版
1. 下载 ZIP 文件
2. 解压到任意目录
3. 双击 `EnhanceVision.exe` 启动

## 许可证

本项目采用 [MIT License](LICENSE) 开源许可证。

### 第三方依赖

本项目使用以下第三方开源库：

| 依赖 | 许可证 |
|------|--------|
| Qt 6 | LGPL v3 |
| NCNN | BSD-3-Clause |
| FFmpeg | LGPL v2.1+ |
| Real-ESRGAN | BSD-3-Clause |

详见 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。

## 贡献

欢迎贡献代码！请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 了解如何参与开发。

## 安全

如果您发现安全漏洞，请参阅 [SECURITY.md](SECURITY.md) 了解如何报告。

## 联系方式

- **项目主页**: [https://www.xiaogans.online/](https://www.xiaogans.online/)
- **备用地址**: [https://xiaogans.online/](https://xiaogans.online/)
- **邮箱**: saokiiritoasuna@qq.com
