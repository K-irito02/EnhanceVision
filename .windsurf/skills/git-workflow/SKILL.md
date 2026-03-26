---
name: "git-workflow"
description: "Git 版本控制和 GitHub 协作专家 - 管理 Git Flow 工作流、语义化提交、分支管理、PR 创建、冲突解决和代码推送。当用户要求提交代码、创建分支、推送更改、解决冲突、创建 PR、查看 Git 状态、回滚更改或任何与版本控制相关的操作时，必须使用此技能。即使只是简单的 git 操作，也应主动使用此技能确保遵循项目规范。"
---

# Git Workflow & Standards

此技能确保所有版本控制操作遵循项目标准，详见 `.trae/rules/01-project-overview.md`。

## 核心原则

1. **每次提交前检查状态** - 确保了解当前更改
2. **遵循语义化提交** - 保持提交历史清晰
3. **功能分支开发** - 不直接在 main 分支工作
4. **推送前拉取** - 避免不必要的冲突
5. **代理按需配置** - 开启代理时配置，关闭代理时取消，避免连接失败

## Git Flow Strategy

| 分支 | 命名 | 用途 | 生命周期 |
|------|------|------|----------|
| `main` | main | 稳定发布分支 | 永久 |
| `develop` | develop | 开发主分支 | 永久 |
| `feature/*` | feature/功能名 | 功能开发分支 | 合并后删除 |
| `hotfix/*` | hotfix/描述 | 紧急修复分支 | 合并后删除 |
| `release/*` | release/版本号 | 发布准备分支 | 发布后删除 |

### 分支命名规范

```
feature/user-authentication    # 功能开发
feature/dark-theme-support     # 功能开发
hotfix/login-crash             # 紧急修复
hotfix/memory-leak             # 紧急修复
release/v1.2.0                 # 发布准备
```

## Semantic Commit Guidelines

Format: `<type>(scope): <subject>`

### Type 类型

| type | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(ui): add dark theme support` |
| `fix` | Bug 修复 | `fix(core): resolve memory leak in AIEngine` |
| `refactor` | 重构（不改变功能） | `refactor(core): simplify FrameCache logic` |
| `perf` | 性能优化 | `perf(ai): optimize NCNN inference pipeline` |
| `style` | 代码格式/样式 | `style(qml): format control panel layout` |
| `docs` | 文档更新 | `docs(readme): update build instructions` |
| `test` | 测试相关 | `test(core): add unit tests for ImageProcessor` |
| `chore` | 构建/工具/依赖 | `chore(cmake): update Qt dependencies` |
| `ci` | CI/CD 配置 | `ci(github): add workflow for Windows build` |
| `revert` | 回滚提交 | `revert: revert "feat(ui): add sidebar"` |

### Scope 范围

| scope | 说明 | 目录 |
|-------|------|------|
| `frontend` | QML 前端代码 | `qml/` |
| `core` | C++ 核心引擎 | `src/core/` |
| `app` | 应用层代码 | `src/app/` |
| `models` | 数据模型 | `src/models/` |
| `providers` | 图像提供者 | `src/providers/` |
| `utils` | 工具类 | `src/utils/` |
| `ui` | UI 组件/样式 | `qml/components/`, `qml/controls/` |
| `theme` | 主题/颜色 | `qml/styles/` |
| `shader` | Shader 效果 | `resources/shaders/`, `qml/shaders/` |
| `ai` | AI 推理相关 | `src/core/AIEngine.cpp` |
| `media` | 音视频处理 | `src/core/VideoProcessor.cpp` |
| `i18n` | 国际化 | `resources/i18n/` |
| `cmake` | 构建配置 | `CMakeLists.txt` |
| `deps` | 依赖管理 | `third_party/` |

### 提交消息模板

```
<type>(<scope>): <subject>

[可选的详细描述]

[可选的 Footer]
```

### 完整示例

```
feat(ui): implement dark theme with blue icons

- Add icons-dark directory with blue SVG icons for dark theme
- Update Theme.icon() to return different icon paths based on theme
- Simplify ColoredIcon component to directly display icons
- Fix control panel border to align with window edges
- Update CMakeLists.txt to include dark theme icons

