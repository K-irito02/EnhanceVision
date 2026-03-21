---
alwaysApply: true
description: '项目技能和 MCP 服务指南 - 定义可用的技能和 MCP 服务及其使用场景'
---
# 项目技能和 MCP 服务指南

## 概述

本文档说明当前项目可用的技能和 MCP 服务，以及它们的使用场景和最佳实践。

## 可用技能 (Skills)

### 1. qt-build-and-fix
**作用**: 自动化构建、运行和修复 EnhanceVision 项目
- **使用场景**: 当需要编译项目、运行测试、检查代码质量或解决编译问题时
- **触发方式**: 用户要求构建项目、运行程序、修复编译错误等
- **功能**: 
  - CMake 项目构建
  - 程序运行和测试
  - 编译错误自动修复
  - 代码质量检查
  - 环境验证
  - 常见错误解决方案

### 2. git-workflow
**作用**: Git 版本控制和 GitHub 协作专家
- **使用场景**: 版本控制、分支管理、PR 创建、冲突解决
- **功能**:
  - Git Flow 工作流管理
  - 语义化提交规范
  - 分支创建和管理
  - Pull Request 创建和合并
  - 冲突解决
  - GitHub CLI (gh) 使用
  - 回滚和撤销操作

### 3. documentation-update
**作用**: 文档更新和维护专家
- **使用场景**: 功能开发完成后更新文档、API 文档、用户手册
- **功能**:
  - 开发笔记创建和更新
  - 变更日志维护
  - README 文件更新
  - API 文档生成
  - 国际化文件更新
  - 规则文件同步

### 4. qt-architecture
**作用**: Qt 应用程序架构设计和项目结构规划
- **使用场景**: 设计 Qt 应用架构、组织项目结构、选择设计模式
- **功能**:
  - MVC/MVP 模式选择
  - 项目目录结构规划
  - QApplication 设置
  - 组件分离和依赖管理

### 5. qt-layouts
**作用**: Qt 布局管理和组件排列
- **使用场景**: 布局管理、控件排列、响应式设计
- **功能**:
  - 布局管理器使用
  - 控件尺寸和位置控制
  - 响应式布局设计
  - 布局问题调试

### 6. qt-debugging
**作用**: Qt 应用程序调试和问题诊断
- **使用场景**: 程序崩溃、性能问题、逻辑错误调试
- **功能**:
  - 调试技术指导
  - 日志分析
  - 内存泄漏检测
  - 性能优化建议

### 7. qt-qml
**作用**: QML 和 Qt Quick 开发指导
- **使用场景**: QML 组件开发、属性绑定、信号处理
- **功能**:
  - QML 最佳实践
  - 组件设计模式
  - 属性绑定和信号槽
  - C++ 与 QML 交互

### 8. code-quality
**作用**: 代码质量检测和改进
- **使用场景**: 静态分析、格式化、代码检查
- **功能**:
  - 静态分析
  - 代码格式化
  - 最佳实践检查
  - 代码规范验证

### 9. cmake
**作用**: CMake 构建系统专家
- **使用场景**: CMake 配置、依赖管理、跨平台构建
- **功能**:
  - 项目配置
  - 目标管理
  - 依赖管理
  - 预设配置

## 技能使用指南

### 何时使用技能

| 场景 | 推荐技能 |
|------|----------|
| 项目构建问题 | `qt-build-and-fix` |
| 版本控制操作 | `git-workflow` |
| 文档更新 | `documentation-update` |
| 架构设计 | `qt-architecture` |
| 布局问题 | `qt-layouts` |
| 调试问题 | `qt-debugging` |
| QML 开发 | `qt-qml` |
| 代码质量 | `code-quality` |
| CMake 配置 | `cmake` |

### 技能组合使用

复杂问题可以组合多个技能：

1. **新功能开发**：
   - `qt-architecture` → 设计架构
   - `qt-qml` → 实现 QML 界面
   - `qt-build-and-fix` → 构建验证
   - `documentation-update` → 更新文档

2. **Bug 修复**：
   - `qt-debugging` → 定位问题
   - `qt-build-and-fix` → 构建验证
   - `git-workflow` → 提交修复
   - `documentation-update` → 更新变更日志

3. **发布流程**：
   - `qt-build-and-fix` → Release 构建
   - `code-quality` → 代码检查
   - `git-workflow` → 创建标签和 PR
   - `documentation-update` → 更新文档

## 可用 MCP 服务

### 1. Context7 MCP Server
**服务**: `mcp_context7_resolve-library-id`, `mcp_context7_query-docs`
- **作用**: 获取最新的库文档和代码示例
- **使用场景**:
  - 查询 Qt、QML、JavaScript 等库的文档
  - 获取代码示例和最佳实践
  - 解决 API 使用问题

### 2. GitHub MCP Server
**服务**: `mcp_GitHub_*` 系列
- **作用**: GitHub 仓库操作和协作
- **使用场景**:
  - 仓库管理
  - Issue 和 PR 管理
  - 代码搜索
  - 分支管理

### 3. Time MCP Server
**服务**: `mcp_Time_get_current_time`, `mcp_Time_convert_time`
- **作用**: 时间获取和转换
- **使用场景**:
  - 获取当前时间
  - 时区转换
  - 时间格式化

### 4. Sequential Thinking MCP Server
**服务**: `mcp_Sequential_Thinking_sequentialthinking`
- **作用**: 复杂问题的逐步推理
- **使用场景**:
  - 复杂问题分析
  - 多步骤决策
  - 方案评估

### 5. shadcn/ui MCP Server
**服务**: `mcp_shadcn-ui_*` 系列
- **作用**: shadcn/ui 组件库管理
- **使用场景**:
  - 添加 UI 组件到项目
  - 查看组件示例
  - UI 组件配置

## MCP 服务使用指南

### 何时使用 MCP 服务

| 场景 | 推荐服务 |
|------|----------|
| 需要外部文档 | Context7 |
| GitHub 操作 | GitHub MCP |
| 时间相关 | Time MCP |
| 复杂推理 | Sequential Thinking |
| UI 组件 | shadcn/ui |

### 最佳实践

1. **优先使用项目特定技能**: Qt 相关问题优先使用 qt-* 系列技能
2. **合理使用 MCP 服务**: 需要外部资源时才使用 MCP
3. **技能组合使用**: 复杂问题可以组合多个技能
4. **及时更新文档**: 完成功能后使用 `documentation-update` 更新文档

## 注意事项

- 技能选择应基于具体问题类型
- MCP 服务使用需要网络连接
- 某些技能可能有特定前置条件
- 使用技能前应明确问题范围
