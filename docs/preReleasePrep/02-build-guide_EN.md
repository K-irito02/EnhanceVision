# Release Build Guide

English | [简体中文](02-build-guide_CN.md)

This document details how to build the Release version of EnhanceVision.

## 📋 Table of Contents

- [Requirements](#requirements)
- [Pre-Build Preparation](#pre-build-preparation)
- [Build Steps](#build-steps)
- [Build Verification](#build-verification)
- [Common Issues](#common-issues)

---

## Requirements

### Operating System
- Windows 10/11 64-bit

### Development Tools

| Tool | Version Required | Purpose |
|------|------------------|---------|
| Visual Studio 2022 | 17.0+ | C++ Compiler (MSVC) |
| CMake | 3.20+ | Build System |
| Qt | 6.10.2 | UI Framework |
| Vulkan SDK | Latest | GPU Acceleration |

### Third-Party Libraries

| Library | Version | Location |
|---------|---------|----------|
| NCNN | latest | third_party/ncnn |
| FFmpeg | 7.1 | third_party/ffmpeg |
| OpenCV | Optional | third_party/opencv |

---

## Pre-Build Preparation

### 1. Check Environment Variables

```powershell
# Check Qt path
$env:Qt6_DIR
# Should output something like: E:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6

# Check Vulkan SDK
$env:VULKAN_SDK
# Should output something like: E:\Vulkan\1.3.xxx.0

# Check CMake
cmake --version
# Should output: cmake version 3.20+

# Check MSVC
cl
# Should output: Microsoft (R) C/C++ Optimizing Compiler Version 19.xx
```

### 2. Sync Code

```powershell
# Enter project directory
cd E:\QtAudio-VideoLearning\EnhanceVision

# Pull latest code
git pull origin main

# Update submodules
git submodule update --init --recursive
```

### 3. Update Version Number

Edit `CMakeLists.txt` to confirm the version number is correct:

```cmake
project(EnhanceVision VERSION 0.1.0 LANGUAGES CXX)
```

### 4. Clean Old Build (Optional)

```powershell
# Delete old build directory
Remove-Item -Recurse -Force build\msvc2022\Release -ErrorAction SilentlyContinue
```

---

## Build Steps

### Method 1: Using CMake Preset (Recommended)

```powershell
# Configure project
cmake --preset windows-msvc-2022-release

# Build project (using 8 parallel tasks)
cmake --build build/msvc2022/Release --config Release -j 8
```

### Method 2: Manual Configuration

```powershell
# Create build directory
New-Item -ItemType Directory -Force -Path build\msvc2022\Release
cd build\msvc2022\Release

# Configure project
cmake ..\..\.. `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="E:/Qt/6.10.2/msvc2022_64" `
    -DNCNN_USE_VULKAN=ON

# Build project
cmake --build . --config Release -j 8

# Return to project root
cd ..\..\..
```

### Build Output

After successful build, output files are located at:
```
build/msvc2022/Release/Release/EnhanceVision.exe
```

---

## Build Verification

### 1. Check Executable File

```powershell
# Check if file exists
Test-Path build\msvc2022\Release\Release\EnhanceVision.exe

# Check file size (should be > 1MB)
(Get-Item build\msvc2022\Release\Release\EnhanceVision.exe).Length / 1MB
```

### 2. Check Dependency Files

```powershell
# Check if Qt DLLs are deployed
Get-ChildItem build\msvc2022\Release\Release\Qt*.dll | Measure-Object

# Check if FFmpeg DLLs are deployed
Get-ChildItem build\msvc2022\Release\Release\av*.dll | Measure-Object
```

### 3. Run Tests

```powershell
# Run unit tests
ctest --test-dir build/msvc2022/Release -C Release --output-on-failure
```

### 4. Run Application

```powershell
# Launch application
.\build\msvc2022\Release\Release\EnhanceVision.exe
```

Verify the following:
- [ ] Application starts normally
- [ ] UI displays correctly
- [ ] Add file function works
- [ ] Shader processing works
- [ ] AI processing works
- [ ] Video processing works
- [ ] Export function works

---

## Build Optimization Options

### Enable Link-Time Optimization (LTCG)

Already configured in CMakeLists.txt:
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(EnhanceVision PRIVATE /O2 /GL)
    target_link_options(EnhanceVision PRIVATE /LTCG)
endif()
```

### Enable Precompiled Headers

Already configured in CMakeLists.txt:
```cmake
option(USE_PRECOMPILED_HEADERS "Use precompiled headers for faster compilation" ON)
```

### Disable Debug Information

Release builds don't generate debug information by default, reducing file size.

---

## Common Issues

### Q1: Qt Not Found

**Error Message**:
```
CMake Error at CMakeLists.txt:40 (find_package):
  Could not find a package configuration file provided by "Qt6"
```

**Solution**:
```powershell
# Set Qt path
$env:CMAKE_PREFIX_PATH = "E:\Qt\6.10.2\msvc2022_64"

# Or specify in CMake command
cmake --preset windows-msvc-2022-release -DCMAKE_PREFIX_PATH="E:/Qt/6.10.2/msvc2022_64"
```

### Q2: Vulkan SDK Not Found

**Error Message**:
```
Vulkan SDK not found, falling back to CPU only
```

**Solution**:
```powershell
# Install Vulkan SDK
# Download from: https://vulkan.lunarg.com/sdk/home

# Set environment variable
$env:VULKAN_SDK = "E:\Vulkan\1.3.xxx.0"
```

### Q3: Out of Memory During Compilation

**Error Message**:
```
fatal error C1060: compiler is out of heap space
```

**Solution**:
```powershell
# Reduce parallel compilation count
cmake --build build/msvc2022/Release --config Release -j 4

# Or use 64-bit compiler
# Select "x64 Native Tools Command Prompt" in Visual Studio
```

### Q4: windeployqt Failed

**Error Message**:
```
windeployqt not found
```

**Solution**:
```powershell
# Run windeployqt manually
$env:PATH += ";E:\Qt\6.10.2\msvc2022_64\bin"
windeployqt build\msvc2022\Release\Release\EnhanceVision.exe --qmldir qml
```

### Q5: FFmpeg DLLs Missing

**Error Message**:
```
The application was unable to start correctly (0xc000007b)
```

**Solution**:
```powershell
# Copy FFmpeg DLLs manually
Copy-Item -Path "third_party\ffmpeg\bin\*.dll" -Destination "build\msvc2022\Release\Release\"
```

---

## Build Artifacts Checklist

After Release build completes, the output directory should contain:

```
build/msvc2022/Release/Release/
├── EnhanceVision.exe          # Main executable
├── models/                    # AI models
│   ├── RealESRGAN_x4plus.param
│   └── RealESRGAN_x4plus.bin
├── translations/              # Translation files
│   ├── app_zh_CN.qm
│   └── app_en_US.qm
├── Qt*.dll                    # Qt core libraries
├── platforms/                 # Qt platform plugins
│   └── qwindows.dll
├── imageformats/              # Qt image format plugins
├── multimedia/                # Qt multimedia plugins
├── avcodec-62.dll             # FFmpeg codec library
├── avformat-62.dll            # FFmpeg format library
├── avutil-60.dll              # FFmpeg utility library
├── swscale-9.dll              # FFmpeg scaling library
├── swresample-6.dll           # FFmpeg resampling library
└── vulkan-1.dll               # Vulkan runtime
```

---

## Next Steps

After building, refer to [03-packaging-guide.md](03-packaging-guide.md) for packaging.

---

*Last updated: 2025*
