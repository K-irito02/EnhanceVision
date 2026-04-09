# 打包指南

[English](03-packaging-guide_EN.md) | 简体中文

本文档详细说明如何将 EnhanceVision 打包为可分发的安装程序和便携版。

## 📋 目录

- [打包概述](#打包概述)
- [准备工作](#准备工作)
- [NSIS 安装程序](#nsis-安装程序)
- [便携版 ZIP](#便携版-zip)
- [打包验证](#打包验证)
- [常见问题](#常见问题)

---

## 打包概述

### 打包目标

| 类型 | 格式 | 用途 |
|------|------|------|
| 安装程序 | `.exe` (NSIS) | 标准安装，支持卸载、开始菜单 |
| 便携版 | `.zip` | 绿色版，解压即用 |

### 打包内容

```
EnhanceVision-v0.1.0-windows-x64/
├── EnhanceVision.exe          # 主程序 (~2MB)
├── models/                    # AI 模型 (~50MB)
│   ├── RealESRGAN_x4plus.param
│   └── RealESRGAN_x4plus.bin
├── translations/              # 翻译文件 (~50KB)
├── Qt DLLs/                   # Qt 运行时 (~30MB)
├── FFmpeg DLLs/               # FFmpeg 运行时 (~20MB)
├── LICENSE                    # MIT 许可证
├── THIRD_PARTY_LICENSES.md    # 第三方许可证
└── README.txt                 # 简要说明
```

### 预计大小

- 安装程序: ~80MB
- 便携版 ZIP: ~100MB（解压后）

---

## 准备工作

### 1. 安装 NSIS

下载并安装 [NSIS (Nullsoft Scriptable Install System)](https://nsis.sourceforge.io/):

```powershell
# 使用 Chocolatey 安装（推荐）
choco install nsis -y

# 或手动下载安装
# https://nsis.sourceforge.io/Download
```

验证安装：
```powershell
makensis /version
# 应输出: NSIS Version 3.x
```

### 2. 构建 Release 版本

参考 [02-build-guide.md](02-build-guide.md) 完成 Release 构建。

```powershell
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### 3. 准备打包目录

```powershell
# 创建打包目录
$version = "0.1.0"
$packageDir = "package\EnhanceVision-v$version-windows-x64"
New-Item -ItemType Directory -Force -Path $packageDir

# 复制主程序
Copy-Item -Path "build\msvc2022\Release\Release\*" -Destination $packageDir -Recurse

# 复制许可证文件
Copy-Item -Path "LICENSE" -Destination $packageDir
Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir

# 创建 README.txt
@"
EnhanceVision v$version

基于 Qt 6 + QML 的桌面端图像与视频画质增强工具。

快速开始:
1. 双击 EnhanceVision.exe 启动程序
2. 添加图像或视频文件
3. 选择 Shader 模式或 AI 模式
4. 调整参数并导出

更多信息请访问: https://github.com/K-irito02/EnhanceVision

许可证: MIT License
"@ | Out-File -FilePath "$packageDir\README.txt" -Encoding UTF8
```

---

## NSIS 安装程序

### 1. 创建 NSIS 脚本

创建文件 `installer/setup.nsi`:

```nsis
; EnhanceVision 安装程序脚本
; 使用 NSIS 3.x 编译

!include "MUI2.nsh"
!include "FileFunc.nsh"

; 应用程序信息
!define APP_NAME "EnhanceVision"
!define APP_VERSION "0.1.0"
!define APP_PUBLISHER "EnhanceVision Contributors"
!define APP_URL "https://github.com/K-irito02/EnhanceVision"
!define APP_GUID "EnhanceVision-0.1.0"

; 安装程序信息
Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\package\EnhanceVision-v${APP_VERSION}-windows-x64-installer.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "Software\${APP_NAME}" "Install_Dir"
RequestExecutionLevel admin

; 界面设置
!define MUI_ICON "..\resources\icons\app.ico"
!define MUI_UNICON "..\resources\icons\app.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\installer\welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "..\installer\welcome.bmp"

; 页面
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; 语言
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; 安装区段
Section "EnhanceVision (required)" SecMain
    SectionIn RO
    
    ; 设置输出路径
    SetOutPath $INSTDIR
    
    ; 写入文件
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\*.*"
    
    ; 创建卸载程序
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; 写入注册表
    WriteRegStr HKLM "Software\${APP_NAME}" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "URLInfoAbout" "${APP_URL}"
    
    ; 计算安装大小
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "EstimatedSize" "$0"
SectionEnd

; 开始菜单快捷方式
Section "Start Menu Shortcuts" SecShortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

; 桌面快捷方式（可选）
Section "Desktop Shortcut" SecDesktop
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
SectionEnd

; 卸载区段
Section "Uninstall"
    ; 删除文件
    RMDir /r "$INSTDIR"
    
    ; 删除快捷方式
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    
    ; 删除注册表
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKLM "Software\${APP_NAME}"
SectionEnd
```

### 2. 编译安装程序

```powershell
# 创建 installer 目录
New-Item -ItemType Directory -Force -Path installer

# 编译 NSIS 脚本
makensis installer\setup.nsi

# 输出文件
# package\EnhanceVision-v0.1.0-windows-x64-installer.exe
```

### 3. 测试安装程序

```powershell
# 运行安装程序
.\package\EnhanceVision-v0.1.0-windows-x64-installer.exe

# 验证安装
Test-Path "C:\Program Files\EnhanceVision\EnhanceVision.exe"

# 测试卸载
# 控制面板 -> 程序和功能 -> EnhanceVision -> 卸载
```

---

## 便携版 ZIP

### 1. 创建便携版

```powershell
$version = "0.1.0"
$sourceDir = "package\EnhanceVision-v$version-windows-x64"
$zipFile = "package\EnhanceVision-v$version-windows-x64-portable.zip"

# 使用 PowerShell 压缩
Compress-Archive -Path $sourceDir -DestinationPath $zipFile -Force

# 或使用 7-Zip（推荐，压缩率更高）
# 7z a -tzip $zipFile $sourceDir
```

### 2. 验证便携版

```powershell
# 解压测试
Expand-Archive -Path $zipFile -DestinationPath "test-portable" -Force

# 运行测试
.\test-portable\EnhanceVision-v$version-windows-x64\EnhanceVision.exe

# 清理测试
Remove-Item -Recurse -Force "test-portable"
```

---

## 打包验证

### 自动化验证脚本

创建 `scripts\verify-package.ps1`:

```powershell
param(
    [string]$Version = "0.1.0"
)

$packageDir = "package\EnhanceVision-v$Version-windows-x64"
$errors = @()

# 检查必要文件
$requiredFiles = @(
    "EnhanceVision.exe",
    "LICENSE",
    "THIRD_PARTY_LICENSES.md",
    "README.txt"
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path "$packageDir\$file")) {
        $errors += "缺少文件: $file"
    }
}

# 检查模型文件
if (-not (Test-Path "$packageDir\models\RealESRGAN_x4plus.param")) {
    $errors += "缺少 AI 模型文件"
}

# 检查翻译文件
if (-not (Test-Path "$packageDir\translations\app_zh_CN.qm")) {
    $errors += "缺少中文翻译文件"
}

# 检查 Qt DLL
if (-not (Test-Path "$packageDir\Qt6Core.dll")) {
    $errors += "缺少 Qt DLL 文件"
}

# 检查 FFmpeg DLL
$ffmpegDlls = @("avcodec-62.dll", "avformat-62.dll", "avutil-60.dll", "swscale-9.dll")
foreach ($dll in $ffmpegDlls) {
    if (-not (Test-Path "$packageDir\$dll")) {
        $errors += "缺少 FFmpeg DLL: $dll"
    }
}

# 输出结果
if ($errors.Count -eq 0) {
    Write-Host "✅ 打包验证通过" -ForegroundColor Green
} else {
    Write-Host "❌ 打包验证失败:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
    exit 1
}
```

运行验证：
```powershell
.\scripts\verify-package.ps1 -Version "0.1.0"
```

---

## 完整打包流程

创建 `scripts\build-package.ps1`:

```powershell
param(
    [string]$Version = "0.1.0"
)

Write-Host "=== EnhanceVision 打包脚本 ===" -ForegroundColor Cyan
Write-Host "版本: $Version" -ForegroundColor Cyan

# 1. 构建 Release 版本
Write-Host "`n[1/5] 构建 Release 版本..." -ForegroundColor Yellow
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
if ($LASTEXITCODE -ne 0) { throw "构建失败" }

# 2. 准备打包目录
Write-Host "`n[2/5] 准备打包目录..." -ForegroundColor Yellow
$packageDir = "package\EnhanceVision-v$Version-windows-x64"
Remove-Item -Recurse -Force $packageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

# 复制文件
Copy-Item -Path "build\msvc2022\Release\Release\*" -Destination $packageDir -Recurse
Copy-Item -Path "LICENSE" -Destination $packageDir
Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir

# 创建 README.txt
@"
EnhanceVision v$Version

基于 Qt 6 + QML 的桌面端图像与视频画质增强工具。

快速开始:
1. 双击 EnhanceVision.exe 启动程序
2. 添加图像或视频文件
3. 选择 Shader 模式或 AI 模式
4. 调整参数并导出

更多信息请访问: https://github.com/K-irito02/EnhanceVision

许可证: MIT License
"@ | Out-File -FilePath "$packageDir\README.txt" -Encoding UTF8

# 3. 验证打包
Write-Host "`n[3/5] 验证打包..." -ForegroundColor Yellow
.\scripts\verify-package.ps1 -Version $Version
if ($LASTEXITCODE -ne 0) { throw "打包验证失败" }

# 4. 创建安装程序
Write-Host "`n[4/5] 创建安装程序..." -ForegroundColor Yellow
makensis installer\setup.nsi
if ($LASTEXITCODE -ne 0) { throw "创建安装程序失败" }

# 5. 创建便携版
Write-Host "`n[5/5] 创建便携版..." -ForegroundColor Yellow
$zipFile = "package\EnhanceVision-v$Version-windows-x64-portable.zip"
Remove-Item -Force $zipFile -ErrorAction SilentlyContinue
Compress-Archive -Path $packageDir -DestinationPath $zipFile

Write-Host "`n=== 打包完成 ===" -ForegroundColor Green
Write-Host "安装程序: package\EnhanceVision-v$Version-windows-x64-installer.exe"
Write-Host "便携版: package\EnhanceVision-v$Version-windows-x64-portable.zip"

# 显示文件大小
$installerSize = (Get-Item "package\EnhanceVision-v$Version-windows-x64-installer.exe").Length / 1MB
$portableSize = (Get-Item $zipFile).Length / 1MB
Write-Host "`n文件大小:"
Write-Host "  安装程序: $([math]::Round($installerSize, 2)) MB"
Write-Host "  便携版: $([math]::Round($portableSize, 2)) MB"
```

运行完整打包：
```powershell
.\scripts\build-package.ps1 -Version "0.1.0"
```

---

## 常见问题

### Q1: NSIS 编译失败

**错误**: `!include: could not find: MUI2.nsh`

**解决**: 安装 NSIS 时确保选择了 "Modern UI 2" 组件。

### Q2: 安装程序体积过大

**解决**: 
- 检查是否包含不必要的文件
- 使用 7-Zip 的 LZMA 压缩（需要 NSIS 插件）
- 考虑将 AI 模型单独提供下载

### Q3: 安装后程序无法启动

**解决**:
- 检查所有 DLL 是否正确复制
- 检查 Qt 平台插件是否存在
- 检查 FFmpeg DLL 是否存在

### Q4: 卸载后残留文件

**解决**: 检查 NSIS 脚本中的卸载区段，确保删除所有文件。

---

## 下一步

打包完成后，请参考 [04-github-release-guide.md](04-github-release-guide.md) 进行发布。

---

*最后更新: 2025年*
