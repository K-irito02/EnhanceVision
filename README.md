# EnhanceVision

English | [简体中文](README_CN.md)

A desktop image and video enhancement tool built with **Qt 6.10.2 + QML**. Using native Qt Quick architecture with GPU Shader processing and NCNN AI inference engine, providing a balance between performance and quality.

## Core Features

### Dual-Mode Processing

| Mode | Technology | Effects |
|------|------------|---------|
| **Shader Mode** | Qt RHI / GLSL | 14 real-time parameter adjustments (exposure, brightness, contrast, saturation, hue, gamma, color temperature, tint, highlights, shadows, vignette, blur, denoise, sharpen) |
| **AI Inference Mode** | NCNN + Vulkan | Real-ESRGAN super-resolution enhancement (4x) |

### Concurrent Scheduling System

- Multi-level priority task queues (Critical/High/Normal/Low)
- AI engine pool supporting 2 concurrent inference tasks
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

### Modern UI

- Qt Quick (QML) declarative UI
- Dark/Light themes
- Chinese/English bilingual support
- Session-based workflow with pinning, reordering, and batch file processing
- Embedded media viewer with fullscreen, drag-out, and smart docking support
- Viewer thumbnails stay synchronized with message cards during incremental file completion and cleanup
- Cache cleanup summaries show residual files and next-step guidance when disk removal is partially blocked
- Message-card runtime state is unified in C++ derivation (including paused/recoverable), with declarative breathing-border animation that only runs during real processing

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
│   ├── models/            # QML data models
│   ├── providers/         # QML image providers
│   ├── services/          # Service layer
│   └── utils/             # Utilities
├── include/EnhanceVision/ # C++ headers
├── qml/                   # QML source code
│   ├── pages/            # Pages
│   ├── components/       # Reusable components
│   ├── controls/         # Custom controls
│   ├── shaders/          # ShaderEffect wrappers
│   └── styles/           # Style definitions
├── resources/            # Qt resources
│   ├── shaders/          # GLSL shaders
│   ├── icons/            # SVG icons
│   ├── models/           # AI models
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
