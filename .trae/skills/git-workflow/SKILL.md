---
name: "git-workflow"
description: "Manages version control and GitHub submissions following Git Flow and Semantic Commit standards. Invoke when committing code, creating branches, or pushing to GitHub."
---

# Git Workflow & Standards

此技能确保所有版本控制操作遵循项目标准，详见 `.trae/rules/01-project-overview.md` 和 `.trae/rules/05-build-system.md`。

## Git Flow Strategy

| 分支 | 命名 | 用途 |
|------|------|------|
| `main` | main | 稳定发布分支 |
| `develop` | develop | 开发主分支（如使用） |
| `feature/*` | feature/功能名 | 功能开发分支 |
| `hotfix/*` | hotfix/描述 | 紧急修复分支 |

## Semantic Commit Guidelines

Format: `<type>(scope): <subject>`

| type | 说明 | scope 示例 |
|------|------|-----------|
| `feat` | 新功能 | `frontend`, `core`, `ui`, `ai` |
| `fix` | Bug 修复 | `frontend`, `core`, `shader` |
| `refactor` | 重构 | `core`, `frontend` |
| `chore` | 构建/工具 | `cmake`, `deps`, `build` |
| `docs` | 文档 | `readme`, `rules` |
| `perf` | 性能 | `core`, `frontend` |
| `test` | 测试 | `core`, `frontend` |
| `style` | 样式/UI | `ui`, `theme`, `icons` |

## Scope 说明

| scope | 说明 |
|-------|------|
| `frontend` | QML 前端代码（qml/ 目录） |
| `core` | C++ 核心代码（src/core/） |
| `ui` | UI 组件、样式、主题 |
| `theme` | 颜色、主题、图标 |
| `icons` | 图标资源 |
| `cmake` | CMake 构建配置 |
| `deps` | 依赖管理 |
| `build` | 构建系统 |

## 必须推送到 GitHub 的项目目录

以下目录和文件必须推送到远程仓库：

### 核心代码目录

| 目录 | 说明 | 优先级 |
|------|------|--------|
| `src/` | C++ 源码（app、core、models、providers、utils） | **必须** |
| `include/EnhanceVision/` | C++ 头文件 | **必须** |
| `qml/` | QML 源码（组件、控件、页面、样式） | **必须** |
| `resources/` | Qt 资源（icons、shaders、models、i18n） | **必须** |
| `tests/` | 单元测试 | **必须** |

### 构建配置文件

| 文件 | 说明 | 优先级 |
|------|------|--------|
| `CMakeLists.txt` | 主构建文件 | **必须** |
| `CMakePresets.json` | CMake 预设 | **必须** |

### 项目文档与配置

| 文件 | 说明 | 优先级 |
|------|------|--------|
| `README.md` | 项目说明 | **必须** |
| `.gitignore` | Git 忽略配置 | **必须** |
| `.clang-format` | C++ 代码格式化配置 | **必须** |
| `.clang-tidy` | C++ 静态分析配置 | **必须** |
| `docs/` | 项目开发文档（Notes 子目录） | **必须** |

### IDE 配置目录

| 目录 | 说明 | 优先级 |
|------|------|--------|
| `.trae/rules/` | Trae IDE 规则文件 | **必须** |
| `.trae/skills/` | Trae IDE 技能文件 | **必须** |
| `.trae/agents/` | Trae IDE 代理配置 | **必须** |
| `.trae/specs/` | Trae IDE 规范文件 | **必须** |
| `.trae/documents/` | Trae IDE 文档 | **必须** |
| `.windsurf/rules/` | Windsurf IDE 规则文件 | **必须** |
| `.windsurf/skills/` | Windsurf IDE 技能文件 | **必须** |
| `.windsurfrules` | Windsurf IDE 规则入口 | **必须** |

### 不需要推送的目录

| 目录 | 说明 |
|------|------|
| `build/` | 构建产物 |
| `logs/` | 日志文件 |
| `third_party/` | 第三方库（通过 add_subdirectory 引入） |

## Usage Instructions

1. **Check Status**: Always run `git status` before starting.
2. **Branch Management**: Create a new branch for new features: `git checkout -b feature/your-feature-name`.
3. **Staging**: Use `git add <files>` to stage specific changes. Avoid `git add .` if unwanted files are present.
4. **Committing**: Use the semantic format: `git commit -m "feat(ui): add control panel collapse functionality"`.
5. **Pushing**: First push: `git push -u origin feature/your-feature-name`.
6. **Pull Requests**: Create a PR on GitHub to merge feature branch into `main`.

## Common Commands

```bash
# Initialize a new feature
git checkout main
git pull origin main
git checkout -b feature/new-feature

# Commit changes
git add qml/ resources/
git commit -m "feat(ui): implement dark theme blue icons"

# Push to remote
git push -u origin feature/new-feature
```

## Commit Message Template

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Example

```
feat(ui): implement dark theme blue icons and control panel collapse

- Add icons-dark directory with blue SVG icons for dark theme
- Update Theme.icon() to return different icon paths based on theme
- Simplify ColoredIcon component to directly display icons
- Fix control panel border to align with window edges
- Update CMakeLists.txt to include dark theme icons

Closes #123
```
