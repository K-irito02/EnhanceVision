**[中文](#中文) | [English](#english)**

---

<h2 id="中文">🎉 EnhanceVision v0.1.0 - 首次发布！</h2>

### 这是什么软件？

EnhanceVision 是一款**图像处理与 AI推理（Real-ESRGAN） 画质增强工具**。简单来说，它能调整你图片和视频的图像参数，也支持使用AI推理自动提升画质，使图片变得更清晰。

### 它能做什么？

**两种处理模式：**

1. **手动调节模式** - 像修图软件一样，你可以自己调整：
   - 亮度、对比度、饱和度
   - 曝光、色温、色调
   - 高光、阴影、锐化
   - 模糊、暗角等效果
   - 总共14种调节选项
   - **上传文件后即可实时调节参数，即时预览效果**

2. **AI智能增强模式** - 一键让图片变清晰：
   - 使用AI技术自动提升画质
   - 可以把图片放大4倍而不模糊
   - 适合处理老照片、低清图片

**拖拽上传文件：**
- 支持直接将图片/视频文件拖拽到主界面中间感知区域，无需点击上传按钮
- 支持批量上传多个文件

**会话标签功能：**
- 支持创建多个会话，每个会话独立管理文件
- 可以为会话命名、重命名
- 支持固定重要会话，防止误删
- 支持拖拽排序，调整会话顺序
- 支持批量选择会话进行操作

**三种处理顺序模式：**
- **自动处理模式**：暂停某个任务后，其他任务继续处理
- **顺序处理模式**：暂停后阻塞整个队列，适合需要中途暂停所有队列任务的场景
- **自由选择模式**：默认暂停发送的消息，后续点击继续按钮手动选择处理消息的顺序，灵活控制处理流程

**恢复功能：**
- 根据所选择的处理模式：恢复到关闭前的任务状态（正在处理的任务、等待处理的任务、被暂停的任务）

**媒体查看器：**
- **嵌入式窗口**：在主界面内直接查看图片/视频，不打断工作流
- **独立窗口**：可将媒体拖出成为独立窗口，支持多显示器
- **最小化停靠**：窗口可最小化到停靠栏，稍后继续查看
- **源/结果对比**：一键切换查看原图与处理后效果，方便对比
   - 开/切自动播放：点击视频进行放大查看（嵌入式和独立式）和点击左右导航按钮切换到视频时自动开始播放
   - 源/结自动播放：放大查看（嵌入式和独立式）切换源件/结果时自动播放
   - 源/结恢复进度：放大查看（嵌入式和独立式）切换源件/结果时恢复播放进度

**自动导出功能：**
- 通过在设置页面开启/关闭自动导出功能，自动导出处理后的文件，无需手动操作

**缓存管理功能：**
- 在设置页面缓存管理下，支持手动删除相关缓存文件，释放硬盘空间

**支持的文件格式：**
- 图片：JPG、PNG、BMP、WebP
- 视频：MP4、MKV、AVI

**其他特点：**
- 支持深色/浅色主题
- 支持中文/英文界面
- 可以批量处理多个文件
- 支持视频播放控制（播放/暂停、进度拖拽、倍速播放、音量调节）

### 和其他软件比有什么优势？

| 特点 | 说明 |
|------|------|
| 🎯 **简单易用** | 界面简洁，操作简单，上传即可预览调节效果 |
| 🔄 **灵活控制** | 三种处理模式，适应不同使用场景 |
| 🖼️ **实时对比** | 一键切换原图与处理结果，效果一目了然 |
| 🆓 **完全免费** | 开源软件，永久免费使用 |

### 系统要求

- Windows 10/11 64位系统
- 4GB以上内存
- 500MB以上硬盘空间
- （可选）支持Vulkan的显卡，用于AI加速

### 下载说明

| 文件 | 大小 | 说明 |
|------|------|------|
| `EnhanceVision-v0.1.0-windows-x64-installer.exe` | ~162.84 MB | 安装版，双击安装即可 |
| `EnhanceVision-v0.1.0-windows-x64-portable.zip` | ~206.62 MB | 便携版，解压即用 |

### 校验码

**安装版 SHA256：**
```
114D303357FD2D6706DCDC5D35635C673035F3B876594622DD7CCBF7A4194450
```

**便携版 SHA256：**
```
747CE99ED50395083C86D0F6F784660BDEB1ED58B88642EFA6EC1336EFDD7DC5
```

### 反馈问题

如果遇到问题或有建议，欢迎在 [Issues](https://github.com/K-irito02/EnhanceVision/issues) 中反馈！

---

**Chinese Website：** https://www.xiaogans.online/

---

### GUI 界面

#### 主题展示

<p align="center">
  <b>暗色主题</b><br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/暗主题.png" alt="暗色主题" width="800">
</p>

<p align="center">
  <b>亮色主题</b><br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/亮主题.png" alt="亮色主题" width="800">
</p>

#### 文件上传与处理

<p align="center">
  <b>拖拽上传</b> — 将文件直接拖入中央区域即可添加<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/拖拽添加.png" alt="拖拽上传" width="800">
</p>

<p align="center">
  <b>AI 推理模式</b> — 选择模型后一键增强，支持 CPU/GPU 双模式推理<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/AI推理模式.png" alt="AI推理模式" width="800">
</p>

<p align="center">
  <b>AI 增强效果对比</b>（左：原图 → 右：AI 增强）<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/AI推理前.png" alt="AI推理前" width="400">　<img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/AI推理后.png" alt="AI推理后" width="400">
</p>

#### 媒体查看器

<p align="center">
  <b>嵌入式窗口</b> — 在主界面内直接预览图片，支持底部缩略图导航<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/嵌入式窗口.png" alt="嵌入式窗口" width="800">
</p>

<p align="center">
  <b>独立式窗口</b> — 可将媒体拖出为独立浮窗，支持多显示器使用<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/独立式窗口.png" alt="独立式窗口" width="800">
</p>

<p align="center">
  <b>最小化停靠区域</b> — 窗口最小化到底部停靠栏，随时恢复查看<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/最小化停靠区域.png" alt="最小化停靠区域" width="800">
</p>

<p align="center">
  <b>最小化面板布局</b> — 文件以网格缩略图形式展开浏览<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/最小化面板布局.png" alt="最小化面板布局" width="800">
</p>

<p align="center">
  <b>信息展开视图</b> — 展开消息卡片详情，完整显示文件缩略图列表与处理状态<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/信息展开.png" alt="信息展开视图" width="800">
</p>

---
---

<h2 id="english">🎉 EnhanceVision v0.1.0 - First Release!</h2>

### What is this software?

EnhanceVision is an **image processing and AI-powered（Real-ESRGAN） quality enhancement tool**. Simply put, it allows you to adjust image parameters for pictures and videos, and also supports using AI inference to automatically improve image quality, making your images clearer.

### What can it do?

**Two processing modes:**

1. **Manual Adjustment Mode** - Like a photo editor, you can adjust:
   - Brightness, Contrast, Saturation
   - Exposure, Color Temperature, Tint
   - Highlights, Shadows, Sharpening
   - Blur, Vignette and more
   - 14 adjustment options in total
   - **Adjust parameters in real-time after uploading, with instant preview**

2. **AI Smart Enhancement Mode** - One-click image clarity:
   - Uses AI technology to automatically improve image quality
   - Can upscale images 4x without blurring
   - Ideal for old photos and low-resolution images

**Drag & Drop Upload:**
- Drag image/video files directly to the central sensing area of the main interface, no upload button needed
- Supports batch uploading of multiple files

**Session Tabs:**
- Create multiple sessions, each managing files independently
- Name and rename sessions
- Pin important sessions to prevent accidental deletion
- Drag to reorder sessions
- Batch select sessions for operations

**Three Processing Order Modes:**
- **Auto Processing Mode**: Other tasks continue processing when one is paused
- **Sequential Processing Mode**: Pausing blocks the entire queue, suitable for scenarios where you need to pause all queued tasks
- **Free Selection Mode**: Newly sent messages are paused by default; manually choose which to process next via the continue button for flexible workflow control

**Resume Functionality:**
- Based on the selected processing mode: restores the task state from before closing (tasks being processed, tasks waiting to be processed, paused tasks)

**Media Viewer:**
- **Embedded Window**: View images/videos directly within the main interface without interrupting your workflow
- **Detached Window**: Drag media out as a separate floating window, supports multi-monitor setups
- **Minimized Dock**: Windows can be minimized to the dock bar for later viewing
- **Source/Result Comparison**: One-click toggle between original and processed results for easy comparison
   - Auto-play on open/switch: Videos auto-start playing when enlarged (embedded/detached) or when navigating to a video via left/right buttons
   - Source/Result auto-play: Auto-play when switching between source and result in enlarged view (embedded/detached)
   - Source/Result resume progress: Resume playback progress when switching between source and result in enlarged view (embedded/detached)

**Auto Export:**
- Enable/disable auto export in the settings page to automatically export processed files without manual operation

**Cache Management:**
- Manually delete cache files in the settings page under cache management to free up disk space

**Supported File Formats:**
- Images: JPG, PNG, BMP, WebP
- Videos: MP4, MKV, AVI

**Other Features:**
- Dark/Light theme support
- Chinese/English interface
- Batch processing of multiple files
- Video playback controls (play/pause, progress seeking, playback speed, volume adjustment)

### Advantages over other software

| Feature | Description |
|---------|-------------|
| 🎯 **Easy to Use** | Clean interface, simple operation, instant preview after uploading |
| 🔄 **Flexible Control** | Three processing modes for different use cases |
| 🖼️ **Real-time Comparison** | One-click toggle between original and processed results |
| 🆓 **Completely Free** | Open source software, free forever |

### System Requirements

- Windows 10/11 64-bit
- 4GB+ RAM
- 500MB+ disk space
- (Optional) Vulkan-capable GPU for AI acceleration

### Downloads

| File | Size | Description |
|------|------|-------------|
| `EnhanceVision-v0.1.0-windows-x64-installer.exe` | ~162.84 MB | Installer, double-click to install |
| `EnhanceVision-v0.1.0-windows-x64-portable.zip` | ~206.62 MB | Portable, extract and run |

### Checksums

**Installer SHA256:**
```
114D303357FD2D6706DCDC5D35635C673035F3B876594622DD7CCBF7A4194450
```

**Portable SHA256:**
```
747CE99ED50395083C86D0F6F784660BDEB1ED58B88642EFA6EC1336EFDD7DC5
```

### Feedback

If you encounter issues or have suggestions, feel free to report them at [Issues](https://github.com/K-irito02/EnhanceVision/issues)!

---

**Chinese Website：** https://www.xiaogans.online/

---

### GUI Screenshots

#### Theme Showcase

<p align="center">
  <b>Dark Theme</b><br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/暗主题.png" alt="Dark Theme" width="800">
</p>

<p align="center">
  <b>Light Theme</b><br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/亮主题.png" alt="Light Theme" width="800">
</p>

#### File Upload & Processing

<p align="center">
  <b>Drag & Drop Upload</b> — Drag files directly into the central area to add them<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/拖拽添加.png" alt="Drag & Drop Upload" width="800">
</p>

<p align="center">
  <b>AI Inference Mode</b> — Select a model and enhance with one click, supports CPU/GPU dual-mode inference<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/AI推理模式.png" alt="AI Inference Mode" width="800">
</p>

<p align="center">
  <b>AI Enhancement Comparison</b> (Left: Original → Right: AI Enhanced)<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/AI推理前.png" alt="Before AI" width="400">　<img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/AI推理后.png" alt="After AI" width="400">
</p>

#### Media Viewer

<p align="center">
  <b>Embedded Window</b> — Preview images directly in the main interface with bottom thumbnail navigation<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/嵌入式窗口.png" alt="Embedded Window" width="800">
</p>

<p align="center">
  <b>Detached Window</b> — Drag media out as a floating window, supports multi-monitor setups<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/独立式窗口.png" alt="Detached Window" width="800">
</p>

<p align="center">
  <b>Minimized Dock Area</b> — Minimize windows to the bottom dock bar, restore anytime<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/最小化停靠区域.png" alt="Minimized Dock Area" width="800">
</p>

<p align="center">
  <b>Minimized Panel Layout</b> — Browse files in a grid thumbnail layout<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/最小化面板布局.png" alt="Minimized Panel Layout" width="800">
</p>

<p align="center">
  <b>Expanded Info View</b> — Expand message card details to show full file thumbnail list and processing status<br>
  <img src="https://raw.githubusercontent.com/K-irito02/EnhanceVision/main/docs/images/信息展开.png" alt="Expanded Info View" width="800">
</p>

---

English | [简体中文](README_CN.md)

A desktop image processing and AI inference quality enhancement tool built with **Qt 6.10.2 + QML**. Using native Qt Quick architecture with GPU Shader processing and NCNN AI inference engine, providing a balance between performance and quality.

## Core Features

### Dual-Mode Processing

| Mode | Technology | Effects |
|------|------------|---------|
| **Shader Mode** | Qt RHI / GLSL | 14 real-time parameter adjustments (exposure, brightness, contrast, saturation, hue, gamma, color temperature, tint, highlights, shadows, vignette, blur, denoise, sharpen) |
| **AI Inference Mode** | NCNN + Vulkan | Real-ESRGAN super-resolution enhancement (4x) |

### Task Management

- Task state tracking and coordination
- Deadlock detection and automatic recovery mechanism
- Task timeout monitoring and retry strategy
- **Enhanced Vulkan synchronization**: Ensures fully synchronized GPU operations to prevent crashes
- Startup recovery: Resume unfinished tasks or mark as failed after restart (see `docs/Plan/任务控制模式详解.md`)

### Performance & Experience

- **Zero-copy image transfer**: QQuickImageProvider directly accesses C++ image data
- **GPU-accelerated rendering**: Qt Scene Graph automatically uses GPU rendering
- **Algorithm consistency**: CPU video export uses the same algorithm as GPU preview
- **GPU OOM auto-recovery**: Automatically degrades to tiled processing when VRAM is insufficient
- **Stability optimization**: Trading latency for complete inference stability
- Main window uses a native `QQuickWindow` host to reduce frameless resize flicker on Windows
- Windows shutdown path now performs bounded cleanup before exit, so closing the main window no longer leaves a background process behind

### Modern UI

- Qt Quick (QML) declarative UI
- Dark/Light themes
- Chinese/English bilingual support
- Main window geometry is restored on restart through unified UI state persistence
- Session-based workflow with pinning, reordering, and batch file processing
- Embedded media viewer with fullscreen, drag-out, and smart docking support
- Shared media viewer kernel (`qml/components/mediaViewer/`) for unified canvas, controls, thumbnail adaptation, and context menu behavior
- Viewer thumbnails stay synchronized with message cards during incremental file completion and cleanup
- Cache cleanup summaries show residual files and next-step guidance when disk removal is partially blocked
- Message-card runtime state is unified in C++ derivation (including paused/recoverable), with declarative breathing-border animation that only runs during real processing
- Theme SVG icons are centrally managed through `Theme.icon()` and `ColoredIcon`, while bitmaps and brand logos stay on `Image`
- Runtime logs are kept lean by default; routine information/debug noise is removed from the common startup and shutdown paths
- First launch now inherits the installer-selected language when `settings.ini` has not been written yet
- Windows drag-and-drop now remains available for installed builds, including installer-finish launches and regular shortcut starts
- If the app is accidentally launched elevated, startup now attempts an automatic non-elevated relaunch to preserve drag-and-drop behavior

## Tech Stack

| Layer | Technology | Version |
|-------|------------|---------|
| UI Framework | Qt Quick (QML) | 6.10.2 |
| Graphics Rendering | Qt Scene Graph + RHI | Vulkan/D3D11/Metal |
| Application Framework | Qt Widgets | 6.10.2 |
| Programming Language | C++20 / QML | MSVC 2022 |
| AI Inference | NCNN (Vulkan accelerated) | latest |
| Multimedia | FFmpeg + Qt Multimedia | 7.1 |
| Internationalization | Qt Linguist | 6.10.2 |
| Build System | CMake | 3.20+ |

## Project Structure

```
EnhanceVision/
├── src/                    # C++ source code
│   ├── app/               # Application layer
│   ├── controllers/       # Controller layer
│   ├── core/              # Core engine
│   │   ├── ai/           # AI inference service
│   │   ├── inference/    # Inference backends
│   │   └── video/        # Video processing
│   ├── models/            # QML data models
│   ├── providers/         # QML image providers
│   ├── services/          # Service layer
│   └── utils/             # Utilities
├── include/EnhanceVision/ # C++ headers
├── qml/                   # QML source code
│   ├── pages/            # Pages
│   ├── components/       # Reusable components
│   │   └── mediaViewer/  # Shared media viewer internals
│   ├── controls/         # Custom controls
│   ├── shaders/          # ShaderEffect wrappers
│   ├── stores/           # State stores
│   ├── utils/            # QML utils and singletons
│   └── styles/           # Style definitions
├── resources/            # Qt resources
│   ├── shaders/          # GLSL shaders
│   ├── icons/            # SVG icons
│   └── i18n/             # Translation files
├── tests/                # Tests
├── docs/                 # Documentation
└── third_party/          # Third-party libraries
```

## Quick Start

### Requirements

- **Qt 6.10.2** (Quick, QuickControls2, Multimedia, ShaderTools, LinguistTools)
- **CMake 3.20+**
- **MSVC 2022** (Visual Studio 17)
- **Vulkan SDK**
- Third-party libraries: ncnn, ffmpeg, opencv (optional)

### Build

```powershell
# Debug build
cmake --preset windows-msvc-2022-debug
cmake --build build/msvc2022/Debug --config Debug -j 8

# Release build
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### Run

```powershell
.\build\msvc2022\Debug\Debug\EnhanceVision.exe
```

## User Guide

### Basic Operations

1. **Add Files**: Click "Add Files" button on the left or drag and drop files
2. **Select Mode**: Shader mode or AI mode
3. **Adjust Parameters**: Adjust in the right control panel
4. **Preview Effect**: Real-time preview of processing effects
5. **Export Result**: Click "Export" button to save

### Supported Formats

| Type | Formats |
|------|---------|
| Image | JPG, PNG, BMP, WebP |
| Video | MP4, MKV, AVI |

### Shader Parameters

| Effect | Range | Description |
|--------|-------|-------------|
| Brightness | -1.0 ~ 1.0 | Adjust image brightness |
| Contrast | 0.0 ~ 2.0 | Adjust contrast |
| Saturation | 0.0 ~ 2.0 | Adjust color saturation |
| Hue | -180 ~ 180 | Adjust hue shift |
| Exposure | -2.0 ~ 2.0 | Adjust exposure compensation |
| Gamma | 0.1 ~ 3.0 | Adjust gamma curve |
| Temperature | -1.0 ~ 1.0 | Adjust color temperature (cool/warm) |
| Tint | -1.0 ~ 1.0 | Adjust tint (green/magenta) |
| Highlights | -1.0 ~ 1.0 | Adjust highlight areas |
| Shadows | -1.0 ~ 1.0 | Adjust shadow areas |
| Sharpen | 0.0 ~ 2.0 | Enhance edge sharpness |
| Blur | 0.0 ~ 10.0 | Gaussian blur intensity |
| Vignette | 0.0 ~ 1.0 | Add vignette effect |

## Download & Installation

### Windows

| Version | File | Description |
|---------|------|-------------|
| v0.1.0 | [Installer](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-installer.exe) | Standard installation with uninstall support |
| v0.1.0 | [Portable](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-portable.zip) | Green version, extract and run |

### System Requirements

- Windows 10/11 64-bit
- Vulkan compatible GPU (optional, for AI acceleration)
- 4GB+ RAM
- 500MB+ disk space

### Installation Instructions

#### Installer Version
1. Download the installer
2. Double-click to run the installer
3. Follow the setup wizard
4. Launch EnhanceVision from Start Menu

Installer notes:
- The installer configures install directory and default export path on the same page
- Application runtime data is stored under `InstallDir\data`
- Upgrade installs can keep using the old data directory, migrate existing data into the new directory, or delete old data and start clean
- On first launch after upgrade, cache statistics and session content follow the effective data directory selected by the installer maintenance flow
- If the install directory is inside a protected path such as `Program Files`, the installer warns instead of forcing a path change
- On first launch, the UI language follows the installer language choice when no saved language exists yet
- Installed builds should accept Explorer drag-and-drop on first launch; include this as a release verification step after installation
- Include one manual admin-launch check in release validation: app should relaunch non-elevated (or show a clear warning if relaunch fails)
- Packaging now emits companion `*.sha256` checksum files for installer and portable archives; verify them before distribution

#### Portable Version
1. Download the ZIP file
2. Extract to any directory
3. Double-click `EnhanceVision.exe` to launch

## License

This project is licensed under the [MIT License](LICENSE).

### Third-Party Dependencies

This project uses the following open-source libraries:

| Dependency | License |
|------------|---------|
| Qt 6 | LGPL v3 |
| NCNN | BSD-3-Clause |
| FFmpeg | LGPL v2.1+ |
| Real-ESRGAN | BSD-3-Clause |

See [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for details.

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) to learn how to participate in development.

## Security

If you discover a security vulnerability, please refer to [SECURITY.md](SECURITY.md) for reporting instructions.

## Contact

- **Website**: [https://www.xiaogans.online/](https://www.xiaogans.online/)
- **Mirror**: [https://xiaogans.online/](https://xiaogans.online/)
- **Email**: saokiiritoasuna@qq.com
