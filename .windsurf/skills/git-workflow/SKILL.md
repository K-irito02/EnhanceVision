---
name: "git-workflow"
description: "Manages version control and GitHub submissions following Git Flow and Semantic Commit standards. Invoke when committing code, creating branches, or pushing to GitHub."
---

# Git Workflow & Standards

此技能确保所有版本控制操作遵循项目标准，详见 `.trae/rules/06-git-and-i18n.md`。

## Git Flow Strategy

| 分支 | 命名 | 用途 |
|------|------|------|
| `master` | master | 稳定发布 |
| `develop` | develop | 开发主分支 |
| `feature/*` | feature/功能名 | 功能开发 |
| `hotfix/*` | hotfix/描述 | 紧急修复 |

## Semantic Commit Guidelines

Format: `<type>(scope): <subject>`

| type | 说明 | scope 示例 |
|------|------|-----------|
| `feat` | 新功能 | `frontend`, `bridge`, `core`, `ai` |
| `fix` | Bug 修复 | `frontend`, `bridge`, `shader` |
| `refactor` | 重构 | `core`, `frontend` |
| `chore` | 构建/工具 | `cmake`, `vite`, `deps` |
| `docs` | 文档 | `readme`, `rules` |
| `perf` | 性能 | `core`, `frontend` |
| `test` | 测试 | `core`, `bridge` |

## 必须推送到 GitHub 的项目目录

以下目录和文件必须推送到远程仓库：

### 核心代码目录

| 目录 | 说明 | 优先级 |
|------|------|--------|
| `src/` | C++ 源码（bridge、core、rhi、utils） | **必须** |
| `include/EnhanceVision/` | C++ 头文件 | **必须** |
| `frontend/src/` | React 前端源码 | **必须** |
| `resources/` | Qt 资源（qrc、shaders、models、i18n） | **必须** |
| `tests/` | 单元测试 | **必须** |

### 构建配置文件

| 文件 | 说明 | 优先级 |
|------|------|--------|
| `CMakeLists.txt` | 主构建文件 | **必须** |
| `CMakePresets.json` | CMake 预设 | **必须** |
| `cmake/` | CMake 脚本目录 | **必须** |
| `frontend/package.json` | 前端依赖配置 | **必须** |
| `frontend/package-lock.json` | 前端依赖锁定 | **必须** |
| `frontend/vite.config.ts` | Vite 配置 | **必须** |
| `frontend/tsconfig.json` | TypeScript 配置 | **必须** |
| `frontend/index.html` | 入口 HTML | **必须** |
| `frontend/postcss.config.mjs` | PostCSS 配置 | **必须** |

### 项目文档与配置

| 文件 | 说明 | 优先级 |
|------|------|--------|
| `README.md` | 项目说明 | **必须** |
| `.gitignore` | Git 忽略配置 | **必须** |
| `.clang-format` | C++ 代码格式化配置 | **必须** |
| `.clang-tidy` | C++ 静态分析配置 | **必须** |

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
| `frontend/node_modules/` | 前端依赖 |
| `frontend/dist/` | 前端构建产物 |
| `third_party/` | 第三方库（太大） |

## Usage Instructions

1. **Check Status**: Always run `git status` before starting.
2. **Branch Management**: Create a new branch for new features: `git checkout -b feature/your-feature-name`.
3. **Staging**: Use `git add <files>` to stage specific changes. Avoid `git add .` if unwanted files are present.
4. **Committing**: Use the semantic format: `git commit -m "feat(bridge): add SessionManager QWebChannel"`.
5. **Pushing**: First push: `git push -u origin feature/your-feature-name`.
6. **Pull Requests**: Create a PR on GitHub to merge feature branch into `develop`.

## Common Commands

```bash
# Initialize a new feature
git checkout develop
git pull origin develop
git checkout -b feature/new-feature

# Commit changes (scope reflects new architecture)
git add src/bridge/ frontend/src/
git commit -m "feat(bridge): expose FileManager via QWebChannel"

# Push to remote
git push -u origin feature/new-feature
```
