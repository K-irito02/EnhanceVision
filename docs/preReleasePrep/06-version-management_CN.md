# 版本号管理规范

[English](06-version-management_EN.md) | 简体中文

本文档说明 EnhanceVision 的版本号管理规范，遵循语义化版本控制 (Semantic Versioning)。

## 📋 目录

- [版本号格式](#版本号格式)
- [版本号递增规则](#版本号递增规则)
- [发布周期](#发布周期)
- [版本分支策略](#版本分支策略)
- [变更日志管理](#变更日志管理)

---

## 版本号格式

### 标准格式

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

### 组成部分

| 部分 | 说明 | 示例 |
|------|------|------|
| MAJOR | 主版本号 | 0, 1, 2 |
| MINOR | 次版本号 | 1, 2, 3 |
| PATCH | 修订号 | 0, 1, 2 |
| PRERELEASE | 预发布标识（可选） | alpha, beta, rc |
| BUILD | 构建元数据（可选） | build.123 |

### 示例

| 版本号 | 说明 |
|--------|------|
| `0.1.0` | 首次发布 |
| `0.1.1` | Bug 修复版本 |
| `0.2.0` | 新功能版本 |
| `1.0.0` | 稳定版本 |
| `1.0.0-alpha` | 内部测试版本 |
| `1.0.0-beta.1` | 公开测试版本 |
| `1.0.0-rc.1` | 发布候选版本 |

---

## 版本号递增规则

### MAJOR (主版本号)

**递增条件：**
- 不兼容的 API 修改
- 重大架构变更
- 移除重要功能

**示例：**
- 0.x.x → 1.0.0：首次稳定发布
- 1.x.x → 2.0.0：架构重构，不兼容旧版本

### MINOR (次版本号)

**递增条件：**
- 向下兼容的功能新增
- 重要功能改进
- 性能优化

**示例：**
- 0.1.0 → 0.2.0：添加新的 AI 模型支持
- 1.0.0 → 1.1.0：添加批量导出功能

### PATCH (修订号)

**递增条件：**
- 向下兼容的问题修正
- 小幅改进
- 文档更新

**示例：**
- 0.1.0 → 0.1.1：修复视频导出崩溃问题
- 1.0.0 → 1.0.1：修复内存泄漏

---

## 发布周期

### 版本阶段

```
开发阶段 → 测试阶段 → 发布阶段
    │           │           │
    ▼           ▼           ▼
  alpha      beta/rc     stable
```

### 预发布版本

| 标识 | 说明 | 目标用户 |
|------|------|----------|
| `alpha` | 内部测试，功能不完整 | 开发团队 |
| `beta` | 公开测试，功能基本完整 | 测试用户 |
| `rc` | 发布候选，预期最终版本 | 早期用户 |

### 版本生命周期

| 版本类型 | 支持周期 | 更新类型 |
|----------|----------|----------|
| 主版本 (MAJOR) | 2 年 | 安全更新、Bug 修复 |
| 次版本 (MINOR) | 1 年 | Bug 修复 |
| 修订版 (PATCH) | 6 个月 | 紧急修复 |

---

## 版本分支策略

### 分支模型

```
main (稳定分支)
  │
  ├── develop (开发分支)
  │     │
  │     ├── feature/xxx (功能分支)
  │     ├── fix/xxx (修复分支)
  │     └── refactor/xxx (重构分支)
  │
  └── release/x.y (发布分支)
```

### 分支说明

| 分支 | 用途 | 合并目标 |
|------|------|----------|
| `main` | 稳定发布版本 | - |
| `develop` | 开发中的代码 | `main` |
| `feature/*` | 新功能开发 | `develop` |
| `fix/*` | Bug 修复 | `develop` 或 `release/*` |
| `release/*` | 发布准备 | `main` 和 `develop` |

### 发布流程

```
1. 从 develop 创建 release/x.y 分支
2. 在 release/x.y 分支进行测试和修复
3. 测试通过后合并到 main
4. 在 main 上打标签 v.x.y.z
5. 发布到 GitHub Releases
6. 合并修复到 develop
```

---

## 变更日志管理

### CHANGELOG.md 格式

遵循 [Keep a Changelog](https://keepachangelog.com/) 规范：

```markdown
# 变更日志

## [Unreleased]

### Added
- 新功能描述

### Changed
- 变更描述

### Deprecated
- 即将废弃的功能

### Removed
- 已移除的功能

### Fixed
- 修复描述

### Security
- 安全相关修复

---

## [1.0.0] - 2025-01-15

### Added
- 首次稳定发布

---

## [0.2.0] - 2025-01-01

### Added
- 添加批量导出功能

### Fixed
- 修复视频处理崩溃问题
```

### 变更类型说明

| 类型 | 说明 |
|------|------|
| `Added` | 新功能 |
| `Changed` | 现有功能的变更 |
| `Deprecated` | 即将废弃的功能 |
| `Removed` | 已移除的功能 |
| `Fixed` | Bug 修复 |
| `Security` | 安全相关修复 |

---

## 版本号更新流程

### 1. 更新 CMakeLists.txt

```cmake
# 更新版本号
project(EnhanceVision VERSION 0.2.0 LANGUAGES CXX)
```

### 2. 更新 CHANGELOG.md

```markdown
## [0.2.0] - 2025-02-01

### Added
- 添加批量导出功能
- 添加新的 AI 模型支持

### Fixed
- 修复视频处理崩溃问题
```

### 3. 创建 Git 标签

```powershell
git tag -a v0.2.0 -m "Release v0.2.0"
git push origin v0.2.0
```

### 4. 发布到 GitHub

参考 [04-github-release-guide.md](04-github-release-guide.md)。

---

## 预发布版本管理

### 创建预发布版本

```powershell
# 创建 alpha 版本
git tag -a v1.0.0-alpha.1 -m "Alpha release for v1.0.0"
git push origin v1.0.0-alpha.1

# 创建 beta 版本
git tag -a v1.0.0-beta.1 -m "Beta release for v1.0.0"
git push origin v1.0.0-beta.1

# 创建 rc 版本
git tag -a v1.0.0-rc.1 -m "Release candidate for v1.0.0"
git push origin v1.0.0-rc.1
```

### 预发布版本命名规则

| 阶段 | 格式 | 示例 |
|------|------|------|
| Alpha | `vX.Y.Z-alpha.N` | v1.0.0-alpha.1 |
| Beta | `vX.Y.Z-beta.N` | v1.0.0-beta.1 |
| RC | `vX.Y.Z-rc.N` | v1.0.0-rc.1 |

---

## 版本号检查脚本

创建 `scripts\check-version.ps1`:

```powershell
param(
    [string]$ExpectedVersion
)

# 从 CMakeLists.txt 读取版本号
$cmakeContent = Get-Content "CMakeLists.txt" -Raw
if ($cmakeContent -match 'project\(EnhanceVision VERSION (\d+\.\d+\.\d+)') {
    $cmakeVersion = $matches[1]
} else {
    Write-Error "无法从 CMakeLists.txt 读取版本号"
    exit 1
}

# 从 CHANGELOG.md 读取最新版本
$changelogContent = Get-Content "CHANGELOG.md" -Raw
if ($changelogContent -match '## \[(\d+\.\d+\.\d+)\]') {
    $changelogVersion = $matches[1]
} else {
    Write-Error "无法从 CHANGELOG.md 读取版本号"
    exit 1
}

# 检查版本一致性
Write-Host "CMakeLists.txt 版本: $cmakeVersion"
Write-Host "CHANGELOG.md 版本: $changelogVersion"

if ($cmakeVersion -ne $changelogVersion) {
    Write-Error "版本号不一致！"
    exit 1
}

if ($ExpectedVersion -and $cmakeVersion -ne $ExpectedVersion) {
    Write-Error "版本号与预期不符！预期: $ExpectedVersion, 实际: $cmakeVersion"
    exit 1
}

Write-Host "✅ 版本号检查通过: $cmakeVersion" -ForegroundColor Green
```

运行检查：
```powershell
.\scripts\check-version.ps1 -ExpectedVersion "0.1.0"
```

---

## 版本号规划示例

### 0.x.x 系列（开发阶段）

| 版本 | 计划内容 | 预计时间 |
|------|----------|----------|
| 0.1.0 | 首次发布 | 2025-01 |
| 0.1.1 | Bug 修复 | 2025-02 |
| 0.2.0 | 新 AI 模型 | 2025-03 |
| 0.3.0 | 视频处理优化 | 2025-04 |

### 1.x.x 系列（稳定阶段）

| 版本 | 计划内容 | 预计时间 |
|------|----------|----------|
| 1.0.0 | 稳定版本 | 2025-06 |
| 1.1.0 | 批量处理增强 | 2025-07 |
| 1.2.0 | 插件系统 | 2025-09 |

---

## 常见问题

### Q: 什么时候从 0.x.x 升级到 1.0.0？

A: 当软件达到以下条件时：
- API 稳定
- 核心功能完整
- 文档完善
- 经过充分测试

### Q: 如何处理紧急修复？

A: 从最新的稳定版本创建 hotfix 分支：
```powershell
git checkout -b hotfix/1.0.1 v1.0.0
# 修复问题
git commit -m "fix: critical bug"
git tag -a v1.0.1 -m "Hotfix release"
git push origin v1.0.1
```

### Q: 预发布版本是否需要 CHANGELOG？

A: 需要。预发布版本的变更记录在 `[Unreleased]` 区段中。

---

## 参考资料

- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)
- [Git Flow](https://nvie.com/posts/a-successful-git-branching-model/)

---

*最后更新: 2025年*
