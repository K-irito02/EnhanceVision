# EnhanceVision Codex Project Guide

本项目的 Codex 项目级配置采用以下结构：

- `AGENTS.md`：项目级总入口与规则索引
- `.agents/rules/`：项目规则文档，按主题拆分
- `.agents/skills/`：项目技能目录，供 Codex 在项目上下文中引用
- `.mcp.json`：项目级 MCP 配置

## 规则优先级

1. 用户本轮明确指令
2. 本文件
3. `.agents/rules/00-rule-governance.md`
4. `.agents/rules/02-architecture.md`
5. 语言与实现规范：`03`、`04`
6. 领域规则：`05` 到 `09`
7. 工具与技能规则：`.agents/rules/10-skills-and-tools.md`

## 项目事实

- 技术栈：Qt Quick (QML) + C++20 + CMake + NCNN + FFmpeg
- 主要目录：
  - `src/`：C++ 实现
  - `include/EnhanceVision/`：头文件
  - `qml/`：QML 页面、组件、控件、样式
  - `resources/`：图标、Shader、翻译、模型资源
  - `tests/`：测试
  - `docs/`：方案、笔记、说明文档

## 执行要求

- 先遵守分层边界：QML 负责展示与轻逻辑，复杂业务与调度下沉到 C++
- 代码改动后优先做构建或针对性验证
- 改动涉及文档、规则、技能时，同步更新对应文件
- 任务控制模式、启动恢复、消息卡片状态相关改动，必须同时考虑：
  - 正常处理语义
  - 暂停/继续语义
  - 应用重启后的恢复语义
  - 设置页模式切换拦截语义

## 规则索引

- 项目总览：`.agents/rules/01-project-overview.md`
- 架构边界：`.agents/rules/02-architecture.md`
- C++ 规范：`.agents/rules/03-cpp-standards.md`
- QML 规范：`.agents/rules/04-qml-standards.md`
- 构建流程：`.agents/rules/05-build-system.md`
- 国际化：`.agents/rules/06-i18n.md`
- 性能与线程：`.agents/rules/07-performance.md`
- UI 规范：`.agents/rules/08-ui-design.md`
- Shader 规范：`.agents/rules/09-shader-standards.md`
- 技能与工具：`.agents/rules/10-skills-and-tools.md`

## 技能索引

高频技能位于 `.agents/skills/`，包含：

- `communication-reflection`
- `documentation-update`
- `qt-build-and-fix`
- `qt-qml`
- `git-workflow`
- `ffmpeg-toolkit`
- `qt-compatibility-build`
- `qt-unittest-make`
- `obs-cpp-qt-patterns`
- `ui-ux-pro-max`

当任务与技能明显匹配时，优先读取对应技能的 `SKILL.md` 再执行。