Closes #123
Breaking change: Theme.icon() now requires theme parameter
```

## 必须推送的目录

### 核心代码（必须）

| 目录 | 说明 |
|------|------|
| `src/` | C++ 源码 |
| `include/EnhanceVision/` | C++ 头文件 |
| `qml/` | QML 源码 |
| `resources/` | Qt 资源 |
| `tests/` | 单元测试 |

### 构建配置（必须）

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | 主构建文件 |
| `CMakePresets.json` | CMake 预设 |

### 项目配置（必须）

| 文件/目录 | 说明 |
|-----------|------|
| `README.md` | 项目说明 |
| `.gitignore` | Git 忽略配置 |
| `.clang-format` | C++ 格式化配置 |
| `.clang-tidy` | C++ 静态分析配置 |
| `docs/` | 项目文档 |
| `.trae/` | Trae IDE 配置 |
| `.windsurf/` | Windsurf IDE 配置 |

### 不推送的目录

| 目录 | 说明 |
|------|------|
| `build/` | 构建产物 |
| `logs/` | 日志文件 |
| `third_party/` | 第三方库（gitignore） |
| `resources/models/` | AI 模型文件（大型二进制，单独分发） |
| `tests/testAssetsDirectory/video/` | 测试视频资源（大型文件） |

## 大文件处理

### 提交前检查大文件

在提交前，务必检查是否有大文件被暂存：

```powershell
# 检查暂存区中的大文件（>1MB）
git ls-files -s | ForEach-Object { 
    $size = (Get-Item $_.Split()[3] -ErrorAction SilentlyContinue).Length
    if ($size -gt 1MB) { 
        Write-Output "$([math]::Round($size/1MB, 2)) MB - $($_.Split()[3])" 
    } 
} | Sort-Object -Descending
```

### 常见大文件类型

| 文件类型 | 典型大小 | 处理方式 |
|----------|----------|----------|
| `.bin` (NCNN模型) | 10-250 MB | 加入 .gitignore |
| `.onnx` (ONNX模型) | 50-500 MB | 加入 .gitignore |
| `.pt/.pth` (PyTorch) | 50-1000 MB | 加入 .gitignore |
| `.mp4/.avi` (视频) | 变化大 | 加入 .gitignore |
| `.dll/.so` (库文件) | 1-50 MB | 加入 .gitignore |

### 撤销已暂存的大文件

如果已经 `git add` 了不应提交的大文件：

```powershell
# 从暂存区移除特定目录
git reset HEAD resources/models/
git reset HEAD tests/testAssetsDirectory/video/

# 从暂存区移除特定文件
git reset HEAD path/to/large-file.bin

# 撤销整个提交但保留更改
git reset --soft HEAD~1
```

### 模型文件分发建议

AI 模型文件应通过以下方式分发：

1. **云存储**：Google Drive、OneDrive、阿里云 OSS
2. **模型仓库**：Hugging Face、ModelScope
3. **Git LFS**：如果必须用 Git 管理，使用 Git LFS
4. **Release 附件**：GitHub Release 作为附件发布

### .gitignore 配置示例

```gitignore
# AI Models (large binary files, distribute separately)
resources/models/

# Third-party libraries
third_party/

# Test assets
tests/testAssetsDirectory/video/
tests/testAssetsDirectory/audio/

# Logs
logs/
*.log
```

## 工作流程

### 新功能开发流程

```powershell
# 1. 确保在最新的 main 分支
git checkout main
git pull origin main

# 2. 创建功能分支
git checkout -b feature/your-feature-name

# 3. 开发并提交（可多次提交）
git add <files>
git commit -m "feat(scope): your changes"

# 4. 推送到远程
git push -u origin feature/your-feature-name

# 5. 创建 Pull Request（见下方 PR 流程）
```

### 紧急修复流程

```powershell
# 1. 从 main 创建 hotfix 分支
git checkout main
git pull origin main
git checkout -b hotfix/issue-description

# 2. 修复并提交
git add <files>
git commit -m "fix(scope): resolve critical issue"

# 3. 推送并创建 PR
git push -u origin hotfix/issue-description
```

### 同步远程更改

```powershell
# 方式 1: Rebase（推荐，保持历史整洁）
git checkout feature/your-feature
git fetch origin
git rebase origin/main

# 方式 2: Merge（保留合并历史）
git checkout feature/your-feature
git fetch origin
git merge origin/main
```

## Pull Request 流程

### 使用 GitHub CLI (gh)

```powershell
# 创建 PR
gh pr create --title "feat(ui): implement dark theme" --body "描述..."

# 创建 PR（使用编辑器）
gh pr create

# 查看 PR 列表
gh pr list

# 查看 PR 详情
gh pr view <number>

# 合并 PR
gh pr merge <number> --squash  # Squash 合并（推荐）
gh pr merge <number> --merge   # 普通合并
gh pr merge <number> --rebase  # Rebase 合并

# 关闭 PR
gh pr close <number>
```

### PR 标题规范

PR 标题应遵循语义化提交格式：

```
feat(ui): implement dark theme with blue icons
fix(core): resolve memory leak in AIEngine
refactor(models): simplify SessionModel implementation
```

### PR 描述模板

```markdown
## 变更概述

