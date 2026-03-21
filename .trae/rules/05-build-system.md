---
alwaysApply: false
globs: ['**/CMakeLists.txt', '**/CMakePresets.json', '**/*.cmake']
description: '构建系统 - CMake 配置、依赖管理、构建预设'
trigger: glob
---
# 构建系统

## 构建环境

### 必需工具

| 工具 | 版本 | 路径 |
|------|------|------|
| CMake | 3.20+ | `C:\Program Files\CMake\bin\cmake.exe` |
| Qt | 6.10.2 | `E:\Qt\6.10.2\msvc2022_64` |
| MSVC | 2022 (VS 17) | Visual Studio 17 2022 |

### 编译器配置

- **Generator**: `Visual Studio 17 2022`
- **Architecture**: `x64`
- **C++ Standard**: `C++20`

## CMake 预设

项目使用 `CMakePresets.json` 管理构建配置：

```json
{
    "version": 3,
    "configurePresets": [
        {
            "name": "windows-msvc-2022-base",
            "hidden": true,
            "generator": "Visual Studio 17 2022",
            "architecture": "x64",
            "binaryDir": "${sourceDir}/build/msvc2022/${presetName}",
            "cacheVariables": {
                "CMAKE_CXX_STANDARD": "20",
                "CMAKE_CXX_STANDARD_REQUIRED": "ON",
                "CMAKE_AUTOMOC": "ON",
                "CMAKE_AUTORCC": "ON",
                "CMAKE_AUTOUIC": "ON",
                "CMAKE_PREFIX_PATH": "E:/Qt/6.10.2/msvc2022_64"
            }
        },
        {
            "name": "windows-msvc-2022-debug",
            "displayName": "Debug",
            "inherits": "windows-msvc-2022-base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug"
            }
        },
        {
            "name": "windows-msvc-2022-release",
            "displayName": "Release",
            "inherits": "windows-msvc-2022-base",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "windows-msvc-2022-debug",
            "configurePreset": "windows-msvc-2022-debug",
            "configuration": "Debug"
        },
        {
            "name": "windows-msvc-2022-release",
            "configurePreset": "windows-msvc-2022-release",
            "configuration": "Release"
        }
    ]
}
```

## 构建命令

### 配置项目

```powershell
# Debug 配置
cmake --preset windows-msvc-2022-debug

# Release 配置
cmake --preset windows-msvc-2022-release
```

### 编译项目

```powershell
# Debug 构建
cmake --build --preset windows-msvc-2022-debug

# Release 构建
cmake --build --preset windows-msvc-2022-release

# 并行编译（指定作业数）
cmake --build --preset windows-msvc-2022-release -- -j 8
```

### 清理构建

```powershell
# 清理构建产物
cmake --build --preset windows-msvc-2022-release --target clean

# 完全清理（删除 build 目录）
Remove-Item -Path "build\msvc2022" -Recurse -Force
```

## CMakeLists.txt 结构

### 主 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)

project(EnhanceVision VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 路径
set(CMAKE_PREFIX_PATH "E:/Qt/6.10.2/msvc2022_64")

# 查找 Qt 模块
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    Widgets
    Multimedia
    ShaderTools
    LinguistTools
)

# 添加子目录
add_subdirectory(third_party/ncnn)

# 源文件
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/controllers/FileController.cpp
    src/controllers/ProcessingController.cpp
    src/controllers/SessionController.cpp
    src/controllers/SettingsController.cpp
    src/core/ProcessingEngine.cpp
    src/core/ImageProcessor.cpp
    src/core/VideoProcessor.cpp
    src/core/FrameCache.cpp
    src/core/TaskCoordinator.cpp
    src/core/ResourceManager.cpp
    src/core/AIEngine.cpp
    src/core/ModelRegistry.cpp
    src/models/SessionModel.cpp
    src/models/FileModel.cpp
    src/models/ProcessingModel.cpp
    src/models/MessageModel.cpp
    src/providers/PreviewProvider.cpp
    src/providers/ThumbnailProvider.cpp
    src/services/ImageExportService.cpp
    src/utils/FileUtils.cpp
    src/utils/ImageUtils.cpp
    src/utils/SubWindowHelper.cpp
    src/utils/WindowHelper.cpp
)

