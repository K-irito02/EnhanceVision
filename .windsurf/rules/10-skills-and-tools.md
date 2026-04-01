---
alwaysApply: true
description: '项目技能和工具指南 - 技能选择、工作流、执行原则'
trigger: always_on
---
# 项目技能和工具指南

## 高频技能（默认优先）

| 技能 | 用途 |
|------|------|
| `communication-reflection` | 技能使用总则，对话开始必须调用 |
| `qt-build-and-fix` | 构建、运行、编译错误修复 |
| `qt-qml` | QML/Qt Quick UI 架构与组件 |
| `git-workflow` | 分支、提交、推送、回滚 |
| `documentation-update` | 文档、规则、笔记同步更新 |

## 条件触发技能

| 技能 | 触发条件 |
|------|----------|
| `qt-unittest-make` | 需要补全/生成 Qt 单元测试 |
| `ffmpeg-toolkit` | 涉及视频/音频处理与导出 |
| `qt-compatibility-build` | 涉及 Qt 版本兼容或迁移 |
| `github_actions_cicd` | 需要建设或修复 CI 流水线 |
| `gitignore-bypass` | 需要访问被 .gitignore 排除的文件 |

## 典型工作流

```
对话开始：communication-reflection → 技能检查 → 执行
新功能：架构设计 → 开发实现 → 构建验证 → 文档更新
Bug修复：问题定位 → 修复 → 回归验证 → 文档更新
测试补全：类/模块分析 → 单测生成 → 构建验证
```

## 执行原则

1. **技能优先**：对话开始必须先调用 `communication-reflection`
2. **优先项目内既有规范与流程**
3. **每次代码变更后执行构建验证**
4. **变更完成后同步更新相关文档**

## 技能使用纪律

1. **1% 原则**：只要认为有 1% 可能性技能适用，必须调用检查
2. **禁止理性化**：不要为跳过技能检查找借口
3. **用户指令优先**：用户明确指令高于技能建议
4. **技能进化**：每次调用都读取最新版本，不依赖记忆
