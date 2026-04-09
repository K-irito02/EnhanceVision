# GitHub Releases 发布流程

[English](04-github-release-guide_EN.md) | 简体中文

本文档详细说明如何通过 GitHub Releases 发布 EnhanceVision。

## 📋 目录

- [发布前准备](#发布前准备)
- [创建 Git 标签](#创建-git-标签)
- [创建 GitHub Release](#创建-github-release)
- [发布后验证](#发布后验证)
- [更新项目主页](#更新项目主页)

---

## 发布前准备

### 1. 确认发布内容

- [ ] Release 版本已构建
- [ ] 安装程序已生成
- [ ] 便携版已生成
- [ ] 所有测试通过
- [ ] CHANGELOG.md 已更新
- [ ] README.md 已更新

### 2. 准备发布文件

```powershell
# 确认文件存在
$version = "0.1.0"

Test-Path "package\EnhanceVision-v$version-windows-x64-installer.exe"
Test-Path "package\EnhanceVision-v$version-windows-x64-portable.zip"
```

### 3. 准备 Release Notes

编辑 `docs/releases/v0.1.0.md`:

```markdown
# EnhanceVision v0.1.0

首次发布！

## ✨ 新功能

### 双模式处理
- **Shader 模式**: 14 种实时参数调整（曝光、亮度、对比度、饱和度、色相、伽马、色温、色调、高光、阴影、晕影、模糊、降噪、锐化）
- **AI 推理模式**: Real-ESRGAN 超分辨率增强（4x 放大）

### 并发调度系统
- 多级优先级任务队列
- AI 引擎池支持 2 个并发推理任务
- 死锁检测与自动恢复机制
- 任务超时监控与重试策略

### 性能优化
- 零拷贝图像传输
- GPU 加速渲染
- GPU OOM 自动恢复

### 现代化 UI
- Qt Quick (QML) 声明式 UI
- 深色/浅色主题
- 中英双语
- 会话式工作流

## 📥 下载

| 文件 | 说明 | 大小 |
|------|------|------|
| [EnhanceVision-v0.1.0-windows-x64-installer.exe]() | Windows 安装程序 | ~80 MB |
| [EnhanceVision-v0.1.0-windows-x64-portable.zip]() | Windows 便携版 | ~100 MB |

## 📋 系统要求

- Windows 10/11 64-bit
- Vulkan 兼容 GPU（可选，用于 AI 加速）
- 4GB+ RAM
- 500MB+ 磁盘空间

## 🚀 快速开始

### 安装版
1. 下载安装程序
2. 双击运行安装程序
3. 按向导完成安装
4. 从开始菜单启动 EnhanceVision

### 便携版
1. 下载 ZIP 文件
2. 解压到任意目录
3. 双击 EnhanceVision.exe 启动

## 🔧 已知问题

- 暂无

## 📝 更新日志

详见 [CHANGELOG.md](CHANGELOG.md)

## 🙏 致谢

感谢以下开源项目：
- [Qt](https://www.qt.io/) - UI 框架
- [NCNN](https://github.com/Tencent/ncnn) - AI 推理引擎
- [FFmpeg](https://ffmpeg.org/) - 多媒体处理
- [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN) - 超分辨率模型
```

---

## 创建 Git 标签

### 1. 确认代码状态

```powershell
# 检查工作区状态
git status

# 确保所有更改已提交
git add .
git commit -m "chore: prepare for v0.1.0 release"
```

### 2. 创建标签

```powershell
# 创建带注释的标签
git tag -a v0.1.0 -m "Release v0.1.0: First public release"

# 查看标签
git tag -l

# 查看标签详情
git show v0.1.0
```

### 3. 推送标签

```powershell
# 推送标签到远程
git push origin v0.1.0

# 或推送所有标签
git push origin --tags
```

---

## 创建 GitHub Release

### 方法一：通过 GitHub 网页界面

1. **访问仓库**
   - 打开 https://github.com/K-irito02/EnhanceVision
   - 点击右侧 "Releases"
   - 点击 "Draft a new release"

2. **填写发布信息**
   - **Choose a tag**: 选择 `v0.1.0`（或创建新标签）
   - **Release title**: `EnhanceVision v0.1.0`
   - **Describe this release**: 粘贴 Release Notes

3. **上传文件**
   - 拖拽或选择文件：
     - `EnhanceVision-v0.1.0-windows-x64-installer.exe`
     - `EnhanceVision-v0.1.0-windows-x64-portable.zip`

4. **发布选项**
   - [ ] Set as the latest release（勾选）
   - [ ] Set as a pre-release（首次发布可选）
   - [ ] Save as draft（先保存草稿）

5. **发布**
   - 点击 "Publish release"

### 方法二：使用 GitHub CLI

```powershell
# 安装 GitHub CLI
# winget install GitHub.cli

# 登录
gh auth login

# 创建 Release
gh release create v0.1.0 `
    package\EnhanceVision-v0.1.0-windows-x64-installer.exe `
    package\EnhanceVision-v0.1.0-windows-x64-portable.zip `
    --title "EnhanceVision v0.1.0" `
    --notes-file docs\releases\v0.1.0.md `
    --latest
```

### 方法三：使用 GitHub API

```powershell
# 需要设置 GitHub Token
$token = "YOUR_GITHUB_TOKEN"
$owner = "K-irito02"
$repo = "EnhanceVision"
$version = "0.1.0"

# 创建 Release
$headers = @{
    "Authorization" = "token $token"
    "Accept" = "application/vnd.github.v3+json"
}

$body = @{
    tag_name = "v$version"
    name = "EnhanceVision v$version"
    body = Get-Content "docs\releases\v$version.md" -Raw
    draft = $false
    prerelease = $false
} | ConvertTo-Json

$response = Invoke-RestMethod `
    -Method Post `
    -Uri "https://api.github.com/repos/$owner/$repo/releases" `
    -Headers $headers `
    -Body $body

# 上传文件
$uploadUrl = $response.upload_url -replace '\{\?name,label\}', "?name="

# 上传安装程序
$installerPath = "package\EnhanceVision-v$version-windows-x64-installer.exe"
$installerName = Split-Path $installerPath -Leaf
Invoke-RestMethod `
    -Method Post `
    -Uri "$uploadUrl$installerName" `
    -Headers @{
        "Authorization" = "token $token"
        "Content-Type" = "application/octet-stream"
    } `
    -InFile $installerPath

# 上传便携版
$portablePath = "package\EnhanceVision-v$version-windows-x64-portable.zip"
$portableName = Split-Path $portablePath -Leaf
Invoke-RestMethod `
    -Method Post `
    -Uri "$uploadUrl$portableName" `
    -Headers @{
        "Authorization" = "token $token"
        "Content-Type" = "application/octet-stream"
    } `
    -InFile $portablePath
```

---

## 发布后验证

### 1. 检查 Release 页面

访问 https://github.com/K-irito02/EnhanceVision/releases/tag/v0.1.0

确认：
- [ ] Release 标题正确
- [ ] Release Notes 显示正常
- [ ] 文件已上传
- [ ] 文件大小正确
- [ ] 下载链接可用

### 2. 测试下载

```powershell
# 下载并验证文件
$version = "0.1.0"
$url = "https://github.com/K-irito02/EnhanceVision/releases/download/v$version"

# 下载安装程序
Invoke-WebRequest -Uri "$url/EnhanceVision-v$version-windows-x64-installer.exe" -OutFile "test-installer.exe"

# 下载便携版
Invoke-WebRequest -Uri "$url/EnhanceVision-v$version-windows-x64-portable.zip" -OutFile "test-portable.zip"

# 验证文件大小
(Get-Item "test-installer.exe").Length / 1MB
(Get-Item "test-portable.zip").Length / 1MB
```

### 3. 测试安装

```powershell
# 运行安装程序
.\test-installer.exe

# 解压便携版
Expand-Archive -Path "test-portable.zip" -DestinationPath "test-portable"

# 运行便携版
.\test-portable\EnhanceVision-v0.1.0-windows-x64\EnhanceVision.exe
```

---

## 更新项目主页

### 1. 更新 README.md

在 README.md 中添加下载链接：

```markdown
## 下载安装

### Windows

| 版本 | 文件 | 说明 |
|------|------|------|
| v0.1.0 | [安装程序](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-installer.exe) | 标准安装 |
| v0.1.0 | [便携版](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-portable.zip) | 解压即用 |

### 系统要求

- Windows 10/11 64-bit
- Vulkan 兼容 GPU（可选，用于 AI 加速）
- 4GB+ RAM
- 500MB+ 磁盘空间
```

### 2. 更新 About 部分

在仓库设置中更新：
- Description: `基于 Qt 6 + QML 的桌面端图像与视频画质增强工具`
- Website: 项目主页 URL
- Topics: `qt`, `qml`, `image-processing`, `video-enhancement`, `ai`, `ncnn`, `real-esrgan`

### 3. 创建发布公告

在 GitHub Discussions 中发布公告（如已启用）：

```markdown
# 🎉 EnhanceVision v0.1.0 发布！

我们很高兴地宣布 EnhanceVision 首个公开版本发布！

## 主要特性

- 双模式处理：Shader 实时滤镜 + AI 超分辨率
- 并发调度系统
- GPU 加速
- 现代化 UI

## 下载

[前往 Releases 页面下载](https://github.com/K-irito02/EnhanceVision/releases/tag/v0.1.0)

## 反馈

如有问题或建议，请在 [Issues](https://github.com/K-irito02/EnhanceVision/issues) 中反馈。

感谢您的支持！
```

---

## 发布检查清单

发布完成后，确认以下事项：

- [ ] Git 标签已创建并推送
- [ ] GitHub Release 已发布
- [ ] 文件已上传并可下载
- [ ] README.md 已更新下载链接
- [ ] CHANGELOG.md 已更新
- [ ] 项目主页已更新
- [ ] 发布公告已发布（如适用）
- [ ] 社交媒体已发布（如适用）

---

## 下个版本发布

发布完成后，准备下个版本开发：

1. 更新版本号（如 0.2.0 或 0.1.1）
2. 在 CHANGELOG.md 中添加 `[Unreleased]` 区段
3. 创建新的开发分支（如需要）

---

*最后更新: 2025年*