# 头文件
set(HEADERS
    include/EnhanceVision/app/Application.h
    include/EnhanceVision/app/MainWindow.h
    include/EnhanceVision/controllers/FileController.h
    include/EnhanceVision/controllers/ProcessingController.h
    include/EnhanceVision/controllers/SessionController.h
    include/EnhanceVision/controllers/SettingsController.h
    include/EnhanceVision/core/ProcessingEngine.h
    include/EnhanceVision/core/ImageProcessor.h
    include/EnhanceVision/core/VideoProcessor.h
    include/EnhanceVision/core/FrameCache.h
    include/EnhanceVision/core/TaskCoordinator.h
    include/EnhanceVision/core/ResourceManager.h
    include/EnhanceVision/core/AIEngine.h
    include/EnhanceVision/core/ModelRegistry.h
    include/EnhanceVision/models/SessionModel.h
    include/EnhanceVision/models/FileModel.h
    include/EnhanceVision/models/ProcessingModel.h
    include/EnhanceVision/models/MessageModel.h
    include/EnhanceVision/providers/PreviewProvider.h
    include/EnhanceVision/providers/ThumbnailProvider.h
    include/EnhanceVision/services/ImageExportService.h
    include/EnhanceVision/utils/FileUtils.h
    include/EnhanceVision/utils/ImageUtils.h
    include/EnhanceVision/utils/SubWindowHelper.h
    include/EnhanceVision/utils/WindowHelper.h
)

# QML 模块
qt_add_qml_module(EnhanceVision
    URI EnhanceVision
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/App.qml
        qml/pages/MainPage.qml
        qml/pages/SettingsPage.qml
        qml/components/Sidebar.qml
        qml/components/TitleBar.qml
        qml/components/FileList.qml
        qml/components/PreviewPane.qml
        qml/components/ControlPanel.qml
        qml/components/EmbeddedMediaViewer.qml
        qml/components/FullShaderEffect.qml
        qml/components/OffscreenShaderRenderer.qml
        qml/controls/IconButton.qml
        qml/controls/Slider.qml
        qml/styles/Theme.qml
        qml/styles/Colors.qml
    RESOURCES
        resources/icons/...
        resources/shaders/full_shader.frag
        resources/shaders/full_shader.vert
)

# 可执行文件
add_executable(EnhanceVision
    ${SOURCES}
    ${HEADERS}
)

