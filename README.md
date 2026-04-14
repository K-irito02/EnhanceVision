# EnhanceVision

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
