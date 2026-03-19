---
alwaysApply: false
globs: ['**/CMakeLists.txt', '**/CMakePresets.json', '**/*.qrc']
description: '构建系统 - 涉及 CMake 配置、QML 资源、第三方库管理时参考'
trigger: glob
---
# 构建系统

## 构建流程

```
CMake 配置 → C++ 编译 → QML 资源打包 → 链接 → 部署 Qt 依赖
```

## CMake 主配置

```cmake
cmake_minimum_required(VERSION 3.20)
project(EnhanceVision VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 模块
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Quick
    QuickControls2
    Concurrent
    Multimedia
    Network
    ShaderTools
    LinguistTools
)

# 第三方库
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

# NCNN
add_subdirectory("${THIRD_PARTY_DIR}/ncnn")

# FFmpeg
set(FFMPEG_INCLUDE_DIR "${THIRD_PARTY_DIR}/ffmpeg/include")
set(FFMPEG_LIB_DIR "${THIRD_PARTY_DIR}/ffmpeg/lib")
set(FFMPEG_LIBS
    avcodec avformat avutil swscale swresample
)

# OpenCV
find_package(OpenCV REQUIRED)

# 可执行文件
qt_add_executable(EnhanceVision
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/core/AIEngine.cpp
    src/core/ImageProcessor.cpp
    src/core/VideoProcessor.cpp
    src/models/SessionModel.cpp
    src/models/FileModel.cpp
    src/providers/PreviewProvider.cpp
    src/providers/ThumbnailProvider.cpp
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
        qml/components/MediaViewer.qml
        qml/controls/IconButton.qml
        qml/controls/Slider.qml
        qml/shaders/BrightnessContrast.qml
        qml/styles/Theme.qml
        qml/styles/Colors.qml
    RESOURCES
        resources/icons/file.svg
        resources/icons/folder.svg
        resources/icons/settings.svg
)

# Shader 资源
qt_add_shaders(EnhanceVision "shaders"
    PREFIX "/shaders"
    BASE "${CMAKE_SOURCE_DIR}/resources/shaders"
    FILES
        "basic.vert"
        "brightness.frag"
        "contrast.frag"
        "saturation.frag"
        "sharpen.frag"
        "blur.frag"
)

# 链接库
target_link_libraries(EnhanceVision PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Network
    Qt6::ShaderTools
    ${OpenCV_LIBS}
    ncnn
)

target_include_directories(EnhanceVision PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${FFMPEG_INCLUDE_DIR}"
)

target_link_directories(EnhanceVision PRIVATE
    "${FFMPEG_LIB_DIR}"
)

foreach(lib ${FFMPEG_LIBS})
    target_link_libraries(EnhanceVision PRIVATE "${lib}")
endforeach()

# Windows 部署
if(WIN32)
    add_custom_command(TARGET EnhanceVision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${THIRD_PARTY_DIR}/ffmpeg/bin"
            "$<TARGET_FILE_DIR:EnhanceVision>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/models"
            "$<TARGET_FILE_DIR:EnhanceVision>/models"
    )
    
    find_program(WINDEPLOYQT windeployqt PATHS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET EnhanceVision POST_BUILD
            COMMAND "${WINDEPLOYQT}" "$<TARGET_FILE:EnhanceVision>"
        )
    endif()
endif()
```

## CMake Presets

```json
{
    "version": 3,
    "configurePresets": [
        {
            "name": "windows-msvc-2022-base",
            "hidden": true,
            "generator": "Visual Studio 17 2022",
            "architecture": "x64",
            "toolchainFile": "",
            "cacheVariables": {
                "CMAKE_PREFIX_PATH": "E:/Qt/6.10.2/msvc2022_64",
                "CMAKE_CXX_COMPILER": "cl"
            }
        },
        {
            "name": "windows-msvc-2022-debug",
            "displayName": "Debug",
            "inherits": "windows-msvc-2022-base",
            "binaryDir": "${sourceDir}/build/msvc2022/Debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug"
            }
        },
        {
            "name": "windows-msvc-2022-release",
            "displayName": "Release",
            "inherits": "windows-msvc-2022-base",
            "binaryDir": "${sourceDir}/build/msvc2022/Release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        },
        {
            "name": "windows-msvc-2022-relwithdebinfo",
            "displayName": "RelWithDebInfo",
            "inherits": "windows-msvc-2022-base",
            "binaryDir": "${sourceDir}/build/msvc2022/RelWithDebInfo",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "RelWithDebInfo"
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
        },
        {
            "name": "windows-msvc-2022-relwithdebinfo",
            "configurePreset": "windows-msvc-2022-relwithdebinfo",
            "configuration": "RelWithDebInfo"
        }
    ]
}
```

## QML 资源管理

### qt_add_qml_module

```cmake
qt_add_qml_module(EnhanceVision
    URI EnhanceVision          # QML 导入名称
    VERSION 1.0                # 模块版本
    QML_FILES                  # QML 源文件
        qml/main.qml
        qml/App.qml
        # ...
    RESOURCES                  # 其他资源
        resources/icons/file.svg
        resources/images/logo.png
    OUTPUT_DIRECTORY           # 输出目录
        ${CMAKE_BINARY_DIR}/qml
)
```