简要描述此 PR 的目的和实现方式。

## 变更类型

- [ ] 新功能 (feat)
- [ ] Bug 修复 (fix)
- [ ] 重构 (refactor)
- [ ] 性能优化 (perf)
- [ ] 文档更新 (docs)
- [ ] 其他: ___

## 测试

- [ ] 已测试新功能
- [ ] 已测试回归
- [ ] 已更新测试用例

## 截图（如适用）

## 相关 Issue

Closes #xxx
```

## 冲突解决

### 检测冲突

```powershell
# 尝试合并时发现冲突
git merge origin/main
# Auto-merging <file>
# CONFLICT (content): Merge conflict in <file>
```

### 解决冲突步骤

```powershell
# 1. 查看冲突文件
git status

# 2. 打开冲突文件，冲突标记如下：
# <<<<<<< HEAD
# 当前分支的内容
# =======
# 要合并分支的内容
# >>>>>>> origin/main

# 3. 手动编辑解决冲突，保留正确内容

# 4. 标记冲突已解决
git add <resolved-file>

# 5. 完成合并
git commit  # Merge 分支
# 或
git rebase --continue  # Rebase 分支
```

### 使用 VS Code 解决冲突

VS Code 提供可视化冲突解决工具：
- 点击 "Accept Current Change" 保留当前分支
- 点击 "Accept Incoming Change" 保留合并分支
- 点击 "Accept Both Changes" 保留两者
- 手动编辑合并结果

### 放弃合并/Rebase

```powershell
# 放弃合并
git merge --abort

# 放弃 rebase
git rebase --abort
```

## 常用命令速查

### 状态查看

```powershell
git status                    # 查看当前状态
git log --oneline -10         # 查看最近 10 条提交
git log --graph --oneline     # 图形化查看提交历史
git diff                      # 查看未暂存的更改
git diff --staged             # 查看已暂存的更改
git diff <branch1> <branch2>  # 比较两个分支
```

### 撤销操作

```powershell
# 撤销工作区更改（未暂存）
git checkout -- <file>
git restore <file>            # Git 2.23+

# 撤销暂存（已 git add）
git reset HEAD <file>
git restore --staged <file>   # Git 2.23+

# 撤销最近一次提交（保留更改）
git reset --soft HEAD~1

# 撤销最近一次提交（丢弃更改）
git reset --hard HEAD~1

# 创建撤销提交（安全，不改变历史）
git revert <commit-hash>
```

### 暂存工作

```powershell
# 暂存当前工作
git stash

# 暂存并添加消息
git stash save "work in progress on feature X"

# 查看暂存列表
git stash list

# 恢复最近的暂存
git stash pop

# 恢复指定暂存
git stash apply stash@{0}

# 删除暂存
git stash drop stash@{0}
```

### 分支操作

```powershell
# 列出所有分支
git branch -a

# 创建分支
git branch <branch-name>

# 切换分支
git checkout <branch-name>
git switch <branch-name>      # Git 2.23+

# 创建并切换
git checkout -b <branch-name>
git switch -c <branch-name>   # Git 2.23+

# 删除本地分支
git branch -d <branch-name>   # 安全删除（已合并）
git branch -D <branch-name>   # 强制删除

# 删除远程分支
git push origin --delete <branch-name>
```

### 标签管理

```powershell
# 创建轻量标签
git tag v1.0.0

# 创建附注标签（推荐）
git tag -a v1.0.0 -m "Release version 1.0.0"

# 推送标签到远程
git push origin v1.0.0
git push origin --tags        # 推送所有标签

# 删除本地标签
git tag -d v1.0.0

# 删除远程标签
git push origin --delete v1.0.0
```

### 代理配置

```powershell
# 查看当前代理配置
git config --global --list | findstr proxy

# 设置代理（代理开启时）
git config --global http.proxy http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890

# 取消代理（代理关闭时）
git config --global --unset http.proxy
git config --global --unset https.proxy

# 临时使用代理（单次命令）
git -c http.proxy=http://127.0.0.1:7890 push origin main

# 测试 GitHub 连接
git ls-remote https://github.com/K-irito02/EnhanceVision.git
```

## 最佳实践

### 提交频率

- **小步提交**：每个逻辑变更一个提交
- **频繁推送**：避免大量本地积累
- **原子提交**：每个提交应该可以独立工作

### 提交消息

- 使用祈使语气：`add` 而非 `added`
- 首行不超过 50 字符
- 详细描述解释"为什么"而非"是什么"
- 引用相关 Issue：`Closes #123`

### 分支管理

- 功能分支生命周期要短
- 定期同步主分支更改
- 合并后删除功能分支
- 保持分支命名一致

### 代码审查

