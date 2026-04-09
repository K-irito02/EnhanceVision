# Packaging Guide

English | [简体中文](03-packaging-guide_CN.md)

This document details how to package EnhanceVision into distributable installers and portable versions.

## 📋 Table of Contents

- [Packaging Overview](#packaging-overview)
- [Packaging Tools Comparison](#packaging-tools-comparison)
- [Preparation](#preparation)
- [NSIS Installer](#nsis-installer)
- [Advanced Features Implementation](#advanced-features-implementation)
- [Portable ZIP](#portable-zip)
- [Packaging Verification](#packaging-verification)
- [Common Issues](#common-issues)

---

## Packaging Overview

### Packaging Targets

| Type | Format | Purpose |
|------|--------|---------|
| Installer | `.exe` (NSIS) | Standard installation with uninstall support, start menu, multi-language |
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

- Installer: ~100MB (including runtime libraries)
- Portable ZIP: ~100MB (extracted)

---

## Packaging Tools Comparison

### Free Packaging Tools Overview

| Tool | License | Learning Curve | Multi-language | Customization | Recommendation |
|------|---------|----------------|----------------|---------------|----------------|
| **NSIS** | zlib/libpng | Medium | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Inno Setup | BSD | Easy | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| WiX Toolset | MS-PL | Hard | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| Qt IFW | GPL/LGPL | Medium | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |

### Detailed Comparison

#### 1. NSIS (Nullsoft Scriptable Install System) ⭐ Recommended

**Pros:**
- Completely free and open source (zlib/libpng license)
- Flexible scripting for any complex logic
- Native multi-language support (40+ languages)
- High compression ratio (supports LZMA, zlib, etc.)
- Active community with rich plugins
- Small installer output size
- Supports silent installation (enterprise deployment friendly)

**Cons:**
- Requires learning specific script syntax
- Default interface is plain (needs customization)
- Debugging is relatively difficult

**Best for:** Open source/commercial projects requiring high customization and multi-language support

**Official Website:** https://nsis.sourceforge.io/

#### 2. Inno Setup

**Pros:**
- Completely free and open source (BSD license)
- Wizard-based script generation, easy to start
- Pascal script syntax is easy to learn
- Modern and beautiful default interface
- Comprehensive documentation with rich examples

**Cons:**
- Slightly less customization than NSIS
- Weaker multi-language support
- Some advanced features require plugins

**Best for:** Quick development, small to medium projects

**Official Website:** https://jrsoftware.org/isinfo.php

#### 3. WiX Toolset

**Pros:**
- Microsoft official tool, generates standard MSI
- Suitable for enterprise environment deployment
- Supports all Windows Installer features
- Good Visual Studio integration

**Cons:**
- Steep learning curve
- Complex and verbose XML configuration
- Not suitable for quick development

**Best for:** Enterprise-level deployment, projects requiring MSI format

**Official Website:** https://wixtoolset.org/

#### 4. Qt Installer Framework

**Pros:**
- High integration with Qt applications
- Modern and beautiful interface
- Supports online installation and incremental updates
- Cross-platform support

**Cons:**
- Requires Qt environment
- Higher learning cost
- Relatively complex configuration

**Best for:** Qt applications, projects requiring online installation

**Official Website:** https://doc.qt.io/qtinstallerframework/

### Why Choose NSIS?

For the EnhanceVision project, we recommend **NSIS** for the following reasons:

1. **Excellent Multi-language Support**: Native Chinese/English interface switching, meeting internationalization needs
2. **Flexible Scripting**: Can implement runtime detection, Vulkan detection, user data protection, and other advanced features
3. **Open Source Friendly**: zlib license is compatible with MIT license
4. **Rich Community Resources**: Many plugins and examples available
5. **Compact Size**: High compression ratio for generated installers

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

### 2. Prepare Runtime Libraries

Download and prepare the following runtime installers:

| Runtime | Download URL | Purpose |
|---------|--------------|---------|
| VC++ 2022 Redistributable (x64) | https://aka.ms/vs/17/release/vc_redist.x64.exe | MSVC runtime |
| DirectX End-User Runtime | https://www.microsoft.com/download/details.aspx?id=35 | DirectX components |

Create directory structure:
```
installer/
├── resources/
│   ├── welcome.bmp          # Welcome page image (500x314)
│   ├── header.bmp           # Header image (150x57)
│   └── app.ico              # Application icon
├── redist/
│   ├── vc_redist.x64.exe    # VC++ runtime
│   └── dxwebsetup.exe       # DirectX installer (optional)
├── license/
│   ├── license_zh.txt       # Chinese license agreement
│   └── license_en.txt       # English license agreement
└── setup.nsi                # NSIS main script
```

### 3. Build Release Version

Refer to [02-build-guide.md](02-build-guide.md) to complete Release build.

```powershell
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### 4. Prepare Packaging Directory

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

### Complete NSIS Script

Create file `installer/setup.nsi`:

```nsis
; ============================================================================
; EnhanceVision Installer Script
; Compile with NSIS 3.x
; ============================================================================

; ----------------------------------------------------------------------------
; Include Files
; ----------------------------------------------------------------------------
!include "MUI2.nsh"           ; Modern UI 2
!include "FileFunc.nsh"       ; File operation functions
!include "LogicLib.nsh"       ; Logic operations
!include "WinVer.nsh"         ; Windows version detection
!include "x64.nsh"            ; 64-bit system support
!include "WordFunc.nsh"       ; String processing
!include "nsDialogs.nsh"      ; Custom dialogs

; ----------------------------------------------------------------------------
; Application Information
; ----------------------------------------------------------------------------
!define APP_NAME "EnhanceVision"
!define APP_VERSION "0.1.0"
!define APP_PUBLISHER "EnhanceVision Contributors"
!define APP_URL "https://github.com/K-irito02/EnhanceVision"
!define APP_GUID "EnhanceVision-${APP_VERSION}"
!define APP_REGISTRY_KEY "Software\${APP_NAME}"
!define APP_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

; Data directory settings
!define DEFAULT_DATA_PATH "$APPDATA\EnhanceVision"
!define MIN_DISK_SPACE_MB 200

; Runtime version
!define VC_REDIST_MIN_VERSION "14.32.31326.0"

; ----------------------------------------------------------------------------
; Installer Information
; ----------------------------------------------------------------------------
Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\package\EnhanceVision-v${APP_VERSION}-windows-x64-installer.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${APP_REGISTRY_KEY}" "Install_Dir"
RequestExecutionLevel admin

; ----------------------------------------------------------------------------
; Interface Settings
; ----------------------------------------------------------------------------
; Icons
!define MUI_ICON "..\resources\icons\app.ico"
!define MUI_UNICON "..\resources\icons\app.ico"

; Custom images (optional)
!define MUI_WELCOMEFINISHPAGE_BITMAP "..\installer\resources\welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "..\installer\resources\welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "..\installer\resources\header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "..\installer\resources\header.bmp"

; Welcome page title
!define MUI_WELCOMEPAGE_TITLE "$(WelcomeTitle)"
!define MUI_WELCOMEPAGE_TEXT "$(WelcomeText)"

; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\EnhanceVision.exe"
!define MUI_FINISHPAGE_RUN_TEXT "$(FinishRunText)"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(FinishReadmeText)"
!define MUI_FINISHPAGE_LINK "$(FinishLinkText)"
!define MUI_FINISHPAGE_LINK_LOCATION "${APP_URL}"

; ----------------------------------------------------------------------------
; Language Selection Page (Before Installation)
; ----------------------------------------------------------------------------
Function .onInit
    ; Initialize language selection
    !insertmacro MUI_LANGDLL_DISPLAY
    
    ; Detect system language as default
    System::Call 'kernel32::GetUserDefaultUILanguage() i .r0'
    ${If} $0 = 2052
        StrCpy $LANGUAGE 2052  ; Simplified Chinese
    ${Else}
        StrCpy $LANGUAGE 1033  ; English
    ${EndIf}
    
    ; Check if 64-bit system
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorNot64Bit)"
        Abort
    ${EndIf}
    
    ; Check disk space
    Call CheckDiskSpace
    
    ; Check previous version
    Call CheckPreviousVersion
    
    ; Check runtime libraries
    Call CheckVCRuntime
    
    ; Check Vulkan support
    Call CheckVulkanSupport
FunctionEnd

; ----------------------------------------------------------------------------
; Page Definitions
; ----------------------------------------------------------------------------
; Language selection page (custom)
Page custom LanguageSelectionPage

; Welcome page
!insertmacro MUI_PAGE_WELCOME

; License agreement page
!insertmacro MUI_PAGE_LICENSE "..\installer\license\license_$LANGUAGE.txt"

; Installation directory page
!insertmacro MUI_PAGE_DIRECTORY

; Data directory page (custom)
Page custom DataDirectoryPage

; Installation options page
!insertmacro MUI_PAGE_COMPONENTS

; Installation progress page
!insertmacro MUI_PAGE_INSTFILES

; Finish page
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; ----------------------------------------------------------------------------
; Multi-language Support
; ----------------------------------------------------------------------------
; Installer languages
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

; Language string definitions
; English
LangString WelcomeTitle ${LANG_ENGLISH} "Welcome to EnhanceVision Setup"
LangString WelcomeText ${LANG_ENGLISH} "This wizard will guide you through the installation of EnhanceVision.$\r$\n$\r$\nEnhanceVision is a desktop image and video enhancement tool built with Qt 6 + QML, supporting both Shader and AI modes.$\r$\n$\r$\nIt is recommended that you close all other applications before continuing."
LangString FinishRunText ${LANG_ENGLISH} "Run EnhanceVision"
LangString FinishReadmeText ${LANG_ENGLISH} "View README file"
LangString FinishLinkText ${LANG_ENGLISH} "Visit project homepage"
LangString ErrorNot64Bit ${LANG_ENGLISH} "This program requires a 64-bit Windows system."
LangString ErrorDiskSpace ${LANG_ENGLISH} "Insufficient disk space! At least ${MIN_DISK_SPACE_MB} MB is required."
LangString ErrorVCRuntime ${LANG_ENGLISH} "Visual C++ runtime not detected. It will be installed automatically."
LangString WarningVulkan ${LANG_ENGLISH} "Vulkan support not detected. AI enhancement will use CPU mode, which may be slower.$\r$\n$\r$\nIt is recommended to update your graphics driver for the best experience."
LangString LangSelectTitle ${LANG_ENGLISH} "Language Selection"
LangString LangSelectText ${LANG_ENGLISH} "Please select the installer language:"
LangString LangSelectCN ${LANG_ENGLISH} "简体中文"
LangString LangSelectEN ${LANG_ENGLISH} "English"
LangString DataDirTitle ${LANG_ENGLISH} "Data Storage Location"
LangString DataDirText ${LANG_ENGLISH} "Select the data storage directory (for AI model cache, processing results, etc.):"
LangString DataDirDefault ${LANG_ENGLISH} "Use default location (Recommended)"
LangString DataDirCustom ${LANG_ENGLISH} "Custom location:"
LangString DataDirBrowse ${LANG_ENGLISH} "Browse..."
LangString SectionMain ${LANG_ENGLISH} "Main Program (Required)"
LangString SectionMainDesc ${LANG_ENGLISH} "Install EnhanceVision main program and core components"
LangString SectionShortcuts ${LANG_ENGLISH} "Start Menu Shortcuts"
LangString SectionShortcutsDesc ${LANG_ENGLISH} "Create program group in Start Menu"
LangString SectionDesktop ${LANG_ENGLISH} "Desktop Shortcut"
LangString SectionDesktopDesc ${LANG_ENGLISH} "Create shortcut on Desktop"
LangString UninstallConfirm ${LANG_ENGLISH} "Are you sure you want to uninstall EnhanceVision?"
LangString UninstallKeepData ${LANG_ENGLISH} "Do you want to keep user data?"

; Simplified Chinese
LangString WelcomeTitle ${LANG_SIMPCHINESE} "欢迎使用 EnhanceVision 安装向导"
LangString WelcomeText ${LANG_SIMPCHINESE} "本向导将引导您完成 EnhanceVision 的安装过程。$\r$\n$\r$\nEnhanceVision 是一款基于 Qt 6 + QML 的桌面端图像与视频画质增强工具，支持 Shader 模式和 AI 模式。$\r$\n$\r$\n建议您在继续之前关闭所有其他应用程序。"
LangString FinishRunText ${LANG_SIMPCHINESE} "运行 EnhanceVision"
LangString FinishReadmeText ${LANG_SIMPCHINESE} "查看 README 文件"
LangString FinishLinkText ${LANG_SIMPCHINESE} "访问项目主页"
LangString ErrorNot64Bit ${LANG_SIMPCHINESE} "此程序需要 64 位 Windows 系统。"
LangString ErrorDiskSpace ${LANG_SIMPCHINESE} "磁盘空间不足！至少需要 ${MIN_DISK_SPACE_MB} MB 可用空间。"
LangString ErrorVCRuntime ${LANG_SIMPCHINESE} "未检测到 Visual C++ 运行库，将自动安装。"
LangString WarningVulkan ${LANG_SIMPCHINESE} "未检测到 Vulkan 支持。AI 增强功能将使用 CPU 模式，性能可能较慢。$\r$\n$\r$\n建议更新显卡驱动以获得最佳体验。"
LangString LangSelectTitle ${LANG_SIMPCHINESE} "语言选择 / Language Selection"
LangString LangSelectText ${LANG_SIMPCHINESE} "请选择安装程序语言："
LangString LangSelectCN ${LANG_SIMPCHINESE} "简体中文"
LangString LangSelectEN ${LANG_SIMPCHINESE} "English"
LangString DataDirTitle ${LANG_SIMPCHINESE} "数据存储位置"
LangString DataDirText ${LANG_SIMPCHINESE} "请选择数据存储目录（用于保存 AI 模型缓存、处理结果等）："
LangString DataDirDefault ${LANG_SIMPCHINESE} "使用默认位置（推荐）"
LangString DataDirCustom ${LANG_SIMPCHINESE} "自定义位置："
LangString DataDirBrowse ${LANG_SIMPCHINESE} "浏览..."
LangString SectionMain ${LANG_SIMPCHINESE} "主程序（必需）"
LangString SectionMainDesc ${LANG_SIMPCHINESE} "安装 EnhanceVision 主程序和核心组件"
LangString SectionShortcuts ${LANG_SIMPCHINESE} "开始菜单快捷方式"
LangString SectionShortcutsDesc ${LANG_SIMPCHINESE} "在开始菜单创建程序组"
LangString SectionDesktop ${LANG_SIMPCHINESE} "桌面快捷方式"
LangString SectionDesktopDesc ${LANG_SIMPCHINESE} "在桌面创建快捷方式"
LangString UninstallConfirm ${LANG_SIMPCHINESE} "确定要卸载 EnhanceVision 吗？"
LangString UninstallKeepData ${LANG_SIMPCHINESE} "是否保留用户数据？"

; ----------------------------------------------------------------------------
; Global Variables
; ----------------------------------------------------------------------------
Var DataDirPath
Var UseDefaultDataDir
Var PreviousVersion
Var VulkanSupported
Var VCRuntimeInstalled

; ----------------------------------------------------------------------------
; Language Selection Page
; ----------------------------------------------------------------------------
Function LanguageSelectionPage
    !insertmacro MUI_HEADER_TEXT "$(LangSelectTitle)" "$(LangSelectText)"
    
    nsDialogs::Create 1018
    Pop $0
    
    ${If} $0 == error
        Abort
    ${EndIf}
    
    ; Chinese option
    ${NSD_CreateRadioButton} 20 20 100% 12u "$(LangSelectCN)"
    Pop $1
    ${NSD_AddStyle} $1 ${WS_GROUP}
    ${NSD_OnClick} $1 OnSelectChinese
    
    ; English option
    ${NSD_CreateRadioButton} 20 40 100% 12u "$(LangSelectEN)"
    Pop $2
    ${NSD_OnClick} $2 OnSelectEnglish
    
    ; Default selection based on system language
    ${If} $LANGUAGE == 2052
        ${NSD_Check} $1
    ${Else}
        ${NSD_Check} $2
    ${EndIf}
    
    nsDialogs::Show
FunctionEnd

Function OnSelectChinese
    StrCpy $LANGUAGE 2052
FunctionEnd

Function OnSelectEnglish
    StrCpy $LANGUAGE 1033
FunctionEnd

; ----------------------------------------------------------------------------
; Data Directory Selection Page
; ----------------------------------------------------------------------------
Function DataDirectoryPage
    !insertmacro MUI_HEADER_TEXT "$(DataDirTitle)" "$(DataDirText)"
    
    nsDialogs::Create 1018
    Pop $0
    
    ${If} $0 == error
        Abort
    ${EndIf}
    
    ; Default location option
    ${NSD_CreateRadioButton} 20 20 100% 12u "$(DataDirDefault)"
    Pop $1
    ${NSD_AddStyle} $1 ${WS_GROUP}
    ${NSD_OnClick} $1 OnSelectDefaultDataDir
    ${NSD_Check} $1
    
    ; Default path display
    ${NSD_CreateLabel} 40 40 100% 12u "${DEFAULT_DATA_PATH}"
    Pop $2
    
    ; Custom location option
    ${NSD_CreateRadioButton} 20 70 100% 12u "$(DataDirCustom)"
    Pop $3
    ${NSD_OnClick} $3 OnSelectCustomDataDir
    
    ; Custom path input
    ${NSD_CreateDirRequest} 40 90 280 12u "${DEFAULT_DATA_PATH}"
    Pop $4
    
    ; Browse button
    ${NSD_CreateBrowseButton} 330 90 50 12u "$(DataDirBrowse)"
    Pop $5
    ${NSD_OnClick} $5 OnBrowseDataDir
    
    ; Initialize
    StrCpy $UseDefaultDataDir 1
    StrCpy $DataDirPath "${DEFAULT_DATA_PATH}"
    
    nsDialogs::Show
FunctionEnd

Function OnSelectDefaultDataDir
    StrCpy $UseDefaultDataDir 1
    StrCpy $DataDirPath "${DEFAULT_DATA_PATH}"
FunctionEnd

Function OnSelectCustomDataDir
    StrCpy $UseDefaultDataDir 0
FunctionEnd

Function OnBrowseDataDir
    nsDialogs::SelectFolderDialog "" ""
    Pop $0
    ${If} $0 != error
        ${NSD_SetText} $4 "$0\EnhanceVision"
        StrCpy $DataDirPath "$0\EnhanceVision"
    ${EndIf}
FunctionEnd

; ----------------------------------------------------------------------------
; System Detection Functions
; ----------------------------------------------------------------------------

; Check disk space
Function CheckDiskSpace
    ${DriveSpace} "C:" "/D=F /S=M" $0
    ${If} $0 < ${MIN_DISK_SPACE_MB}
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorDiskSpace)"
        Abort
    ${EndIf}
FunctionEnd

; Check previous version
Function CheckPreviousVersion
    ReadRegStr $PreviousVersion HKLM "${APP_REGISTRY_KEY}" "Version"
    
    ${If} $PreviousVersion != ""
        MessageBox MB_OKCANCEL|MB_ICONQUESTION \
            "Detected EnhanceVision $PreviousVersion is already installed.$\r$\n$\r$\nDo you want to overwrite? (User data will be preserved)" \
            IDOK continue
        Abort
        
        continue:
        ; Preserve user data, don't delete config files
    ${EndIf}
FunctionEnd

; Check VC++ runtime
Function CheckVCRuntime
    StrCpy $VCRuntimeInstalled 0
    
    ; Check VC++ 2022 x64
    ReadRegDword $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
    ${If} $0 == 1
        StrCpy $VCRuntimeInstalled 1
    ${EndIf}
    
    ; If not installed, notify and prepare for installation
    ${If} $VCRuntimeInstalled == 0
        MessageBox MB_OK|MB_ICONINFORMATION "$(ErrorVCRuntime)"
    ${EndIf}
FunctionEnd

; Check Vulkan support
Function CheckVulkanSupport
    StrCpy $VulkanSupported 0
    
    ; Check Vulkan driver
    ${If} ${FileExists} "$SYSDIR\vulkan-1.dll"
        StrCpy $VulkanSupported 1
    ${EndIf}
    
    ; If not supported, show warning
    ${If} $VulkanSupported == 0
        MessageBox MB_OK|MB_ICONWARNING "$(WarningVulkan)$\r$\n$\r$\nDriver download links:$\r$\nNVIDIA: https://www.nvidia.com/drivers$\r$\nAMD: https://www.amd.com/support$\r$\nIntel: https://downloadcenter.intel.com"
    ${EndIf}
FunctionEnd

; ----------------------------------------------------------------------------
; Installation Sections
; ----------------------------------------------------------------------------

; Main program (required)
Section "$(SectionMain)" SecMain
    SectionIn RO
    
    ; If previous version exists, backup config first
    ${If} $PreviousVersion != ""
        ; Backup user config
        IfFileExists "$APPDATA\EnhanceVision\settings.ini" 0 +3
        CreateDirectory "$TEMP\EnhanceVision_backup"
        CopyFiles "$APPDATA\EnhanceVision\settings.ini" "$TEMP\EnhanceVision_backup\"
    ${EndIf}
    
    ; Set output path
    SetOutPath $INSTDIR
    
    ; Write files
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\*.*"
    
    ; Install VC++ runtime (if needed)
    ${If} $VCRuntimeInstalled == 0
        SetOutPath "$TEMP\vc_redist"
        File "..\installer\redist\vc_redist.x64.exe"
        ExecWait '"$TEMP\vc_redist\vc_redist.x64.exe" /quiet /norestart' $0
        RMDir /r "$TEMP\vc_redist"
    ${EndIf}
    
    ; Create data directory
    ${If} $UseDefaultDataDir == 0
        ; Use custom data directory
        CreateDirectory "$DataDirPath"
        
        ; Write config file
        SetShellVarContext current
        CreateDirectory "$APPDATA\EnhanceVision"
        WriteINIStr "$APPDATA\EnhanceVision\settings.ini" "General" "customDataPath" "$DataDirPath"
    ${EndIf}
    
    ; Write language setting
    ${If} $LANGUAGE == 2052
        WriteINIStr "$APPDATA\EnhanceVision\settings.ini" "General" "language" "zh_CN"
    ${Else}
        WriteINIStr "$APPDATA\EnhanceVision\settings.ini" "General" "language" "en_US"
    ${EndIf}
    
    ; Restore user config (if upgrade install)
    ${If} $PreviousVersion != ""
        IfFileExists "$TEMP\EnhanceVision_backup\settings.ini" 0 +3
        CopyFiles "$TEMP\EnhanceVision_backup\settings.ini" "$APPDATA\EnhanceVision\"
        RMDir /r "$TEMP\EnhanceVision_backup"
    ${EndIf}
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Write to registry
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Version" "${APP_VERSION}"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "DataDir" "$DataDirPath"
    
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "URLInfoAbout" "${APP_URL}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\EnhanceVision.exe"
    
    ; Calculate installation size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${APP_UNINSTALL_KEY}" "EstimatedSize" "$0"
SectionEnd

; Start menu shortcuts
Section "$(SectionShortcuts)" SecShortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

; Desktop shortcut
Section "$(SectionDesktop)" SecDesktop
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "$(SectionMainDesc)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "$(SectionShortcutsDesc)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "$(SectionDesktopDesc)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ----------------------------------------------------------------------------
; Uninstall Section
; ----------------------------------------------------------------------------
Section "Uninstall"
    ; Ask whether to keep user data
    MessageBox MB_YESNO|MB_ICONQUESTION "$(UninstallKeepData)" IDYES keepdata
    
    ; Delete user data
    RMDir /r "$APPDATA\EnhanceVision"
    
    keepdata:
    
    ; Delete installation files
    RMDir /r "$INSTDIR"
    
    ; Delete shortcuts
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    
    ; Delete registry entries
    DeleteRegKey HKLM "${APP_UNINSTALL_KEY}"
    DeleteRegKey HKLM "${APP_REGISTRY_KEY}"
SectionEnd

; ----------------------------------------------------------------------------
; Uninstall Initialization
; ----------------------------------------------------------------------------
Function un.onInit
    MessageBox MB_OKCANCEL|MB_ICONQUESTION "$(UninstallConfirm)" IDOK +2
    Abort
FunctionEnd
```

---

## Advanced Features Implementation

### 1. Language Selection Mechanism

#### Workflow

```
┌─────────────────────────────────────────────────────────────┐
│                     Installer Starts                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Detect System Language (GetUserDefaultUILanguage)│
│              Chinese system → Default to Chinese             │
│              Other systems → Default to English              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Display Language Selection Dialog               │
│              User can manually switch language               │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Subsequent installer pages use selected language│
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Write config file on installation complete      │
│              %APPDATA%\EnhanceVision\settings.ini            │
│              [General]                                       │
│              language=zh_CN or en_US                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Application reads config on startup             │
│              SettingsController::loadSettings()              │
│              Theme.setLanguage(lang)                         │
└─────────────────────────────────────────────────────────────┘
```

#### Configuration File Format

Application config file located at `%APPDATA%\EnhanceVision\settings.ini`:

```ini
[General]
language=zh_CN
customDataPath=D:\MyData\EnhanceVision
theme=dark
```

### 2. Data Directory Selection Mechanism

#### Workflow

```
┌─────────────────────────────────────────────────────────────┐
│              Display data directory selection page           │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────────┐
│    Use default location │     │    Custom location          │
│    %APPDATA%\           │     │    User selected directory  │
│    EnhanceVision        │     │                             │
└─────────────────────────┘     └─────────────────────────────┘
              │                               │
              └───────────────┬───────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Create data directory (if not exists)           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Write config file                               │
│              If custom path, write customDataPath            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Application reads on startup                    │
│              SettingsController::effectiveDataPath()         │
│              Returns actual data path                        │
└─────────────────────────────────────────────────────────────┘
```

#### Data Directory Structure

```
EnhanceVision/
├── ai_image/              # AI image processing cache
├── ai_video/              # AI video processing cache
├── shader_image/          # Shader image processing cache
├── shader_video/          # Shader video processing cache
├── logs/                  # Log files
└── thumbnails/            # Thumbnail cache
```

### 3. User Data Protection Mechanism

#### Upgrade Installation Flow

```
┌─────────────────────────────────────────────────────────────┐
│              Detected previous version installation          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Prompt user: Overwrite installation?            │
│              (User data will be preserved)                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Backup user config file                         │
│              %APPDATA%\EnhanceVision\settings.ini            │
│              → %TEMP%\EnhanceVision_backup\                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Install new version files                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Restore user config file                        │
│              Merge new settings (language, data directory)   │
└─────────────────────────────────────────────────────────────┘
```

#### Data Protection on Uninstall

```
┌─────────────────────────────────────────────────────────────┐
│              User executes uninstall                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Ask: Keep user data?                            │
└─────────────────────────────────────────────────────────────┘
              │                               │
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────────┐
│    Keep data            │     │    Delete data              │
│    Keep settings.ini    │     │    Delete %APPDATA%\        │
│    Keep cache files     │     │    EnhanceVision\           │
└─────────────────────────┘     └─────────────────────────────┘
```

### 4. Runtime Detection and Installation

#### Detection Logic

```nsis
; Check VC++ 2022 x64 runtime
ReadRegDword $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
${If} $0 == 1
    ; Installed
${Else}
    ; Not installed, auto install
    ExecWait '"vc_redist.x64.exe" /quiet /norestart'
${EndIf}
```

#### Embedded Runtime

Place runtime installers in `installer/redist/` directory:

```
installer/redist/
├── vc_redist.x64.exe      # VC++ 2022 Redistributable (x64)
└── dxwebsetup.exe         # DirectX Web Setup (optional)
```

### 5. Vulkan Support Detection

#### Detection Logic

```nsis
; Check Vulkan driver
${If} ${FileExists} "$SYSDIR\vulkan-1.dll"
    ; Vulkan supported
${Else}
    ; Not supported, show warning with driver download links
    MessageBox MB_OK|MB_ICONWARNING "Vulkan support not detected..."
${EndIf}
```

#### Driver Download Links

| GPU Vendor | Download Link |
|------------|---------------|
| NVIDIA | https://www.nvidia.com/drivers |
| AMD | https://www.amd.com/support |
| Intel | https://downloadcenter.intel.com |

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

### 2. Portable Launcher Script

Create `portable\start.bat`:

```batch
@echo off
REM EnhanceVision Portable Launcher

REM Set data directory to data folder in current directory
set APPDATA=%~dp0data

REM Launch program
start "" "%~dp0EnhanceVision.exe"
```

### 3. Verify Portable Version

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
Write-Host "`n[1/6] Building Release version..." -ForegroundColor Yellow
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# 2. Prepare packaging directory
Write-Host "`n[2/6] Preparing packaging directory..." -ForegroundColor Yellow
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
Write-Host "`n[3/6] Verifying package..." -ForegroundColor Yellow
.\scripts\verify-package.ps1 -Version $Version
if ($LASTEXITCODE -ne 0) { throw "Package verification failed" }

# 4. Create installer
Write-Host "`n[4/6] Creating installer..." -ForegroundColor Yellow
makensis installer\setup.nsi
if ($LASTEXITCODE -ne 0) { throw "Installer creation failed" }

# 5. Create portable version
Write-Host "`n[5/6] Creating portable version..." -ForegroundColor Yellow
$zipFile = "package\EnhanceVision-v$Version-windows-x64-portable.zip"
Remove-Item -Force $zipFile -ErrorAction SilentlyContinue
Compress-Archive -Path $packageDir -DestinationPath $zipFile

# 6. Calculate checksums
Write-Host "`n[6/6] Calculating checksums..." -ForegroundColor Yellow
$installerHash = Get-FileHash "package\EnhanceVision-v$Version-windows-x64-installer.exe" -Algorithm SHA256
$portableHash = Get-FileHash $zipFile -Algorithm SHA256

Write-Host "`n=== Packaging Complete ===" -ForegroundColor Green
Write-Host "Installer: package\EnhanceVision-v$Version-windows-x64-installer.exe"
Write-Host "Portable: package\EnhanceVision-v$Version-windows-x64-portable.zip"

# Display file sizes
$installerSize = (Get-Item "package\EnhanceVision-v$Version-windows-x64-installer.exe").Length / 1MB
$portableSize = (Get-Item $zipFile).Length / 1MB
Write-Host "`nFile Sizes:"
Write-Host "  Installer: $([math]::Round($installerSize, 2)) MB"
Write-Host "  Portable: $([math]::Round($portableSize, 2)) MB"

Write-Host "`nSHA256 Checksums:"
Write-Host "  Installer: $($installerHash.Hash)"
Write-Host "  Portable: $($portableHash.Hash)"
```

Run complete packaging:
```powershell
.\scripts\build-package.ps1 -Version "0.1.0"
```

---

## Installer Resource Preparation

### 1. Brand Image Specifications

| Image | Size | Format | Purpose |
|-------|------|--------|---------|
| welcome.bmp | 500 x 314 | BMP 24-bit | Welcome/Finish page |
| header.bmp | 150 x 57 | BMP 24-bit | Page header image |
| app.ico | 256 x 256 | ICO | Application icon |

### 2. License Agreement Files

Create `installer/license/license_en.txt`:

```
EnhanceVision End User License Agreement

Copyright (c) 2025 EnhanceVision Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy...
```

Create `installer/license/license_zh.txt`:

```
EnhanceVision 最终用户许可协议

版权所有 (c) 2025 EnhanceVision Contributors

特此免费授予任何获得本软件副本和相关文档文件（"软件"）的人不受限制地处置该软件的权利...
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

### Q5: Language Setting Not Working

**Solution**: 
- Check if config file is written correctly
- Check if application reads config correctly
- Confirm translation files exist

### Q6: Custom Data Directory Not Working

**Solution**:
- Check if `customDataPath` is correct in config file
- Confirm directory permissions
- Check `effectiveDataPath()` return value

---

## Next Steps

After packaging, refer to [04-github-release-guide.md](04-github-release-guide.md) for release.

---

*Last updated: 2025*