### 资源文件引用

```qml
// QML 中引用资源
import QtQuick
import EnhanceVision 1.0  // 导入自定义模块

Image {
    source: "qrc:/icons/file.svg"  // 资源路径
}

// 或使用相对路径（同一 qrc 中）
Image {
    source: "../icons/file.svg"
}
```

## Shader 编译

### qt_add_shaders

```cmake
qt_add_shaders(EnhanceVision "shaders"
    PREFIX "/shaders"                           # 资源前缀
    BASE "${CMAKE_SOURCE_DIR}/resources/shaders" # 基础路径
    FILES
        "basic.vert"
        "brightness.frag"
        "contrast.frag"
        "saturation.frag"
        "sharpen.frag"
        "blur.frag"
    OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders"    # 输出目录
)
```

### Shader 使用

```qml
ShaderEffect {
    vertexShader: "qrc:/shaders/basic.vert.qsb"
    fragmentShader: "qrc:/shaders/brightness.frag.qsb"
    
    property real brightness: 0.0
}
```

## 国际化构建

### 翻译文件生成

```cmake
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

set(TS_FILES
    "${CMAKE_SOURCE_DIR}/resources/i18n/app_zh_CN.ts"
    "${CMAKE_SOURCE_DIR}/resources/i18n/app_en_US.ts"
)

qt_add_translations(EnhanceVision
    TS_FILES ${TS_FILES}
    LUPDATE_OPTIONS -no-obsolete
    LRELEASE_OPTIONS -compress
)
```

### 更新翻译

```powershell
# 生成/更新 .ts 文件
cmake --build . --target EnhanceVision_lupdate

# 编译 .ts 为 .qm
cmake --build . --target EnhanceVision_lrelease
```

## 构建命令

### 配置

```powershell
# Debug 构建（开发调试）
cmake --preset windows-msvc-2022-debug

# Release 构建（性能测试）
cmake --preset windows-msvc-2022-release
```

### 编译

```powershell
# Debug 构建
cmake --build --preset windows-msvc-2022-debug

# Release 构建
cmake --build --preset windows-msvc-2022-release

# 并行编译（加速）
cmake --build --preset windows-msvc-2022-release -- /m
```

### 运行

```powershell
# Debug 版本
.\build\msvc2022\Debug\EnhanceVision.exe

# Release 版本
.\build\msvc2022\Release\EnhanceVision.exe
```

## 第三方库管理

| 库 | 管理方式 | 路径 |
|----|----------|------|
| NCNN | add_subdirectory | `third_party/ncnn/` |
| FFmpeg | 预编译库 | `third_party/ffmpeg/` |
| OpenCV | FetchContent | 自动下载 |

### OpenCV FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    opencv
    GIT_REPOSITORY https://github.com/opencv/opencv.git
    GIT_TAG 4.5.5
)

FetchContent_MakeAvailable(opencv)
```

## 构建后处理

### 自动复制 DLL

```cmake
add_custom_command(TARGET EnhanceVision POST_BUILD
    # 复制 FFmpeg DLL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${THIRD_PARTY_DIR}/ffmpeg/bin"
        "$<TARGET_FILE_DIR:EnhanceVision>"
    
    # 复制 AI 模型
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources/models"
        "$<TARGET_FILE_DIR:EnhanceVision>/models"
    
    # 复制翻译文件
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/resources/i18n"
        "$<TARGET_FILE_DIR:EnhanceVision>/i18n"
)
```

### windeployqt

```cmake
find_program(WINDEPLOYQT windeployqt 
    PATHS "${Qt6_DIR}/../../../bin"
)

if(WINDEPLOYQT)
    add_custom_command(TARGET EnhanceVision POST_BUILD
        COMMAND "${WINDEPLOYQT}" 
            "$<TARGET_FILE:EnhanceVision>"
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
    )
endif()
```

## 常见问题

### 找不到 Qt

```powershell
# 设置 Qt 路径
cmake --preset windows-msvc-2022-debug -DCMAKE_PREFIX_PATH=E:/Qt/6.10.2/msvc2022_64
```

### QML 模块找不到

```cmake
# 确保 QML_FILES 路径正确
qt_add_qml_module(EnhanceVision
    URI EnhanceVision
    VERSION 1.0
    QML_FILES
        qml/main.qml  # 相对于 CMakeLists.txt
)
```

### Shader 编译失败

```powershell
# 检查 ShaderTools 模块
find_package(Qt6 REQUIRED COMPONENTS ShaderTools)

# 手动编译 shader
qsb --glsl "100 es,120,150" --hlsl 50 --msl 12 -o shader.frag.qsb shader.frag
```

### Debug 构建性能问题

Debug 构建禁用优化，性能比 Release 慢 2-10x。性能测试必须使用 Release 构建。
