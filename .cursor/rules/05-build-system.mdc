---
alwaysApply: false
description: '构建系统 - CMake预设、构建流程、部署'
---
# 构建系统

## 构建基线

| 配置项 | 值 |
|--------|-----|
| 构建系统 | CMake 3.20+ |
| 生成器 | Visual Studio 17 2022 (x64) |
| C++ 标准 | C++20 |

## 构建预设

| 预设 | 用途 | 输出目录 |
|------|------|----------|
| `windows-msvc-2022-debug` | 开发调试 | `build/msvc2022/Debug` |
| `windows-msvc-2022-release` | 性能测试 | `build/msvc2022/Release` |

## 构建原则

1. **优先增量构建**：避免无故清理 `build/`
2. **变更 CMake 后重新配置**
3. **构建失败先修复根因**：不绕过警告/错误

## 部署原则

1. **发布前必须完成运行验证**
2. **Qt 运行时依赖通过 `windeployqt` 部署**
3. **打包步骤可复现**
