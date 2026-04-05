---
name: "git-workflow"
description: "Git 版本控制专家。当用户要求提交代码、创建分支、推送更改、解决冲突、创建 PR 或任何 Git 操作时调用。"
---

# Git Workflow

此技能确保所有版本控制操作遵循项目标准，详见 `.trae/rules/01-project-overview.md`。

## 核心原则

1. **每次提交前检查状态** - 确保了解当前更改
2. **遵循语义化提交** - 保持提交历史清晰
3. **功能分支开发** - 不直接在 main 分支工作
4. **推送前拉取** - 避免不必要的冲突
5. **代理按需配置** - 开启代理时配置，关闭代理时取消

## Git Flow Strategy

| 分支 | 命名 | 用途 | 生命周期 |
|------|------|------|----------|
| `main` | main | 稳定发布分支 | 永久 |
| `develop` | develop | 开发主分支 | 永久 |
| `feature/*` | feature/功能名 | 功能开发分支 | 合并后删除 |
| `hotfix/*` | hotfix/描述 | 紧急修复分支 | 合并后删除 |
| `release/*` | release/版本号 | 发布准备分支 | 发布后删除 |

## Semantic Commit

Format: `<type>(scope): <subject>`

### Type 类型

| type | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(ui): add dark theme support` |
| `fix` | Bug 修复 | `fix(core): resolve memory leak` |
| `refactor` | 重构 | `refactor(core): simplify logic` |
| `perf` | 性能优化 | `perf(ai): optimize inference` |
| `style` | 代码格式 | `style(qml): format layout` |
| `docs` | 文档更新 | `docs(readme): update instructions` |
| `test` | 测试相关 | `test(core): add unit tests` |
| `chore` | 构建/工具 | `chore(cmake): update deps` |

### Scope 范围

| scope | 说明 |
|-------|------|
| `frontend` | QML 前端代码 |
| `core` | C++ 核心引擎 |
| `app` | 应用层代码 |
| `models` | 数据模型 |
| `providers` | 图像提供者 |
| `utils` | 工具类 |
| `ui` | UI 组件/样式 |
| `shader` | Shader 效果 |
| `ai` | AI 推理相关 |
| `media` | 音视频处理 |
| `i18n` | 国际化 |
| `cmake` | 构建配置 |

## 必须推送的目录

| 目录 | 说明 |
|------|------|
| `src/` | C++ 源码 |
| `include/EnhanceVision/` | C++ 头文件 |
| `qml/` | QML 源码 |
| `resources/` | Qt 资源（不含 models/） |
| `tests/` | 单元测试 |
| `CMakeLists.txt` | 主构建文件 |
| `CMakePresets.json` | CMake 预设 |
| `.trae/` | Trae IDE 配置 |
| `.windsurf/` | Windsurf IDE 配置 |
| `.cursor/` | Cursor IDE 配置 |

## 禁止推送的目录

| 目录 | 原因 |
|------|------|
| `build/` | 构建产物，可重新生成 |
| `logs/` | 运行时产物 |
| `third_party/` | 第三方库，体积大 |
| `resources/models/` | AI 模型文件，大型二进制 |
| `tests/testAssetsDirectory/video/` | 测试视频资源 |
| `tests/testAssetsDirectory/audio/` | 测试音频资源 |

## 标准提交流程

```powershell
# 1. 检查状态
git status

# 2. 添加文件
git add src/ include/ qml/ resources/ tests/
git add CMakeLists.txt CMakePresets.json
git add .trae/ docs/

# 3. 检查暂存区
git diff --staged --stat

# 4. 提交
git commit -m "<type>(<scope>): <subject>"

# 5. 推送
git push origin <branch>
```

## 常用命令速查

### 状态查看

```powershell
git status                    # 查看当前状态
git log --oneline -10         # 查看最近 10 条提交
git diff                      # 查看未暂存的更改
git diff --staged             # 查看已暂存的更改
```

### 撤销操作

```powershell
git restore <file>            # 撤销工作区更改
git restore --staged <file>   # 撤销暂存
git reset --soft HEAD~1       # 撤销最近一次提交（保留更改）
git revert <commit-hash>      # 创建撤销提交
```

### 分支操作

```powershell
git branch -a                 # 列出所有分支
git switch <branch>           # 切换分支
git switch -c <branch>        # 创建并切换
git branch -d <branch>        # 删除本地分支
git push origin --delete <branch>  # 删除远程分支
```

### 代理配置

```powershell
# 查看代理配置
git config --global --list | findstr proxy

# 设置代理
git config --global http.proxy http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890

# 取消代理
git config --global --unset http.proxy
git config --global --unset https.proxy
```

## Pull Request 流程

```powershell
# 创建 PR
gh pr create --title "feat(ui): implement feature" --body "描述..."

# 查看 PR 列表
gh pr list

# 合并 PR
gh pr merge <number> --squash
```

### PR 标题规范

```
feat(ui): implement dark theme
fix(core): resolve memory leak
refactor(models): simplify implementation
```

## 冲突解决

```powershell
# 1. 查看冲突文件
git status

# 2. 编辑解决冲突（冲突标记：<<<<<<< HEAD ... ======= ... >>>>>>> branch）

# 3. 标记已解决
git add <resolved-file>

# 4. 完成合并
git commit  # 或 git rebase --continue
```

## 故障排除

### 推送被拒绝

```powershell
# 拉取并 rebase
git pull --rebase origin <branch>
git push origin <branch>
```

### 代理连接失败

```powershell
# 检测代理是否可用
$tcpClient = New-Object System.Net.Sockets.TcpClient
$connect = $tcpClient.BeginConnect("127.0.0.1", 7890, $null, $null)
if ($connect.AsyncWaitHandle.WaitOne(2000)) {
    git config --global http.proxy http://127.0.0.1:7890
} else {
    git config --global --unset http.proxy
}
```
