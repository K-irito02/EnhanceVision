# Release 构建指南

[English](02-build-guide_EN.md) | 简体中文

本文档详细说明如何构建 EnhanceVision 的 Release 版本。

## 📋 目录

- [环境要求](#环境要求)
- [构建前准备](#构建前准备)
- [构建步骤](#构建步骤)
- [构建验证](#构建验证)
- [常见问题](#常见问题)

---

## 环境要求

### 操作系统
- Windows 10/11 64-bit

### 开发工具

| 工具 | 版本要求 | 用途 |
|------|----------|------|
| Visual Studio 2022 | 17.0+ | C++ 编译器 (MSVC) |
| CMake | 3.20+ | 构建系统 |
| Qt | 6.10.2 | UI 框架 |
| Vulkan SDK | 最新版 | GPU 加速 |

### 第三方库

| 库 | 版本 | 位置 |
|----|------|------|
| NCNN | latest | third_party/ncnn |
| FFmpeg | 7.1 | third_party/ffmpeg |
| OpenCV | 可选 | third_party/opencv |

---

## 构建前准备

### 1. 检查环境变量

```powershell
# 检查 Qt 路径
$env:Qt6_DIR
# 应输出类似: E:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6

# 检查 Vulkan SDK
$env:VULKAN_SDK
# 应输出类似: E:\Vulkan\1.3.xxx.0

# 检查 CMake
cmake --version
# 应输出: cmake version 3.20+

# 检查 MSVC
cl
# 应输出: Microsoft (R) C/C++ Optimizing Compiler Version 19.xx
```

### 2. 同步代码

```powershell
# 进入项目目录
cd E:\QtAudio-VideoLearning\EnhanceVision

# 拉取最新代码
git pull origin main

# 更新子模块
git submodule update --init --recursive
```

### 3. 更新版本号

编辑 `CMakeLists.txt`，确认版本号正确：

```cmake
project(EnhanceVision VERSION 0.1.0 LANGUAGES CXX)
```

### 4. 清理旧构建（可选）

```powershell
# 删除旧的构建目录
Remove-Item -Recurse -Force build\msvc2022\Release -ErrorAction SilentlyContinue
```

---

## 构建步骤

### 方法一：使用 CMake Preset（推荐）

```powershell
# 配置项目
cmake --preset windows-msvc-2022-release

# 构建项目（使用 8 个并行任务）
cmake --build build/msvc2022/Release --config Release -j 8
```

### 方法二：手动配置

```powershell
# 创建构建目录
New-Item -ItemType Directory -Force -Path build\msvc2022\Release
cd build\msvc2022\Release

# 配置项目
cmake ..\..\.. `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="E:/Qt/6.10.2/msvc2022_64" `
    -DNCNN_USE_VULKAN=ON

# 构建项目
cmake --build . --config Release -j 8

# 返回项目根目录
cd ..\..\..
```

### 构建输出

构建成功后，输出文件位于：
```
build/msvc2022/Release/Release/EnhanceVision.exe
```

---

## 构建验证

### 1. 检查可执行文件

```powershell
# 检查文件是否存在
Test-Path build\msvc2022\Release\Release\EnhanceVision.exe

# 检查文件大小（应该 > 1MB）
(Get-Item build\msvc2022\Release\Release\EnhanceVision.exe).Length / 1MB
```

### 2. 检查依赖文件

```powershell
# 检查 Qt DLL 是否部署
Get-ChildItem build\msvc2022\Release\Release\Qt*.dll | Measure-Object

# 检查 FFmpeg DLL 是否部署
Get-ChildItem build\msvc2022\Release\Release\av*.dll | Measure-Object
```

### 3. 运行测试

```powershell
# 运行单元测试
ctest --test-dir build/msvc2022/Release -C Release --output-on-failure
```

### 4. 运行应用程序

```powershell
# 启动应用程序
.\build\msvc2022\Release\Release\EnhanceVision.exe
```

验证以下功能：
- [ ] 应用程序启动正常
- [ ] UI 显示正常
- [ ] 添加文件功能正常
- [ ] Shader 处理功能正常
- [ ] AI 处理功能正常
- [ ] 视频处理功能正常
- [ ] 导出功能正常

---

## 构建优化选项

### 启用链接时优化 (LTCG)

已在 CMakeLists.txt 中配置：
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(EnhanceVision PRIVATE /O2 /GL)
    target_link_options(EnhanceVision PRIVATE /LTCG)
endif()
```

### 启用预编译头

已在 CMakeLists.txt 中配置：
```cmake
option(USE_PRECOMPILED_HEADERS "Use precompiled headers for faster compilation" ON)
```

### 禁用调试信息

Release 构建默认不生成调试信息，减小文件大小。

---

## 常见问题

### Q1: 找不到 Qt

**错误信息**:
```
CMake Error at CMakeLists.txt:40 (find_package):
  Could not find a package configuration file provided by "Qt6"
```

**解决方案**:
```powershell
# 设置 Qt 路径
$env:CMAKE_PREFIX_PATH = "E:\Qt\6.10.2\msvc2022_64"

# 或在 CMake 命令中指定
cmake --preset windows-msvc-2022-release -DCMAKE_PREFIX_PATH="E:/Qt/6.10.2/msvc2022_64"
```

### Q2: 找不到 Vulkan SDK

**错误信息**:
```
Vulkan SDK not found, falling back to CPU only
```

**解决方案**:
```powershell
# 安装 Vulkan SDK
# 下载地址: https://vulkan.lunarg.com/sdk/home

# 设置环境变量
$env:VULKAN_SDK = "E:\Vulkan\1.3.xxx.0"
```

### Q3: 编译内存不足

**错误信息**:
```
fatal error C1060: compiler is out of heap space
```

**解决方案**:
```powershell
# 减少并行编译数
cmake --build build/msvc2022/Release --config Release -j 4

# 或使用 64 位编译器
# 在 Visual Studio 中选择 "x64 Native Tools Command Prompt"
```

### Q4: windeployqt 失败

**错误信息**:
```
windeployqt not found
```

**解决方案**:
```powershell
# 手动运行 windeployqt
$env:PATH += ";E:\Qt\6.10.2\msvc2022_64\bin"
windeployqt build\msvc2022\Release\Release\EnhanceVision.exe --qmldir qml
```

### Q5: FFmpeg DLL 缺失

**错误信息**:
```
应用程序无法正确启动 (0xc000007b)
```

**解决方案**:
```powershell
# 手动复制 FFmpeg DLL
Copy-Item -Path "third_party\ffmpeg\bin\*.dll" -Destination "build\msvc2022\Release\Release\"
```

---

## 构建产物清单

Release 构建完成后，输出目录应包含以下文件：

```
build/msvc2022/Release/Release/
├── EnhanceVision.exe          # 主程序
├── models/                    # AI 模型
│   ├── RealESRGAN_x4plus.param
│   └── RealESRGAN_x4plus.bin
├── translations/              # 翻译文件
│   ├── app_zh_CN.qm
│   └── app_en_US.qm
├── Qt*.dll                    # Qt 核心库
├── platforms/                 # Qt 平台插件
│   └── qwindows.dll
├── imageformats/              # Qt 图像格式插件
├── multimedia/                # Qt 多媒体插件
├── avcodec-62.dll             # FFmpeg 编解码库
├── avformat-62.dll            # FFmpeg 格式库
├── avutil-60.dll              # FFmpeg 工具库
├── swscale-9.dll              # FFmpeg 缩放库
├── swresample-6.dll           # FFmpeg 重采样库
└── vulkan-1.dll               # Vulkan 运行时
```

---

## 下一步

构建完成后，请参考 [03-packaging-guide.md](03-packaging-guide.md) 进行打包。

---

*最后更新: 2025年*
