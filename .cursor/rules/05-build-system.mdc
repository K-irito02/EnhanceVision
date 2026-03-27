---
alwaysApply: false
description: '构建系统 - CMake预设、构建流程、发布部署与构建可复现性'
---
# 构建系统

## 构建基线

| 配置项 | 值 |
|--------|-----|
| 构建系统 | CMake 3.20+ |
| 生成器 | Visual Studio 17 2022 (x64) |
| C++ 标准 | C++20 |
| 预设文件 | `CMakePresets.json` |

## 日常流程

```powershell
# Debug 构建（开发调试）
cmake --preset windows-msvc-2022-debug
cmake --build build/msvc2022/Debug --config Debug -j 8

# Release 构建（性能测试）
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

## 运行

```powershell
.\build\msvc2022\Debug\Debug\EnhanceVision.exe
# 或 Release 版本
.\build\msvc2022\Release\Release\EnhanceVision.exe
```

## 构建原则

- ✅ 优先增量构建，避免无故清理 `build/`
- ✅ 变更 CMake 后必须重新配置
- ✅ 构建失败先修复根因，不绕过警告/错误

## 部署原则

- 发布前必须完成可运行验证
- Qt 运行时依赖通过 `windeployqt` 部署
- 打包路径、参数、步骤可复现

## 构建优化

| 优化项 | 说明 |
|--------|------|
| 预编译头 | 启用 `USE_PRECOMPILED_HEADERS` |
| 多进程编译 | MSVC `/MP` 选项 |
| 编译缓存 | 支持 ccache / sccache |
| Release优化 | `/O2 /GL` + `/LTCG` |

## 本文件边界

- 仅定义"构建与部署"
- 第三方依赖治理见项目文档
