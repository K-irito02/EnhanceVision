# GitHub Releases Guide

English | [简体中文](04-github-release-guide_CN.md)

This document details how to release EnhanceVision through GitHub Releases.

## 📋 Table of Contents

- [Pre-Release Preparation](#pre-release-preparation)
- [Create Git Tag](#create-git-tag)
- [Create GitHub Release](#create-github-release)
- [Post-Release Verification](#post-release-verification)
- [Update Project Homepage](#update-project-homepage)

---

## Pre-Release Preparation

### 1. Confirm Release Contents

- [ ] Release version has been built
- [ ] Installer has been generated
- [ ] Portable version has been generated
- [ ] All tests pass
- [ ] CHANGELOG.md has been updated
- [ ] README.md has been updated

### 2. Prepare Release Files

```powershell
# Confirm files exist
$version = "0.1.0"

Test-Path "package\EnhanceVision-v$version-windows-x64-installer.exe"
Test-Path "package\EnhanceVision-v$version-windows-x64-portable.zip"
```

### 3. Prepare Release Notes

Edit `docs/releases/v0.1.0.md`:

```markdown
# EnhanceVision v0.1.0

First public release!

## ✨ New Features

### Dual-Mode Processing
- **Shader Mode**: 14 real-time parameter adjustments (exposure, brightness, contrast, saturation, hue, gamma, color temperature, tint, highlights, shadows, vignette, blur, denoise, sharpen)
- **AI Inference Mode**: Real-ESRGAN super-resolution enhancement (4x upscale)

### Concurrent Scheduling System
- Multi-level priority task queues
- AI engine pool supporting 2 concurrent inference tasks
- Deadlock detection and automatic recovery mechanism
- Task timeout monitoring and retry strategy

### Performance Optimization
- Zero-copy image transfer
- GPU-accelerated rendering
- GPU OOM auto-recovery

### Modern UI
- Qt Quick (QML) declarative UI
- Dark/Light themes
- Chinese/English bilingual support
- Session-based workflow

## 📥 Downloads

| File | Description | Size |
|------|-------------|------|
| [EnhanceVision-v0.1.0-windows-x64-installer.exe]() | Windows Installer | ~80 MB |
| [EnhanceVision-v0.1.0-windows-x64-portable.zip]() | Windows Portable | ~100 MB |

## 📋 System Requirements

- Windows 10/11 64-bit
- Vulkan compatible GPU (optional, for AI acceleration)
- 4GB+ RAM
- 500MB+ disk space

## 🚀 Quick Start

### Installer Version
1. Download the installer
2. Double-click to run the installer
3. Follow the setup wizard
4. Launch EnhanceVision from Start Menu

### Portable Version
1. Download the ZIP file
2. Extract to any directory
3. Double-click `EnhanceVision.exe` to launch

## 🔧 Known Issues

- None at this time

## 📝 Changelog

See [CHANGELOG.md](CHANGELOG.md) for details

## 🙏 Acknowledgments

Thanks to the following open-source projects:
- [Qt](https://www.qt.io/) - UI Framework
- [NCNN](https://github.com/Tencent/ncnn) - AI Inference Engine
- [FFmpeg](https://ffmpeg.org/) - Multimedia Processing
- [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN) - Super-Resolution Model
```

---

## Create Git Tag

### 1. Confirm Code Status

```powershell
# Check working directory status
git status

# Ensure all changes are committed
git add .
git commit -m "chore: prepare for v0.1.0 release"
```

### 2. Create Tag

```powershell
# Create annotated tag
git tag -a v0.1.0 -m "Release v0.1.0: First public release"

# View tags
git tag -l

# View tag details
git show v0.1.0
```

### 3. Push Tag

```powershell
# Push tag to remote
git push origin v0.1.0

# Or push all tags
git push origin --tags
```

---

## Create GitHub Release

### Method 1: Via GitHub Web Interface

1. **Visit Repository**
   - Open https://github.com/K-irito02/EnhanceVision
   - Click "Releases" on the right
   - Click "Draft a new release"

2. **Fill in Release Information**
   - **Choose a tag**: Select `v0.1.0` (or create new tag)
   - **Release title**: `EnhanceVision v0.1.0`
   - **Describe this release**: Paste Release Notes

3. **Upload Files**
   - Drag and drop or select files:
     - `EnhanceVision-v0.1.0-windows-x64-installer.exe`
     - `EnhanceVision-v0.1.0-windows-x64-portable.zip`

4. **Release Options**
   - [ ] Set as the latest release (check)
   - [ ] Set as a pre-release (optional for first release)
   - [ ] Save as draft (save draft first)

5. **Publish**
   - Click "Publish release"

### Method 2: Using GitHub CLI

```powershell
# Install GitHub CLI
# winget install GitHub.cli

# Login
gh auth login

# Create Release
gh release create v0.1.0 `
    package\EnhanceVision-v0.1.0-windows-x64-installer.exe `
    package\EnhanceVision-v0.1.0-windows-x64-portable.zip `
    --title "EnhanceVision v0.1.0" `
    --notes-file docs\releases\v0.1.0.md `
    --latest
```

### Method 3: Using GitHub API

```powershell
# Requires GitHub Token
$token = "YOUR_GITHUB_TOKEN"
$owner = "K-irito02"
$repo = "EnhanceVision"
$version = "0.1.0"

# Create Release
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

# Upload files
$uploadUrl = $response.upload_url -replace '\{\?name,label\}', "?name="

# Upload installer
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

# Upload portable version
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

## Post-Release Verification

### 1. Check Release Page

Visit https://github.com/K-irito02/EnhanceVision/releases/tag/v0.1.0

Confirm:
- [ ] Release title is correct
- [ ] Release Notes display correctly
- [ ] Files are uploaded
- [ ] File sizes are correct
- [ ] Download links work

### 2. Test Downloads

```powershell
# Download and verify files
$version = "0.1.0"
$url = "https://github.com/K-irito02/EnhanceVision/releases/download/v$version"

# Download installer
Invoke-WebRequest -Uri "$url/EnhanceVision-v$version-windows-x64-installer.exe" -OutFile "test-installer.exe"

# Download portable version
Invoke-WebRequest -Uri "$url/EnhanceVision-v$version-windows-x64-portable.zip" -OutFile "test-portable.zip"

# Verify file sizes
(Get-Item "test-installer.exe").Length / 1MB
(Get-Item "test-portable.zip").Length / 1MB
```

### 3. Test Installation

```powershell
# Run installer
.\test-installer.exe

# Extract portable version
Expand-Archive -Path "test-portable.zip" -DestinationPath "test-portable"

# Run portable version
.\test-portable\EnhanceVision-v0.1.0-windows-x64\EnhanceVision.exe
```

---

## Update Project Homepage

### 1. Update README.md

Add download links to README.md:

```markdown
## Download & Installation

### Windows

| Version | File | Description |
|---------|------|-------------|
| v0.1.0 | [Installer](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-installer.exe) | Standard installation |
| v0.1.0 | [Portable](https://github.com/K-irito02/EnhanceVision/releases/download/v0.1.0/EnhanceVision-v0.1.0-windows-x64-portable.zip) | Extract and run |

### System Requirements

- Windows 10/11 64-bit
- Vulkan compatible GPU (optional, for AI acceleration)
- 4GB+ RAM
- 500MB+ disk space
```

### 2. Update About Section

In repository settings, update:
- Description: `A desktop image and video enhancement tool built with Qt 6 + QML`
- Website: Project homepage URL
- Topics: `qt`, `qml`, `image-processing`, `video-enhancement`, `ai`, `ncnn`, `real-esrgan`

### 3. Create Release Announcement

Post announcement in GitHub Discussions (if enabled):

```markdown
# 🎉 EnhanceVision v0.1.0 Released!

We are excited to announce the first public release of EnhanceVision!

## Key Features

- Dual-mode processing: Shader real-time filters + AI super-resolution
- Concurrent scheduling system
- GPU acceleration
- Modern UI

## Download

[Go to Releases page](https://github.com/K-irito02/EnhanceVision/releases/tag/v0.1.0)

## Feedback

If you have any issues or suggestions, please report them in [Issues](https://github.com/K-irito02/EnhanceVision/issues).

Thank you for your support!
```

---

## Release Checklist

After release, confirm the following:

- [ ] Git tag created and pushed
- [ ] GitHub Release published
- [ ] Files uploaded and downloadable
- [ ] README.md updated with download links
- [ ] CHANGELOG.md updated
- [ ] Project homepage updated
- [ ] Release announcement posted (if applicable)
- [ ] Social media posted (if applicable)

---

## Next Version Release

After release, prepare for next version development:

1. Update version number (e.g., 0.2.0 or 0.1.1)
2. Add `[Unreleased]` section in CHANGELOG.md
3. Create new development branch (if needed)

---

*Last updated: 2025*
