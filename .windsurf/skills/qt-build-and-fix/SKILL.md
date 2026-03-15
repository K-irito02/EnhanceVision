---
name: "qt-build-and-fix"
description: "Automated EnhanceVision project build, run and fix. Use when user asks to build project, run tests, check code quality or solve compilation issues."
---

# Qt Build and Fix Skill

自动化 EnhanceVision 项目（Qt Quick + C++）的构建、运行和修复生命周期管理。

## 项目架构

| 层级 | 技术 |
|------|------|
| UI 框架 | Qt Quick (QML) + Qt Widgets |
| 图形渲染 | Qt Scene Graph + RHI |
| C++ 后端 | Core 引擎 + Controllers + Models |
| AI 推理 | NCNN (Vulkan 加速) |
| 多媒体 | FFmpeg 7.1 + Qt Multimedia |
| 构建系统 | CMake 3.20+ |

## 执行流程

### 1. 清理日志

每次执行前清空 `logs/` 目录所有旧日志文件。

```powershell
if (Test-Path "logs") { 
    Remove-Item -Path "logs\*" -Recurse -Force -ErrorAction SilentlyContinue
} else { 
    New-Item -ItemType Directory -Path "logs" -Force | Out-Null
}
```

### 2. 环境检查

确认以下环境：
- CMake: `C:\Program Files\CMake\bin\cmake.exe`
- Qt 6.10.2: `E:\Qt\6.10.2\msvc2022_64`
- MSVC 2022: Visual Studio 17 2022
- 项目文件: `CMakeLists.txt`, `CMakePresets.json`

### 3. CMake 配置

使用 CMake Presets：

```powershell
# 清理旧构建（可选，首次构建或遇到问题时执行）
Remove-Item -Path "build\msvc2022" -Recurse -Force -ErrorAction SilentlyContinue

# 配置（Release 模式）
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release 2>&1 | Tee-Object -FilePath "logs/cmake_configure.log"
```

### 4. 编译项目

```powershell
& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8 2>&1 | Tee-Object -FilePath "logs/build_output.log"
```

### 5. 分析与修复

读取编译日志，分析错误和警告：

```powershell
# 检查错误
Select-String -Path "logs\build_output.log" -Pattern "error C\d+" | Select-Object -Unique

# 检查警告
Select-String -Path "logs\build_output.log" -Pattern "warning C\d+" | Select-Object -Unique
```

常见错误修复：

| 错误类型 | 原因 | 解决方案 |
|---------|------|---------|
| `C1083: 无法打开包括文件` | 头文件路径错误或模块未添加 | 检查 CMakeLists.txt 中是否添加了对应的 Qt 模块 |
| `C2011: 重定义` | 类型重复定义 | 移除重复定义，使用前置声明 |
| `C2027: 使用了未定义类型` | 缺少头文件包含 | 添加正确的 `#include` |
| `C2039: 不是类的成员` | 方法未声明/实现 | 在头文件和实现文件中添加方法 |
| `Q_ENUM 宏错误` | 宏不在 Q_OBJECT 类内 | 将 Q_ENUM 移到类定义内部 |

### 6. Qt DLL 自动部署

项目已配置 `windeployqt` 自动部署。构建完成后会自动复制所有 Qt 依赖 DLL。

如需手动部署：

```powershell
& "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" `
    "build\msvc2022\Release\Release\EnhanceVision.exe" `
    --release --qmldir "qml"
```

### 7. 运行验证

启动应用程序：

```powershell
Start-Process -FilePath "build\msvc2022\Release\Release\EnhanceVision.exe"
```

## 构建优化

项目已集成以下构建优化措施：

### 1. 预编译头 (PCH)

CMakeLists.txt 中已配置预编译头，可显著减少编译时间：

```cmake
if(USE_PRECOMPILED_HEADERS AND MSVC)
    target_precompile_headers(EnhanceVision PRIVATE
        <memory>
        <string>
        <vector>
        <QObject>
        <QAbstractListModel>
        ...
    )
endif()
```

### 2. 多进程编译

MSVC 已启用 `/MP` 选项，并行编译多个源文件：

```cmake
target_compile_options(EnhanceVision PRIVATE /MP)
```

### 3. 编译缓存支持

支持 ccache/sccache 编译缓存（需安装）：

```powershell
# 安装 sccache（推荐）
cargo install sccache

# 或安装 ccache
choco install ccache
```

### 4. 增量构建

- **不要每次都删除 build 目录**：增量构建只编译修改过的文件
- **仅在遇到构建问题时清理**：`Remove-Item -Path "build\msvc2022" -Recurse -Force`

### 5. NCNN 预编译

NCNN 首次构建后会缓存，后续构建会复用。

### 构建时间参考

| 场景 | 预计时间 |
|------|---------|
| 首次完整构建 | 3-5 分钟 |
| 增量构建（修改少量文件） | 10-30 秒 |
| 仅修改 QML 文件 | 5-10 秒 |

## 日志文件

| 文件 | 内容 |
|------|------|
| `cmake_configure.log` | CMake 配置输出 |
| `build_output.log` | 编译输出 |

## 第三方库配置

| 库 | 路径 | 说明 |
|----|------|------|
| NCNN | `third_party/ncnn/` | AI 推理框架，add_subdirectory |

## 注意事项

- 日志覆盖：每次执行前删除所有旧日志
- 警告等级：MSVC `/W4`
- 递归修复：持续循环直到零警告
- Qt 路径：`E:\Qt\6.10.2\msvc2022_64`
- CMake 路径：`C:\Program Files\CMake\bin\cmake.exe`
- 构建目录：`build/msvc2022/Release/`（Release）
- Qt DLL 部署：构建时自动执行 `windeployqt`
- QML 文件：需要在 CMakeLists.txt 的 `qt_add_qml_module` 中注册
- 头文件：需要在 CMakeLists.txt 中添加到目标以支持 MOC
- **增量构建**：不要每次都删除 build 目录，仅在遇到问题时清理
