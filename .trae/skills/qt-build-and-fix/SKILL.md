---
name: "qt-build-and-fix"
description: "Qt 项目构建专家。当用户要求构建项目、运行程序、解决编译错误或任何构建相关问题时调用。"
---

# Qt Build and Fix

自动化 EnhanceVision 项目（Qt Quick + C++ + NCNN）的构建、运行和修复生命周期管理。

## 核心原则

| 原则 | 说明 |
|------|------|
| 增量构建优先 | 不删除 build 目录，除非必要 |
| 日志驱动诊断 | 所有操作记录日志便于分析 |
| 构建后验证 | 每次成功构建后运行程序验证 |
| 错误循环修复 | 持续修复直到零错误零警告 |
| 测试循环验证 | 启动程序后等待用户测试，弹出询问卡片确认结果 |
| 本地调试优先 | 所有调试操作在本地编辑器完成 |

## 项目架构

| 层级 | 技术 | 版本 |
|------|------|------|
| UI 框架 | Qt Quick (QML) + Qt Widgets | Qt 6.10.2 |
| 图形渲染 | Qt Scene Graph + RHI | Vulkan/D3D11/Metal |
| C++ 后端 | Core 引擎 + Controllers + Models | C++20 |
| AI 推理 | NCNN (Vulkan 加速) | latest |
| 多媒体 | FFmpeg 7.1 + Qt Multimedia | 预编译库 |
| 构建系统 | CMake | 3.20+ |
| 编译器 | MSVC 2022 | VS 17 |

## 关键路径

| 工具 | 路径 |
|------|------|
| CMake | `C:\Program Files\CMake\bin\cmake.exe` |
| Qt | `E:\Qt\6.10.2\msvc2022_64` |
| 构建目录 | `build/msvc2022/Release/` |
| 日志目录 | `logs/` |
| 可执行文件 | `build/msvc2022/Release/Release/EnhanceVision.exe` |

## 执行流程

```
清理日志 → CMake配置 → 编译项目 → 分析结果 → 运行验证 → 测试循环
```

## 日志管理规范

### 日志文件命名

| 阶段 | 日志文件 | 说明 |
|------|----------|------|
| 配置阶段 | `logs/cmake_configure.log` | CMake 配置输出 |
| 编译阶段 | `logs/build_output.log` | 编译器输出 |
| 运行阶段 | `logs/app_runtime.log` | 应用程序运行日志 |

### 日志覆盖机制

**关键原则**：每个阶段执行前**覆盖**旧日志文件，避免冗余。

```powershell
# 使用 Set-Content 或 Out-File -Force 实现覆盖
# 或使用 > 重定向符（PowerShell 中自动覆盖）
```

### 日志写入流程

```powershell
# 1. 确保日志目录存在
if (-not (Test-Path "logs")) { New-Item -ItemType Directory -Path "logs" | Out-Null }

# 2. CMake 配置阶段（覆盖写入）
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release 2>&1 | Out-File -FilePath "logs/cmake_configure.log" -Encoding UTF8 -Force

# 3. 编译阶段（覆盖写入）
& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8 2>&1 | Out-File -FilePath "logs/build_output.log" -Encoding UTF8 -Force

# 4. 运行阶段（覆盖写入）
# 使用 Python 脚本捕获应用程序输出
python launch_with_logging.py
```

## 模块索引

| 模块 | 文件 | 用途 |
|------|------|------|
| 环境配置 | `environment/setup.md` | 环境验证、依赖检查 |
| 编译错误 | `errors/compile-errors.md` | C++ 编译错误解决方案 |
| Qt错误 | `errors/qt-errors.md` | MOC、QML、信号槽错误 |
| 链接错误 | `errors/link-errors.md` | LNK 错误解决方案 |
| 运行时错误 | `errors/runtime-errors.md` | 启动崩溃、内存泄漏、性能问题 |
| 调试技巧 | `debugging/techniques.md` | 本地调试方法、日志分析 |
| 崩溃诊断 | `debugging/crash-diagnosis.md` | 异常代码、诊断流程 |

## 快速开始

```powershell
# 0. 确保日志目录存在
if (-not (Test-Path "logs")) { New-Item -ItemType Directory -Path "logs" | Out-Null }

# 1. CMake 配置阶段（覆盖写入）
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release 2>&1 | Out-File -FilePath "logs/cmake_configure.log" -Encoding UTF8 -Force

# 2. 编译阶段（覆盖写入）
& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8 2>&1 | Out-File -FilePath "logs/build_output.log" -Encoding UTF8 -Force

# 3. 运行阶段（覆盖写入）
if ($LASTEXITCODE -eq 0) {
    Get-Process | Where-Object { $_.Name -like "*EnhanceVision*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    
    # 使用 Python 脚本捕获应用程序输出到日志
    python launch_with_logging.py
}
```

**日志文件说明**：
- `cmake_configure.log` - CMake 配置阶段的完整输出
- `build_output.log` - 编译器的完整输出（错误、警告、成功信息）
- `app_runtime.log` - 应用程序运行时的输出（通过 `launch_with_logging.py` 捕获）

## 本地调试策略

### 调试信息增强
- 在关键位置添加详细日志输出
- 使用 `fflush(stdout)` 确保日志及时写入
- 添加线程ID和时间戳信息

### Windows异常捕获
- 使用 `Get-WinEvent` 查看系统崩溃日志
- 分析异常代码定位问题类型
- 检查进程状态和DLL依赖

### 日志驱动诊断
- 所有操作记录到 `logs/` 目录
- 实时分析日志中的错误和警告
- 根据日志信息进行针对性修复

## 注意事项

- **增量构建**：不要每次都删除 build 目录
- **日志覆盖**：每个阶段执行前覆盖旧日志文件，避免冗余
  - CMake 配置：`Out-File -Force` 覆盖 `cmake_configure.log`
  - 编译输出：`Out-File -Force` 覆盖 `build_output.log`
  - 应用运行：`launch_with_logging.py` 启动时清空 `app_runtime.log`
- **构建后运行**：每次构建成功后必须运行程序验证
- **关闭已运行程序**：启动新程序前必须先关闭已运行的相同程序
- **测试循环验证**：启动程序后等待用户手动测试，通过弹出询问卡片确认结果
- **本地调试优先**：所有调试操作通过添加日志、分析日志、查看系统信息等方式在本地完成

## 日志文件位置

所有日志文件位于项目根目录的 `logs/` 文件夹：

```
logs/
├── cmake_configure.log    # CMake 配置输出（每次配置覆盖）
├── build_output.log       # 编译器输出（每次构建覆盖）
└── app_runtime.log        # 应用运行日志（每次启动覆盖）
```