# 包含目录
target_include_directories(EnhanceVision PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EnhanceVision
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接库
target_link_libraries(EnhanceVision PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Multimedia
    ncnn
)

# 编译选项
target_compile_options(EnhanceVision PRIVATE
    /W4           # 警告级别 4
    /WX           # 警告视为错误
    /MP           # 多进程编译
)

# 定义
target_compile_definitions(EnhanceVision PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    QT_NO_KEYWORDS
)

# 安装规则
install(TARGETS EnhanceVision
    RUNTIME DESTINATION bin
)
```

## 依赖管理

### Qt 模块

| 模块 | 用途 |
|------|------|
| `Core` | 核心功能、QObject、信号槽 |
| `Gui` | 图像处理、QImage |
| `Quick` | QML 引擎 |
| `QuickControls2` | Qt Quick Controls 2 |
| `Widgets` | QApplication、主窗口 |
| `Multimedia` | 视频播放 |
| `ShaderTools` | Shader 编译 |
| `LinguistTools` | 国际化 |

### 第三方库

| 库 | 用途 | 集成方式 |
|-----|------|----------|
| NCNN | AI 推理 | `add_subdirectory` |
| FFmpeg | 视频解码/编码 | 预编译库 |

### NCNN 集成

```cmake
# third_party/ncnn/CMakeLists.txt
add_subdirectory(ncnn)

target_include_directories(ncnn PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ncnn/src
)
```

### FFmpeg 集成

```cmake
# FFmpeg 预编译库
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/third_party/ffmpeg")

target_include_directories(EnhanceVision PRIVATE
    ${FFMPEG_ROOT}/include
)

target_link_directories(EnhanceVision PRIVATE
    ${FFMPEG_ROOT}/lib
)

target_link_libraries(EnhanceVision PRIVATE
    avcodec
    avformat
    avutil
    swscale
    swresample
)
```

## 资源管理

### Qt 资源文件 (.qrc)

```xml
<!-- resources/qml.qrc -->
<RCC>
    <qresource prefix="/">
        <file>icons/add.svg</file>
        <file>icons/remove.svg</file>
        <file>icons-dark/add.svg</file>
        <file>icons-dark/remove.svg</file>
        <file>shaders/full_shader.frag</file>
        <file>shaders/full_shader.vert</file>
        <file>models/RealESRGAN_x4plus.param</file>
        <file>models/RealESRGAN_x4plus.bin</file>
    </qresource>
</RCC>
```

### Shader 编译

```cmake
# 编译 GLSL 到 QSB
qt_add_shaders(EnhanceVision "shaders"
    PREFIX "/shaders"
    FILES
        resources/shaders/full_shader.frag
        resources/shaders/full_shader.vert
)
```

## 国际化

### 翻译文件生成

```cmake
# 生成 .ts 文件
qt_add_lrelease(EnhanceVision
    TS_FILES
        resources/i18n/app_zh_CN.ts
        resources/i18n/app_en_US.ts
)

# 更新翻译
add_custom_target(lupdate
    COMMAND ${Qt6_LUPDATE_EXECUTABLE}
        ${SOURCES}
        ${HEADERS}
        ${QML_FILES}
        -ts resources/i18n/app_zh_CN.ts
        resources/i18n/app_en_US.ts
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

## 构建产物

### 输出目录

```
build/
└── msvc2022/
    ├── Debug/
    │   ├── EnhanceVision.exe
    │   └── ...
    └── Release/
        ├── EnhanceVision.exe
        └── ...
```

### 部署

```powershell
# 使用 windeployqt 部署 Qt 依赖
& "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" `
    "build\msvc2022\Release\Release\EnhanceVision.exe" `
    --release `
    --qmldir "qml"
```

## 常见构建问题

### Qt 路径错误

```
CMake Error: Could not find Qt6
```

**解决方案**：

```cmake
set(CMAKE_PREFIX_PATH "E:/Qt/6.10.2/msvc2022_64")
```

### MOC 错误

```
Error: Undefined reference to vtable
```

**解决方案**：

```cmake
set(CMAKE_AUTOMOC ON)
```

### QML 模块未找到

```
module "QtQuick" is not installed
```

**解决方案**：

```powershell
# 确保 QML 导入路径正确
set QML2_IMPORT_PATH=E:\Qt\6.10.2\msvc2022_64\qml
```

### 链接错误

```
LNK2019: unresolved external symbol
```

**解决方案**：

1. 检查源文件是否添加到 `SOURCES`
2. 检查库是否链接
3. 检查库路径是否正确

## 构建优化

### 预编译头

```cmake
if(MSVC)
    target_precompile_headers(EnhanceVision PRIVATE
        <memory>
        <string>
        <vector>
        <QObject>
        <QImage>
    )
endif()
```

### 多进程编译

```cmake
target_compile_options(EnhanceVision PRIVATE /MP)
```

### 增量链接

```cmake
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL")
endif()
```

## 构建脚本

### 完整构建脚本

```powershell
# build.ps1
param(
    [string]$Config = "Release",
    [int]$Jobs = 8
)

$ErrorActionPreference = "Stop"

Write-Host "构建 EnhanceVision ($Config)..."

# 配置
cmake --preset "windows-msvc-2022-$Config"
if ($LASTEXITCODE -ne 0) { exit 1 }

# 编译
cmake --build "build/msvc2022/$Config" --config $Config -j $Jobs
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "构建成功！"
Write-Host "输出: build\msvc2022\$Config\$Config\EnhanceVision.exe"
```

### 运行脚本

```powershell
# Debug 构建
.\build.ps1 -Config Debug

# Release 构建
.\build.ps1 -Config Release -Jobs 16
```
