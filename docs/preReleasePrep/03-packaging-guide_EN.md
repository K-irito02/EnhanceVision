# Packaging Guide

English | [简体中文](03-packaging-guide_CN.md)

This document details how to package EnhanceVision into distributable installers and portable versions.

## 📋 Table of Contents

- [Packaging Overview](#packaging-overview)
- [Preparation](#preparation)
- [NSIS Installer](#nsis-installer)
- [Portable ZIP](#portable-zip)
- [Packaging Verification](#packaging-verification)
- [Common Issues](#common-issues)

---

## Packaging Overview

### Packaging Targets

| Type | Format | Purpose |
|------|--------|---------|
| Installer | `.exe` (NSIS) | Standard installation with uninstall support, start menu |
| Portable | `.zip` | Green version, extract and run |

### Package Contents

```
EnhanceVision-v0.1.0-windows-x64/
├── EnhanceVision.exe          # Main executable (~2MB)
├── models/                    # AI models (~50MB)
│   ├── RealESRGAN_x4plus.param
│   └── RealESRGAN_x4plus.bin
├── translations/              # Translation files (~50KB)
├── Qt DLLs/                   # Qt runtime (~30MB)
├── FFmpeg DLLs/               # FFmpeg runtime (~20MB)
├── LICENSE                    # MIT License
├── THIRD_PARTY_LICENSES.md    # Third-party licenses
└── README.txt                 # Brief description
```

### Estimated Size

- Installer: ~80MB
- Portable ZIP: ~100MB (extracted)

---

## Preparation

### 1. Install NSIS

Download and install [NSIS (Nullsoft Scriptable Install System)](https://nsis.sourceforge.io/):

```powershell
# Install using Chocolatey (recommended)
choco install nsis -y

# Or download and install manually
# https://nsis.sourceforge.io/Download
```

Verify installation:
```powershell
makensis /version
# Should output: NSIS Version 3.x
```

### 2. Build Release Version

Refer to [02-build-guide.md](02-build-guide.md) to complete Release build.

```powershell
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### 3. Prepare Packaging Directory

```powershell
# Create packaging directory
$version = "0.1.0"
$packageDir = "package\EnhanceVision-v$version-windows-x64"
New-Item -ItemType Directory -Force -Path $packageDir

# Copy main program
Copy-Item -Path "build\msvc2022\Release\Release\*" -Destination $packageDir -Recurse

# Copy license files
Copy-Item -Path "LICENSE" -Destination $packageDir
Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir

# Create README.txt
@"
EnhanceVision v$version

A desktop image and video enhancement tool built with Qt 6 + QML.

Quick Start:
1. Double-click EnhanceVision.exe to launch
2. Add image or video files
3. Select Shader mode or AI mode
4. Adjust parameters and export

For more information, visit: https://github.com/K-irito02/EnhanceVision

License: MIT License
"@ | Out-File -FilePath "$packageDir\README.txt" -Encoding UTF8
```

---

## NSIS Installer

### 1. Create NSIS Script

Create file `installer/setup.nsi`:

```nsis
; EnhanceVision Installer Script
; Compile with NSIS 3.x

!include "MUI2.nsh"
!include "FileFunc.nsh"

; Application Information
!define APP_NAME "EnhanceVision"
!define APP_VERSION "0.1.0"
!define APP_PUBLISHER "EnhanceVision Contributors"
!define APP_URL "https://github.com/K-irito02/EnhanceVision"
!define APP_GUID "EnhanceVision-0.1.0"

; Installer Information
Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\package\EnhanceVision-v${APP_VERSION}-windows-x64-installer.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "Software\${APP_NAME}" "Install_Dir"
RequestExecutionLevel admin

; Interface Settings
!define MUI_ICON "..\resources\icons\app.ico"
!define MUI_UNICON "..\resources\icons\app.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\installer\welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "..\installer\welcome.bmp"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

; Installation Section
Section "EnhanceVision (required)" SecMain
    SectionIn RO
    
    ; Set output path
    SetOutPath $INSTDIR
    
    ; Write files
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\*.*"
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Write to registry
    WriteRegStr HKLM "Software\${APP_NAME}" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "URLInfoAbout" "${APP_URL}"
    
    ; Calculate installation size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "EstimatedSize" "$0"
SectionEnd

; Start Menu Shortcuts
Section "Start Menu Shortcuts" SecShortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

; Desktop Shortcut (Optional)
Section "Desktop Shortcut" SecDesktop
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
SectionEnd

; Uninstallation Section
Section "Uninstall"
    ; Delete files
    RMDir /r "$INSTDIR"
    
    ; Delete shortcuts
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    
    ; Delete registry entries
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKLM "Software\${APP_NAME}"
SectionEnd
```

### 2. Compile Installer

```powershell
# Create installer directory
New-Item -ItemType Directory -Force -Path installer

# Compile NSIS script
makensis installer\setup.nsi

# Output file
# package\EnhanceVision-v0.1.0-windows-x64-installer.exe
```

### 3. Test Installer

```powershell
# Run installer
.\package\EnhanceVision-v0.1.0-windows-x64-installer.exe

# Verify installation
Test-Path "C:\Program Files\EnhanceVision\EnhanceVision.exe"

# Test uninstall
# Control Panel -> Programs and Features -> EnhanceVision -> Uninstall
```

---

## Portable ZIP

### 1. Create Portable Version

```powershell
$version = "0.1.0"
$sourceDir = "package\EnhanceVision-v$version-windows-x64"
$zipFile = "package\EnhanceVision-v$version-windows-x64-portable.zip"

# Compress using PowerShell
Compress-Archive -Path $sourceDir -DestinationPath $zipFile -Force

# Or use 7-Zip (recommended, better compression)
# 7z a -tzip $zipFile $sourceDir
```

### 2. Verify Portable Version

```powershell
# Extract for testing
Expand-Archive -Path $zipFile -DestinationPath "test-portable" -Force

# Run test
.\test-portable\EnhanceVision-v$version-windows-x64\EnhanceVision.exe

# Clean up test
Remove-Item -Recurse -Force "test-portable"
```

---

## Packaging Verification

### Automated Verification Script

Create `scripts\verify-package.ps1`:

```powershell
param(
    [string]$Version = "0.1.0"
)

$packageDir = "package\EnhanceVision-v$Version-windows-x64"
$errors = @()

# Check required files
$requiredFiles = @(
    "EnhanceVision.exe",
    "LICENSE",
    "THIRD_PARTY_LICENSES.md",
    "README.txt"
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path "$packageDir\$file")) {
        $errors += "Missing file: $file"
    }
}

# Check model files
if (-not (Test-Path "$packageDir\models\RealESRGAN_x4plus.param")) {
    $errors += "Missing AI model file"
}

# Check translation files
if (-not (Test-Path "$packageDir\translations\app_zh_CN.qm")) {
    $errors += "Missing Chinese translation file"
}

# Check Qt DLLs
if (-not (Test-Path "$packageDir\Qt6Core.dll")) {
    $errors += "Missing Qt DLL files"
}

# Check FFmpeg DLLs
$ffmpegDlls = @("avcodec-62.dll", "avformat-62.dll", "avutil-60.dll", "swscale-9.dll")
foreach ($dll in $ffmpegDlls) {
    if (-not (Test-Path "$packageDir\$dll")) {
        $errors += "Missing FFmpeg DLL: $dll"
    }
}

# Output results
if ($errors.Count -eq 0) {
    Write-Host "✅ Package verification passed" -ForegroundColor Green
} else {
    Write-Host "❌ Package verification failed:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
    exit 1
}
```

Run verification:
```powershell
.\scripts\verify-package.ps1 -Version "0.1.0"
```

---

## Complete Packaging Workflow

Create `scripts\build-package.ps1`:

```powershell
param(
    [string]$Version = "0.1.0"
)

Write-Host "=== EnhanceVision Packaging Script ===" -ForegroundColor Cyan
Write-Host "Version: $Version" -ForegroundColor Cyan

# 1. Build Release version
Write-Host "`n[1/5] Building Release version..." -ForegroundColor Yellow
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# 2. Prepare packaging directory
Write-Host "`n[2/5] Preparing packaging directory..." -ForegroundColor Yellow
$packageDir = "package\EnhanceVision-v$Version-windows-x64"
Remove-Item -Recurse -Force $packageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

# Copy files
Copy-Item -Path "build\msvc2022\Release\Release\*" -Destination $packageDir -Recurse
Copy-Item -Path "LICENSE" -Destination $packageDir
Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir

# Create README.txt
@"
EnhanceVision v$Version

A desktop image and video enhancement tool built with Qt 6 + QML.

Quick Start:
1. Double-click EnhanceVision.exe to launch
2. Add image or video files
3. Select Shader mode or AI mode
4. Adjust parameters and export

For more information, visit: https://github.com/K-irito02/EnhanceVision

License: MIT License
"@ | Out-File -FilePath "$packageDir\README.txt" -Encoding UTF8

# 3. Verify package
Write-Host "`n[3/5] Verifying package..." -ForegroundColor Yellow
.\scripts\verify-package.ps1 -Version $Version
if ($LASTEXITCODE -ne 0) { throw "Package verification failed" }

# 4. Create installer
Write-Host "`n[4/5] Creating installer..." -ForegroundColor Yellow
makensis installer\setup.nsi
if ($LASTEXITCODE -ne 0) { throw "Installer creation failed" }

# 5. Create portable version
Write-Host "`n[5/5] Creating portable version..." -ForegroundColor Yellow
$zipFile = "package\EnhanceVision-v$Version-windows-x64-portable.zip"
Remove-Item -Force $zipFile -ErrorAction SilentlyContinue
Compress-Archive -Path $packageDir -DestinationPath $zipFile

Write-Host "`n=== Packaging Complete ===" -ForegroundColor Green
Write-Host "Installer: package\EnhanceVision-v$Version-windows-x64-installer.exe"
Write-Host "Portable: package\EnhanceVision-v$Version-windows-x64-portable.zip"

# Display file sizes
$installerSize = (Get-Item "package\EnhanceVision-v$Version-windows-x64-installer.exe").Length / 1MB
$portableSize = (Get-Item $zipFile).Length / 1MB
Write-Host "`nFile Sizes:"
Write-Host "  Installer: $([math]::Round($installerSize, 2)) MB"
Write-Host "  Portable: $([math]::Round($portableSize, 2)) MB"
```

Run complete packaging:
```powershell
.\scripts\build-package.ps1 -Version "0.1.0"
```

---

## Common Issues

### Q1: NSIS Compilation Failed

**Error**: `!include: could not find: MUI2.nsh`

**Solution**: Make sure to select "Modern UI 2" component when installing NSIS.

### Q2: Installer Size Too Large

**Solution**: 
- Check for unnecessary files
- Use 7-Zip's LZMA compression (requires NSIS plugin)
- Consider providing AI models as separate download

### Q3: Program Won't Start After Installation

**Solution**:
- Check if all DLLs are copied correctly
- Check if Qt platform plugins exist
- Check if FFmpeg DLLs exist

### Q4: Residual Files After Uninstall

**Solution**: Check the uninstall section in NSIS script to ensure all files are deleted.

---

## Next Steps

After packaging, refer to [04-github-release-guide.md](04-github-release-guide.md) for release.

---

*Last updated: 2025*
