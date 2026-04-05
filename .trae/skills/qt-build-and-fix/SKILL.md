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
# 1. 配置
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release 2>&1 | Tee-Object -FilePath "logs/cmake_configure.log"

# 2. 构建
& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8 2>&1 | Tee-Object -FilePath "logs/build_output.log"

# 3. 运行
if ($LASTEXITCODE -eq 0) {
    Get-Process | Where-Object { $_.Name -like "*EnhanceVision*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    Start-Process -FilePath "build\msvc2022\Release\Release\EnhanceVision.exe"
}
```

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
- **日志覆盖**：每次执行前删除所有旧日志
- **构建后运行**：每次构建成功后必须运行程序验证
- **关闭已运行程序**：启动新程序前必须先关闭已运行的相同程序
- **测试循环验证**：启动程序后等待用户手动测试，通过弹出询问卡片确认结果
- **本地调试优先**：所有调试操作通过添加日志、分析日志、查看系统信息等方式在本地完成