- PR 保持小规模（< 400 行最佳）
- 自我审查后再提交 PR
- 响应审查意见要及时
- 使用 Draft PR 标记未完成工作

### 代理管理

- **按需配置**：开启代理时立即配置，关闭代理时立即取消
- **常用端口**：Clash (7890)、V2Ray (1080)、SSR (1080)
- **优先使用 GitHub 专用代理**：避免影响其他 Git 仓库
- **推送失败先检查代理**：连接超时通常是代理配置问题

## 故障排除

### 网络代理问题

当 Git 操作失败并显示代理连接错误时（如 `Failed to connect to 127.0.0.1 port 7890`），需要根据当前网络环境配置或取消代理。

#### 检测代理问题

```powershell
# 错误示例
git push origin main
# fatal: unable to access 'https://github.com/...': Failed to connect to 127.0.0.1 port 7890

# 查看当前代理配置
git config --global --list | findstr proxy
# http.proxy=http://127.0.0.1:7890
# https.proxy=http://127.0.0.1:7890
```

#### 代理开启时（需要代理访问 GitHub）

```powershell
# 设置 HTTP/HTTPS 代理（常用代理端口：7890, 1080, 8080）
git config --global http.proxy http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890

# 仅对 GitHub 设置代理
git config --global http.https://github.com.proxy http://127.0.0.1:7890
git config --global https.https://github.com.proxy http://127.0.0.1:7890

# 设置 SOCKS5 代理
git config --global http.proxy socks5://127.0.0.1:7890
git config --global https.proxy socks5://127.0.0.1:7890
```

#### 代理未开启时（直连网络）

```powershell
# 取消全局代理设置
git config --global --unset http.proxy
git config --global --unset https.proxy

# 取消 GitHub 专用代理
git config --global --unset http.https://github.com.proxy
git config --global --unset https.https://github.com.proxy

# 验证代理已取消
git config --global --list | findstr proxy
# （无输出表示已取消）
```

#### 自动检测并配置代理

```powershell
# 检测代理是否可用并自动配置（PowerShell 脚本）
$proxyPort = 7890
$proxyUrl = "http://127.0.0.1:$proxyPort"

try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    $connect = $tcpClient.BeginConnect("127.0.0.1", $proxyPort, $null, $null)
    $wait = $connect.AsyncWaitHandle.WaitOne(2000)
    
    if ($wait -and $tcpClient.Connected) {
        Write-Host "代理可用，配置 Git 代理..."
        git config --global http.proxy $proxyUrl
        git config --global https.proxy $proxyUrl
        Write-Host "代理配置完成"
    } else {
        Write-Host "代理不可用，取消 Git 代理设置..."
        git config --global --unset http.proxy 2>$null
        git config --global --unset https.proxy 2>$null
        Write-Host "代理已取消"
    }
    $tcpClient.Close()
} catch {
    Write-Host "检测失败，取消 Git 代理设置..."
    git config --global --unset http.proxy 2>$null
    git config --global --unset https.proxy 2>$null
}
```

#### 常用代理命令速查

| 操作 | 命令 |
|------|------|
| 查看代理配置 | `git config --global --list \| findstr proxy` |
| 设置代理 | `git config --global http.proxy http://127.0.0.1:7890` |
| 取消代理 | `git config --global --unset http.proxy` |
| 临时使用代理 | `git -c http.proxy=http://127.0.0.1:7890 push` |
| 测试连接 | `git ls-remote https://github.com/用户名/仓库名.git` |

#### 代理配置文件位置

```powershell
# 全局配置文件
~/.gitconfig  # Windows: C:\Users\<用户名>\.gitconfig

# 查看配置文件内容
git config --global --list
# 或直接编辑
notepad ~/.gitconfig
```

### 推送被拒绝

```powershell
# 原因：远程有新提交
git push origin <branch>
# ! [rejected] <branch> -> <branch> (non-fast-forward)

# 解决方案 1: 拉取并合并
git pull origin <branch>
git push origin <branch>

# 解决方案 2: 拉取并 rebase（推荐）
git pull --rebase origin <branch>
git push origin <branch>
```

### 意外提交到错误分支

```powershell
# 1. 撤销提交（保留更改）
git reset --soft HEAD~1

# 2. 切换到正确分支
git checkout correct-branch

# 3. 重新提交
git commit -m "your message"
```

### 恢复已删除的分支

```powershell
# 1. 找到分支最后的提交
git reflog

# 2. 恢复分支
git checkout -b <branch-name> <commit-hash>
```

### 清理未跟踪文件

```powershell
# 预览将删除的文件
git clean -n

# 删除未跟踪的文件
git clean -f

# 删除未跟踪的文件和目录
git clean -fd
```
