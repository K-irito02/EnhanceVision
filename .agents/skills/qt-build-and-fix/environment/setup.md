# 环境配置

## 必需环境

| 工具 | 路径 | 验证命令 |
|------|------|----------|
| CMake | `C:\Program Files\CMake\bin\cmake.exe` | `cmake --version` |
| Qt 6.10.2 | `E:\Qt\6.10.2\msvc2022_64` | `qmake --version` |
| MSVC 2022 | Visual Studio 17 2022 | `cl` (Developer Command Prompt) |
| Vulkan SDK | `E:\Vulkan\VulkanSDK` | `vulkaninfo` |

## 环境验证脚本

```powershell
# 验证 CMake
if (-not (Test-Path "C:\Program Files\CMake\bin\cmake.exe")) {
    Write-Error "CMake 未找到，请安装 CMake 3.20+"
    exit 1
}

# 验证 Qt
if (-not (Test-Path "E:\Qt\6.10.2\msvc2022_64")) {
    Write-Error "Qt 6.10.2 未找到，请检查安装路径"
    exit 1
}

# 验证项目文件
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Error "CMakeLists.txt 未找到，请在项目根目录执行"
    exit 1
}

Write-Host "环境验证通过"
```

## 第三方依赖

| 依赖 | 路径 | 说明 |
|------|------|------|
| NCNN | `third_party/ncnn` | AI 推理框架 |
| FFmpeg | `third_party/ffmpeg` | 视频处理 |
| OpenCV | `third_party/opencv` | 图像处理（可选） |

## Qt 模块依赖

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Quick
    QuickWidgets
    QuickControls2
    Concurrent
    Multimedia
    Network
    ShaderTools
    LinguistTools
    QuickEffects
)
```

## 环境变量

```powershell
# Qt 路径
$env:Qt6_DIR = "E:\Qt\6.10.2\msvc2022_64"

# Vulkan SDK
$env:VULKAN_SDK = "E:\Vulkan\VulkanSDK"

# CMake 前缀路径
$env:CMAKE_PREFIX_PATH = "E:\Qt\6.10.2\msvc2022_64"
```

## 常见环境问题

### Qt 路径未找到

```powershell
# 检查 Qt 安装
Test-Path "E:\Qt\6.10.2\msvc2022_64\bin\qmake.exe"

# 设置环境变量
$env:PATH += ";E:\Qt\6.10.2\msvc2022_64\bin"
```

### Vulkan SDK 未找到

```powershell
# 检查 Vulkan
Test-Path "E:\Vulkan\VulkanSDK"

# 设置环境变量
$env:VULKAN_SDK = "E:\Vulkan\VulkanSDK"
```

### MSVC 编译器未找到

在 Visual Studio Developer Command Prompt 或 Developer PowerShell 中执行构建命令。
