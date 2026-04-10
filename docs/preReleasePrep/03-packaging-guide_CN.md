# 打包指南

[English](03-packaging-guide_EN.md) | 简体中�?
本文档详细说明如何将 EnhanceVision 打包为可分发的安装程序和便携版�?
## 📋 目录

- [打包概述](#打包概述)
- [打包工具对比](#打包工具对比)
- [准备工作](#准备工作)
- [NSIS 安装程序](#nsis-安装程序)
- [高级功能实现](#高级功能实现)
- [便携�?ZIP](#便携�?zip)
- [打包验证](#打包验证)
- [常见问题](#常见问题)

---

## 打包概述

### 打包目标

| 类型 | 格式 | 用�?|
|------|------|------|
| 安装程序 | `.exe` (NSIS) | 标准安装，支持卸载、开始菜单、多语言 |
| 便携�?| `.zip` | 绿色版，解压即用 |

### 打包内容

```
EnhanceVision-v0.1.0-windows-x64/
├── EnhanceVision.exe          # 主程�?(~2MB)
├── models/                    # AI 模型 (~50MB)
�?  ├── RealESRGAN_x4plus.param
�?  └── RealESRGAN_x4plus.bin
├── translations/              # 翻译文件 (~50KB)
├── Qt DLLs/                   # Qt 运行�?├── multimedia/                # Qt Multimedia 后端插件
├── qml/QtMultimedia/          # QML 多媒体插�?├── FFmpeg DLLs/               # FFmpeg 运行�?├── LICENSE                    # MIT 许可�?├── THIRD_PARTY_LICENSES.md    # 第三方许可证
└── README.txt                 # 简要说�?```

### 预计大小

- 安装程序: ~155MB（含运行库，LZMA 压缩�?- 便携�?ZIP: ~200MB

---

## 打包工具对比

### 免费打包工具一�?
| 工具 | 许可�?| 学习曲线 | 多语言支持 | 自定义程�?| 推荐指数 |
|------|--------|----------|------------|------------|----------|
| **NSIS** | zlib/libpng | 中等 | ⭐⭐⭐⭐�?| ⭐⭐⭐⭐�?| ⭐⭐⭐⭐�?|
| Inno Setup | BSD | 简�?| ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| WiX Toolset | MS-PL | 困难 | ⭐⭐�?| ⭐⭐⭐⭐�?| ⭐⭐�?|
| Qt IFW | GPL/LGPL | 中等 | ⭐⭐⭐⭐�?| ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |

### 详细对比

#### 1. NSIS (Nullsoft Scriptable Install System) �?推荐

**优点�?*
- 完全免费开源（zlib/libpng 许可证）
- 脚本灵活，可实现任意复杂逻辑
- 原生支持多语言界面�?0+ 语言�?- 压缩率高（支�?LZMA、zlib 等）
- 社区活跃，插件丰�?- 生成的安装程序体积小
- 支持静默安装（企业部署友好）

**缺点�?*
- 需要学习特定脚本语�?- 默认界面较朴素（需自定义美化）
- 调试相对困难

**适用场景�?* 需要高度自定义、多语言支持的开�?商业项目

**官方网站�?* https://nsis.sourceforge.io/

#### 2. Inno Setup

**优点�?*
- 完全免费开源（BSD 许可证）
- 向导式脚本生成，上手简�?- Pascal 脚本语法易学
- 默认界面美观现代
- 文档完善，示例丰�?
**缺点�?*
- 自定义程度略低于 NSIS
- 多语言支持稍弱
- 部分高级功能需要插�?
**适用场景�?* 快速开发、中小型项目

**官方网站�?* https://jrsoftware.org/isinfo.php

#### 3. WiX Toolset

**优点�?*
- 微软官方工具，生成标�?MSI
- 适合企业环境部署
- 支持 Windows Installer 全部功能
- �?Visual Studio 集成良好

**缺点�?*
- 学习曲线陡峭
- XML 配置复杂繁琐
- 不适合快速开�?
**适用场景�?* 企业级部署、需�?MSI 格式的项�?
**官方网站�?* https://wixtoolset.org/

#### 4. Qt Installer Framework

**优点�?*
- �?Qt 应用集成度高
- 界面现代美观
- 支持在线安装、增量更�?- 跨平台支�?
**缺点�?*
- 需�?Qt 环境
- 学习成本较高
- 配置相对复杂

**适用场景�?* Qt 应用、需要在线安装功�?
**官方网站�?* https://doc.qt.io/qtinstallerframework/

### 为什么选择 NSIS�?
对于 EnhanceVision 项目，推荐使�?**NSIS**，原因如下：

1. **多语言支持完善**：原生支持中英文界面切换，符合项目国际化需�?2. **灵活的脚本能�?*：可实现运行库检测、Vulkan 检测、用户数据保护等高级功能
3. **开源友�?*：zlib 许可证与 MIT 许可证兼�?4. **社区资源丰富**：大量插件和示例可供参�?5. **体积小巧**：生成的安装程序压缩率高

---

## 准备工作

### 1. 安装 NSIS

**已完�?* - NSIS 3.11 已安装在 `E:\Program Files (x86)\NSIS`

安装方法�?
```powershell
# 使用 Chocolatey 安装（推荐）
choco install nsis -y

# 或手动下载安�?# https://nsis.sourceforge.io/Download
```

验证安装�?```powershell
"E:\Program Files (x86)\NSIS\makensis.exe" /version
# 应输�? v3.11
```

### 2. 准备运行�?
**已完�?* - 运行库已下载，目录结构已创建

| 运行�?| 下载地址 | 用�?| 状�?|
|--------|----------|------|------|
| VC++ 2022 Redistributable (x64) | https://aka.ms/vs/17/release/vc_redist.x64.exe | MSVC 运行�?| �?已下�?(24.5 MB) |
| DirectX End-User Runtime | https://www.microsoft.com/download/details.aspx?id=35 | DirectX 组件 | �?可�?|

当前目录结构�?```
installer/
├── license/
�?  ├── license_en.txt       �?英文许可协议
�?  └── license_zh.txt       �?中文许可协议
├── redist/
�?  └── vc_redist.x64.exe    �?VC++ 2022 运行�?├── resources/             📁 品牌资源
�?  └── app_icon.ico      �?已就�?(来自 resources/icons/)
└── README.md              �?资源准备说明
```

### 3. 构建 Release 版本

参�?[02-build-guide.md](02-build-guide.md) 完成 Release 构建�?
```powershell
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
```

### 4. 准备打包目录

```powershell
# 创建打包目录
$version = "0.1.0"
$packageDir = "packaging\output\EnhanceVision-v$version-windows-x64"
New-Item -ItemType Directory -Force -Path $packageDir

# 复制 Release 运行目录
Copy-Item -Path "build\msvc2022\Release\Release\*" -Destination $packageDir -Recurse

# 补齐 Qt 运行时依�?& "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" `
    --release `
    --qmldir "qml" `
    --dir $packageDir `
    "$packageDir\EnhanceVision.exe"

# 复制许可证文�?Copy-Item -Path "LICENSE" -Destination $packageDir
Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir

# 创建 README.txt
@"
EnhanceVision v$version

基于 Qt 6 + QML 的桌面端图像与视频画质增强工具�?
快速开�?
1. 双击 EnhanceVision.exe 启动程序
2. 添加图像或视频文�?3. 选择 Shader 模式�?AI 模式
4. 调整参数并导�?
更多信息请访�? https://github.com/K-irito02/EnhanceVision

许可�? MIT License
"@ | Out-File -FilePath "$packageDir\README.txt" -Encoding UTF8
```

---

## NSIS 安装程序

### 完整 NSIS 脚本

创建文件 `installer/setup.nsi`:

```nsis
; ============================================================================
; EnhanceVision 安装程序脚本
; 使用 NSIS 3.x 编译
; ============================================================================

; ----------------------------------------------------------------------------
; 包含文件
; ----------------------------------------------------------------------------
!include "MUI2.nsh"           ; Modern UI 2
!include "FileFunc.nsh"       ; 文件操作函数
!include "LogicLib.nsh"       ; 逻辑判断
!include "WinVer.nsh"         ; Windows 版本检�?!include "x64.nsh"            ; 64位系统支�?!include "WordFunc.nsh"       ; 字符串处�?!include "nsDialogs.nsh"      ; 自定义对话框

; ----------------------------------------------------------------------------
; 应用程序信息
; ----------------------------------------------------------------------------
!define APP_NAME "EnhanceVision"
!define APP_VERSION "0.1.0"
!define APP_PUBLISHER "EnhanceVision Contributors"
!define APP_URL "https://github.com/K-irito02/EnhanceVision"
!define APP_GUID "EnhanceVision-${APP_VERSION}"
!define APP_REGISTRY_KEY "Software\${APP_NAME}"
!define APP_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

; 数据目录相关
!define DEFAULT_DATA_PATH "$APPDATA\EnhanceVision"
!define MIN_DISK_SPACE_MB 500

; 运行库版�?!define VC_REDIST_MIN_VERSION "14.32.31326.0"

; ----------------------------------------------------------------------------
; 安装程序信息
; ----------------------------------------------------------------------------
Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64-installer.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${APP_REGISTRY_KEY}" "Install_Dir"
RequestExecutionLevel admin

; 压缩设置
SetCompressor /SOLID lzma
SetCompressorDictSize 64

; ----------------------------------------------------------------------------
; 界面设置
; ----------------------------------------------------------------------------
; 图标
!define MUI_ICON "..\resources\icons\app_icon.ico"
!define MUI_UNICON "..\resources\icons\app_icon.ico"

; 欢迎页面标题
!define MUI_WELCOMEPAGE_TITLE "$(WelcomeTitle)"
!define MUI_WELCOMEPAGE_TEXT "$(WelcomeText)"

; 完成页面
!define MUI_FINISHPAGE_RUN "$INSTDIR\EnhanceVision.exe"
!define MUI_FINISHPAGE_RUN_TEXT "$(FinishRunText)"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(FinishReadmeText)"
!define MUI_FINISHPAGE_LINK "$(FinishLinkText)"
!define MUI_FINISHPAGE_LINK_LOCATION "${APP_URL}"

; ----------------------------------------------------------------------------
; 语言选择页面（安装前�?; ----------------------------------------------------------------------------
Function .onInit
    ; 初始化语言选择
    !insertmacro MUI_LANGDLL_DISPLAY
    
    ; 检测系统语言作为默认�?    System::Call 'kernel32::GetUserDefaultUILanguage() i .r0'
    ${If} $0 = 2052
        StrCpy $LANGUAGE 2052  ; 简体中�?    ${Else}
        StrCpy $LANGUAGE 1033  ; 英文
    ${EndIf}
    
    ; 检测是否为 64 位系�?    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorNot64Bit)"
        Abort
    ${EndIf}
    
    ; 检测磁盘空�?    Call CheckDiskSpace
    
    ; 检测旧版本
    Call CheckPreviousVersion
    
    ; 检测运行库
    Call CheckVCRuntime
    
    ; 检�?Vulkan 支持
    Call CheckVulkanSupport
FunctionEnd

; ----------------------------------------------------------------------------
; 页面定义
; ----------------------------------------------------------------------------
; 语言选择页面（自定义�?Page custom LanguageSelectionPage

; 欢迎页面
!insertmacro MUI_PAGE_WELCOME

; 许可协议页面
!insertmacro MUI_PAGE_LICENSE "$(LicenseFile)"

; 安装目录选择页面
!insertmacro MUI_PAGE_DIRECTORY

; 数据目录选择页面（自定义�?Page custom DataDirectoryPage

; 安装选项页面
!insertmacro MUI_PAGE_COMPONENTS

; 安装进度页面
!insertmacro MUI_PAGE_INSTFILES

; 完成页面
!insertmacro MUI_PAGE_FINISH

; 卸载页面
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; ----------------------------------------------------------------------------
; 多语言支持
; ----------------------------------------------------------------------------
; 安装程序语言
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; 多语言许可证（必须�?MUI_LANGUAGE 之后�?LicenseLangString LicenseFile ${LANG_SIMPCHINESE} "..\packaging\installer\license\license_zh.txt"
LicenseLangString LicenseFile ${LANG_ENGLISH} "..\packaging\installer\license\license_en.txt"

; 语言字符串定�?; 简体中�?LangString WelcomeTitle ${LANG_SIMPCHINESE} "欢迎使用 EnhanceVision 安装向导"
LangString WelcomeText ${LANG_SIMPCHINESE} "本向导将引导您完�?EnhanceVision 的安装过程�?\r$\n$\r$\nEnhanceVision 是一款基�?Qt 6 + QML 的桌面端图像与视频画质增强工具，支持 Shader 模式�?AI 模式�?\r$\n$\r$\n建议您在继续之前关闭所有其他应用程序�?
LangString FinishRunText ${LANG_SIMPCHINESE} "运行 EnhanceVision"
LangString FinishReadmeText ${LANG_SIMPCHINESE} "查看 README 文件"
LangString FinishLinkText ${LANG_SIMPCHINESE} "访问项目主页"
LangString ErrorNot64Bit ${LANG_SIMPCHINESE} "此程序需�?64 �?Windows 系统�?
LangString ErrorDiskSpace ${LANG_SIMPCHINESE} "磁盘空间不足！至少需�?${MIN_DISK_SPACE_MB} MB 可用空间�?
LangString ErrorVCRuntime ${LANG_SIMPCHINESE} "未检测到 Visual C++ 运行库，将自动安装�?
LangString WarningVulkan ${LANG_SIMPCHINESE} "未检测到 Vulkan 支持。AI 增强功能将使�?CPU 模式，性能可能较慢�?\r$\n$\r$\n建议更新显卡驱动以获得最佳体验�?
LangString LangSelectTitle ${LANG_SIMPCHINESE} "语言选择 / Language Selection"
LangString LangSelectText ${LANG_SIMPCHINESE} "请选择安装程序语言�?
LangString LangSelectCN ${LANG_SIMPCHINESE} "简体中�?
LangString LangSelectEN ${LANG_SIMPCHINESE} "English"
LangString DataDirTitle ${LANG_SIMPCHINESE} "数据存储位置"
LangString DataDirText ${LANG_SIMPCHINESE} "请选择数据存储目录（用于保�?AI 模型缓存、处理结果等）："
LangString DataDirDefault ${LANG_SIMPCHINESE} "使用默认位置（推荐）"
LangString DataDirCustom ${LANG_SIMPCHINESE} "自定义位置："
LangString DataDirBrowse ${LANG_SIMPCHINESE} "浏览..."
LangString SectionMain ${LANG_SIMPCHINESE} "主程序（必需�?
LangString SectionMainDesc ${LANG_SIMPCHINESE} "安装 EnhanceVision 主程序和核心组件"
LangString SectionShortcuts ${LANG_SIMPCHINESE} "开始菜单快捷方�?
LangString SectionShortcutsDesc ${LANG_SIMPCHINESE} "在开始菜单创建程序组"
LangString SectionDesktop ${LANG_SIMPCHINESE} "桌面快捷方式"
LangString SectionDesktopDesc ${LANG_SIMPCHINESE} "在桌面创建快捷方�?
LangString UninstallConfirm ${LANG_SIMPCHINESE} "确定要卸�?EnhanceVision 吗？"
LangString UninstallKeepData ${LANG_SIMPCHINESE} "是否保留用户数据�?
LangString PrevVersionDetected ${LANG_SIMPCHINESE} "检测到已安�?EnhanceVision $PreviousVersion$\r$\n$\r$\n是否覆盖安装？（用户数据将被保留�?

; 英文
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
LangString LangSelectCN ${LANG_ENGLISH} "简体中�?
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
LangString PrevVersionDetected ${LANG_ENGLISH} "Detected EnhanceVision $PreviousVersion$\r$\n$\r$\nOverwrite installation? (User data will be preserved)"

; ----------------------------------------------------------------------------
; 全局变量
; ----------------------------------------------------------------------------
Var DataDirPath
Var UseDefaultDataDir
Var PreviousVersion
Var VulkanSupported
Var VCRuntimeInstalled

; ----------------------------------------------------------------------------
; 语言选择页面
; ----------------------------------------------------------------------------
Function LanguageSelectionPage
    !insertmacro MUI_HEADER_TEXT "$(LangSelectTitle)" "$(LangSelectText)"
    
    nsDialogs::Create 1018
    Pop $0
    
    ${If} $0 == error
        Abort
    ${EndIf}
    
    ; 中文选项
    ${NSD_CreateRadioButton} 20 20 100% 12u "$(LangSelectCN)"
    Pop $1
    ${NSD_AddStyle} $1 ${WS_GROUP}
    ${NSD_OnClick} $1 OnSelectChinese
    
    ; 英文选项
    ${NSD_CreateRadioButton} 20 40 100% 12u "$(LangSelectEN)"
    Pop $2
    ${NSD_OnClick} $2 OnSelectEnglish
    
    ; 根据系统语言默认选择
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
; 数据目录选择页面
; ----------------------------------------------------------------------------
Function DataDirectoryPage
    !insertmacro MUI_HEADER_TEXT "$(DataDirTitle)" "$(DataDirText)"
    
    nsDialogs::Create 1018
    Pop $0
    
    ${If} $0 == error
        Abort
    ${EndIf}
    
    ; 默认位置选项
    ${NSD_CreateRadioButton} 20 20 100% 12u "$(DataDirDefault)"
    Pop $1
    ${NSD_AddStyle} $1 ${WS_GROUP}
    ${NSD_OnClick} $1 OnSelectDefaultDataDir
    ${NSD_Check} $1
    
    ; 默认路径显示
    ${NSD_CreateLabel} 40 40 100% 12u "${DEFAULT_DATA_PATH}"
    Pop $2
    
    ; 自定义位置选项
    ${NSD_CreateRadioButton} 20 70 100% 12u "$(DataDirCustom)"
    Pop $3
    ${NSD_OnClick} $3 OnSelectCustomDataDir
    
    ; 自定义路径输�?    ${NSD_CreateDirRequest} 40 90 280 12u "${DEFAULT_DATA_PATH}"
    Pop $4
    
    ; 浏览按钮
    ${NSD_CreateBrowseButton} 330 90 50 12u "$(DataDirBrowse)"
    Pop $5
    ${NSD_OnClick} $5 OnBrowseDataDir
    
    ; 初始�?    StrCpy $UseDefaultDataDir 1
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
; 系统检测函�?; ----------------------------------------------------------------------------

; 检测磁盘空�?Function CheckDiskSpace
    ${DriveSpace} "C:" "/D=F /S=M" $0
    ${If} $0 < ${MIN_DISK_SPACE_MB}
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorDiskSpace)"
        Abort
    ${EndIf}
FunctionEnd

; 检测旧版本
Function CheckPreviousVersion
    ReadRegStr $PreviousVersion HKLM "${APP_REGISTRY_KEY}" "Version"
    
    ${If} $PreviousVersion != ""
        MessageBox MB_OKCANCEL|MB_ICONQUESTION "$(PrevVersionDetected)" IDOK continue
        Abort
        
        continue:
        ; 保留用户数据，不删除配置文件
    ${EndIf}
FunctionEnd

; 检�?VC++ 运行�?Function CheckVCRuntime
    StrCpy $VCRuntimeInstalled 0
    
    ; 检�?VC++ 2022 x64
    ReadRegDword $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
    ${If} $0 == 1
        StrCpy $VCRuntimeInstalled 1
    ${EndIf}
    
    ; 如果未安装，提示并准备安�?    ${If} $VCRuntimeInstalled == 0
        MessageBox MB_OK|MB_ICONINFORMATION "$(ErrorVCRuntime)"
    ${EndIf}
FunctionEnd

; 检�?Vulkan 支持
Function CheckVulkanSupport
    StrCpy $VulkanSupported 0
    
    ; 检�?Vulkan 驱动
    ${If} ${FileExists} "$SYSDIR\vulkan-1.dll"
        StrCpy $VulkanSupported 1
    ${EndIf}
    
    ; 如果不支持，显示警告
    ${If} $VulkanSupported == 0
        StrCpy $0 "$(WarningVulkan)"
        MessageBox MB_ICONEXCLAMATION|MB_OK $0
    ${EndIf}
FunctionEnd

; ----------------------------------------------------------------------------
; 安装区段
; ----------------------------------------------------------------------------

; 主程序（必需�?Section "$(SectionMain)" SecMain
    SectionIn RO
    
    ; 如果存在旧版本，先备份配�?    ${If} $PreviousVersion != ""
        ; 备份用户配置
        IfFileExists "$APPDATA\EnhanceVision\settings.ini" 0 +3
        CreateDirectory "$TEMP\EnhanceVision_backup"
        CopyFiles "$APPDATA\EnhanceVision\settings.ini" "$TEMP\EnhanceVision_backup\"
    ${EndIf}
    
    ; 设置输出路径
    SetOutPath $INSTDIR
    
    ; 写入文件（逐文件列举，避免包含不需要的文件�?    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\EnhanceVision.exe"

    ; Qt DLLs
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Core.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Gui.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Qml.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Quick.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Impl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Basic.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2BasicStyleImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Fusion.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2FusionStyleImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Material.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2MaterialStyleImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Imagine.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2ImagineStyleImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Universal.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2UniversalStyleImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2WindowsStyleImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2FluentWinUI3StyleImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickTemplates2.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickLayouts.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickShapes.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickEffects.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickDialogs2.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickDialogs2QuickImpl.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickDialogs2Utils.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickWidgets.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Network.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6OpenGL.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Multimedia.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6MultimediaQuick.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlModels.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlCore.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlMeta.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlWorkerScript.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6LabsFolderListModel.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6LabsQmlModels.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Widgets.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Svg.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Sql.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Quick3DUtils.dll"

    ; 系统 DLLs
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\D3Dcompiler_47.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\dxcompiler.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\dxil.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\opengl32sw.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\icuuc.dll"

    ; FFmpeg DLLs
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\avcodec-62.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\avformat-62.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\avutil-60.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\swscale-9.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\swresample-6.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\avdevice-62.dll"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\avfilter-11.dll"

    ; 许可证和说明
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\LICENSE"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\THIRD_PARTY_LICENSES.md"
    File "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\README.txt"

    ; 子目�?    SetOutPath $INSTDIR\models
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\models\*.*"

    SetOutPath $INSTDIR\translations
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\translations\*.*"

    SetOutPath $INSTDIR\qml
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\qml\*.*"

    SetOutPath $INSTDIR\platforms
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\platforms\*.*"

    SetOutPath $INSTDIR\styles
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\styles\*.*"

    SetOutPath $INSTDIR\imageformats
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\imageformats\*.*"

    SetOutPath $INSTDIR\iconengines
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\iconengines\*.*"

    SetOutPath $INSTDIR\generic
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\generic\*.*"

    SetOutPath $INSTDIR\networkinformation
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\networkinformation\*.*"

    SetOutPath $INSTDIR\tls
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\tls\*.*"

    SetOutPath $INSTDIR\sqldrivers
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\sqldrivers\*.*"

    SetOutPath $INSTDIR\multimedia
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\multimedia\*.*"

    SetOutPath $INSTDIR\qmltooling
    File /r "..\packaging\output\EnhanceVision-v${APP_VERSION}-windows-x64\qmltooling\*.*"
    
    ; 安装 VC++ 运行库（如果需要）
    ${If} $VCRuntimeInstalled == 0
        SetOutPath "$TEMP\vc_redist"
        File "..\packaging\installer\redist\vc_redist.x64.exe"
        ExecWait '"$TEMP\vc_redist\vc_redist.x64.exe" /quiet /norestart' $0
        RMDir /r "$TEMP\vc_redist"
    ${EndIf}
    
    ; 创建数据目录
    ${If} $UseDefaultDataDir == 0
        ; 使用自定义数据目�?        CreateDirectory "$DataDirPath"
        
        ; 写入配置文件
        SetShellVarContext current
        CreateDirectory "$APPDATA\EnhanceVision"
        WriteINIStr "$APPDATA\EnhanceVision\settings.ini" "General" "customDataPath" "$DataDirPath"
    ${EndIf}
    
    ; 写入语言设置
    ${If} $LANGUAGE == 2052
        WriteINIStr "$APPDATA\EnhanceVision\settings.ini" "General" "language" "zh_CN"
    ${Else}
        WriteINIStr "$APPDATA\EnhanceVision\settings.ini" "General" "language" "en_US"
    ${EndIf}
    
    ; 恢复用户配置（如果是覆盖安装�?    ${If} $PreviousVersion != ""
        IfFileExists "$TEMP\EnhanceVision_backup\settings.ini" 0 +3
        CopyFiles "$TEMP\EnhanceVision_backup\settings.ini" "$APPDATA\EnhanceVision\"
        RMDir /r "$TEMP\EnhanceVision_backup"
    ${EndIf}
    
    ; 创建卸载程序
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; 写入注册�?    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Version" "${APP_VERSION}"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "DataDir" "$DataDirPath"
    
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "URLInfoAbout" "${APP_URL}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\EnhanceVision.exe"
    
    ; 计算安装大小
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${APP_UNINSTALL_KEY}" "EstimatedSize" "$0"
SectionEnd

; 开始菜单快捷方�?Section "$(SectionShortcuts)" SecShortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

; 桌面快捷方式
Section "$(SectionDesktop)" SecDesktop
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
SectionEnd

; 区段描述
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "$(SectionMainDesc)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "$(SectionShortcutsDesc)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "$(SectionDesktopDesc)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ----------------------------------------------------------------------------
; 卸载区段
; ----------------------------------------------------------------------------
Section "Uninstall"
    ; 询问是否保留用户数据
    MessageBox MB_YESNO|MB_ICONQUESTION "$(UninstallKeepData)" IDYES keepdata
    
    ; 删除用户数据
    RMDir /r "$APPDATA\EnhanceVision"
    
    keepdata:
    
    ; 删除安装文件
    RMDir /r "$INSTDIR"
    
    ; 删除快捷方式
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    
    ; 删除注册�?    DeleteRegKey HKLM "${APP_UNINSTALL_KEY}"
    DeleteRegKey HKLM "${APP_REGISTRY_KEY}"
SectionEnd

; ----------------------------------------------------------------------------
; 卸载初始�?; ----------------------------------------------------------------------------
Function un.onInit
    MessageBox MB_OKCANCEL|MB_ICONQUESTION "$(UninstallConfirm)" IDOK +2
    Abort
FunctionEnd
```

---

## 高级功能实现

### 1. 语言选择机制

#### 工作流程

```
┌─────────────────────────────────────────────────────────────�?�?                    安装程序启动                             �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             检测系统语言（GetUserDefaultUILanguage�?       �?�?             中文系统 �?默认选中中文                         �?�?             其他系统 �?默认选中英文                         �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             显示语言选择对话�?                             �?�?             用户可手动切换语言                              �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             后续安装界面使用选定语言                        �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             安装完成时写入配置文�?                         �?�?             %APPDATA%\EnhanceVision\settings.ini           �?�?             [General]                                       �?�?             language=zh_CN �?en_US                         �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             应用启动时读取配�?                             �?�?             SettingsController::loadSettings()              �?�?             Theme.setLanguage(lang)                         �?└─────────────────────────────────────────────────────────────�?```

#### 配置文件格式

应用配置文件位于 `%APPDATA%\EnhanceVision\settings.ini`�?
```ini
[General]
language=zh_CN
customDataPath=D:\MyData\EnhanceVision
theme=dark
```

### 2. 数据目录选择机制

#### 工作流程

```
┌─────────────────────────────────────────────────────────────�?�?             安装过程中显示数据目录选择页面                  �?└─────────────────────────────────────────────────────────────�?                              �?              ┌───────────────┴───────────────�?              �?                              �?┌─────────────────────────�?    ┌─────────────────────────────�?�?   使用默认位置          �?    �?   自定义位�?              �?�?   %APPDATA%\           �?    �?   用户选择目录             �?�?   EnhanceVision        �?    �?                            �?└─────────────────────────�?    └─────────────────────────────�?              �?                              �?              └───────────────┬───────────────�?                              �?┌─────────────────────────────────────────────────────────────�?�?             创建数据目录（如果不存在�?                     �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             写入配置文件                                    �?�?             如果是自定义路径，写�?customDataPath           �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             应用启动时读�?                                 �?�?             SettingsController::effectiveDataPath()         �?�?             返回实际使用的数据路�?                         �?└─────────────────────────────────────────────────────────────�?```

#### 数据目录结构

```
EnhanceVision/
├── ai_image/              # AI 图像处理缓存
├── ai_video/              # AI 视频处理缓存
├── shader_image/          # Shader 图像处理缓存
├── shader_video/          # Shader 视频处理缓存
├── logs/                  # 日志文件
└── thumbnails/            # 缩略图缓�?```

### 3. 用户数据保护机制

#### 覆盖安装流程

```
┌─────────────────────────────────────────────────────────────�?�?             检测到旧版本安�?                               �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             提示用户：是否覆盖安装？                        �?�?             （用户数据将被保留）                            �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             备份用户配置文件                                �?�?             %APPDATA%\EnhanceVision\settings.ini           �?�?             �?%TEMP%\EnhanceVision_backup\                 �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             安装新版本文�?                                 �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             恢复用户配置文件                                �?�?             合并新设置（如语言、数据目录）                  �?└─────────────────────────────────────────────────────────────�?```

#### 卸载时数据保�?
```
┌─────────────────────────────────────────────────────────────�?�?             用户执行卸载                                    �?└─────────────────────────────────────────────────────────────�?                              �?                              �?┌─────────────────────────────────────────────────────────────�?�?             询问：是否保留用户数据？                        �?└─────────────────────────────────────────────────────────────�?              �?                              �?              �?                              �?┌─────────────────────────�?    ┌─────────────────────────────�?�?   保留数据              �?    �?   删除数据                 �?�?   保留 settings.ini     �?    �?   删除 %APPDATA%\          �?�?   保留缓存文件          �?    �?   EnhanceVision\           �?└─────────────────────────�?    └─────────────────────────────�?```

### 4. 运行库检测与安装

#### 检测逻辑

```nsis
; 检�?VC++ 2022 x64 运行�?ReadRegDword $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
${If} $0 == 1
    ; 已安�?${Else}
    ; 未安装，自动安装
    ExecWait '"vc_redist.x64.exe" /quiet /norestart'
${EndIf}
```

#### 内嵌运行�?
将运行库安装程序放入 `installer/redist/` 目录�?
```
installer/redist/
├── vc_redist.x64.exe      # VC++ 2022 Redistributable (x64)
└── dxwebsetup.exe         # DirectX Web Setup（可选）
```

### 5. Vulkan 支持检�?
#### 检测逻辑

```nsis
; 检�?Vulkan 驱动
${If} ${FileExists} "$SYSDIR\vulkan-1.dll"
    ; Vulkan 支持
${Else}
    ; 不支持，显示警告和驱动下载链�?    MessageBox MB_ICONEXCLAMATION|MB_OK "未检测到 Vulkan 支持..."
${EndIf}
```

#### 驱动下载链接

| GPU 厂商 | 下载链接 |
|----------|----------|
| NVIDIA | https://www.nvidia.com/drivers |
| AMD | https://www.amd.com/support |
| Intel | https://downloadcenter.intel.com |

---

## 便携�?ZIP

### 1. 创建便携�?
```powershell
$version = "0.1.0"
$sourceDir = "packaging\output\EnhanceVision-v$version-windows-x64"
$zipFile = "packaging\output\EnhanceVision-v$version-windows-x64-portable.zip"

# 使用 PowerShell 压缩
Compress-Archive -Path $sourceDir -DestinationPath $zipFile -Force

# 或使�?7-Zip（推荐，压缩率更高）
# 7z a -tzip $zipFile $sourceDir
```

### 2. 便携版启动器脚本

创建 `packaging\portable\start.bat`�?
```batch
@echo off
REM EnhanceVision 便携版启动器

REM 设置数据目录为当前目录下�?data 文件�?set APPDATA=%~dp0data

REM 启动程序
start "" "%~dp0EnhanceVision.exe"
```

### 3. 验证便携�?
```powershell
# 解压测试
Expand-Archive -Path $zipFile -DestinationPath "test-portable" -Force

# 运行测试
.\test-packaging\portable\EnhanceVision-v$version-windows-x64\EnhanceVision.exe

# 清理测试
Remove-Item -Recurse -Force "test-portable"
```

---

## 打包验证

### 自动化验证脚�?
创建 `packaging\scripts\verify-package.ps1`:

```powershell
param(
    [string]$Version = "0.1.0"
)

$packageDir = "packaging\output\EnhanceVision-v$Version-windows-x64"
$errors = @()
$warnings = @()

# 检查必要文�?$requiredFiles = @(
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

# 检查模型文�?if (-not (Test-Path "$packageDir\models")) {
    $errors += "缺少 models 目录"
} else {
    $modelFiles = Get-ChildItem -Path "$packageDir\models" -Recurse -Filter "*.param"
    if ($modelFiles.Count -eq 0) {
        $errors += "models 目录中没�?AI 模型文件"
    }
}

# 检查翻译文�?if (-not (Test-Path "$packageDir\translations")) {
    $warnings += "缺少 translations 目录"
}

# 检�?Qt DLL
if (-not (Test-Path "$packageDir\Qt6Core.dll")) {
    $errors += "缺少 Qt DLL 文件"
}

# 检查多媒体后端
$mediaPlugins = @(
    "multimedia\ffmpegmediaplugin.dll",
    "multimedia\windowsmediaplugin.dll",
    "qml\QtMultimedia\quickmultimediaplugin.dll"
)
foreach ($file in $mediaPlugins) {
    if (-not (Test-Path "$packageDir\$file")) {
        $errors += "缺少多媒体插�? $file"
    }
}

# 输出结果
if ($errors.Count -eq 0) {
    Write-Host "�?打包验证通过" -ForegroundColor Green
} else {
    Write-Host "�?打包验证失败:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
    exit 1
}
```

运行验证�?```powershell
.\packaging\scripts\verify-package.ps1 -Version "0.1.0"
```

---

## 完整打包流程

创建 `packaging\scripts\build-package.ps1`:

```powershell
param(
    [string]$Version = "0.1.0"
)

Write-Host "=== EnhanceVision 打包脚本 ===" -ForegroundColor Cyan
Write-Host "版本: $Version" -ForegroundColor Cyan

# 1. 构建 Release 版本
Write-Host "`n[1/6] 构建 Release 版本..." -ForegroundColor Yellow
cmake --preset windows-msvc-2022-release
cmake --build build/msvc2022/Release --config Release -j 8
if ($LASTEXITCODE -ne 0) { throw "构建失败" }

# 2. 准备打包目录
Write-Host "`n[2/6] 准备打包目录..." -ForegroundColor Yellow
$packageDir = "packaging\output\EnhanceVision-v$Version-windows-x64"
Remove-Item -Recurse -Force $packageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

# 复制 Release 运行目录
Copy-Item -Path "build\msvc2022\Release\Release\*" -Destination $packageDir -Recurse

# 补齐 Qt 运行时依赖，避免安装版和便携版缺少多媒体后端
& "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" `
    --release `
    --qmldir "qml" `
    --dir $packageDir `
    "$packageDir\EnhanceVision.exe"

# 复制项目说明文件
Copy-Item -Path "LICENSE" -Destination $packageDir
Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir

# 创建 README.txt
@"
EnhanceVision v$Version

基于 Qt 6 + QML 的桌面端图像与视频画质增强工具�?
快速开�?
1. 双击 EnhanceVision.exe 启动程序
2. 添加图像或视频文�?3. 选择 Shader 模式�?AI 模式
4. 调整参数并导�?
更多信息请访�? https://github.com/K-irito02/EnhanceVision

许可�? MIT License
"@ | Out-File -FilePath "$packageDir\README.txt" -Encoding UTF8

# 3. 验证打包
Write-Host "`n[3/6] 验证打包..." -ForegroundColor Yellow
.\packaging\scripts\verify-package.ps1 -Version $Version
if ($LASTEXITCODE -ne 0) { throw "打包验证失败" }

# 4. 创建安装程序
Write-Host "`n[4/6] 创建安装程序..." -ForegroundColor Yellow
"E:\Program Files (x86)\NSIS\makensis.exe" /INPUTCHARSET UTF8 packaging\installer\setup.nsi
if ($LASTEXITCODE -ne 0) { throw "创建安装程序失败" }

# 5. 创建便携�?Write-Host "`n[5/6] 创建便携�?.." -ForegroundColor Yellow
$zipFile = "packaging\output\EnhanceVision-v$Version-windows-x64-portable.zip"
Remove-Item -Force $zipFile -ErrorAction SilentlyContinue

# 复制便携版启动器
Copy-Item -Path "packaging\portable\start.bat" -Destination $packageDir

Compress-Archive -Path $packageDir -DestinationPath $zipFile

# 清理临时文件
Remove-Item "$packageDir\start.bat" -ErrorAction SilentlyContinue

# 6. 计算校验�?Write-Host "`n[6/6] 计算校验�?.." -ForegroundColor Yellow
$installerHash = Get-FileHash "packaging\output\EnhanceVision-v$Version-windows-x64-installer.exe" -Algorithm SHA256
$portableHash = Get-FileHash $zipFile -Algorithm SHA256

Write-Host "`n=== 打包完成 ===" -ForegroundColor Green
Write-Host "安装程序: packaging\output\EnhanceVision-v$Version-windows-x64-installer.exe"
Write-Host "便携�? packaging\output\EnhanceVision-v$Version-windows-x64-portable.zip"

# 显示文件大小
$installerSize = (Get-Item "packaging\output\EnhanceVision-v$Version-windows-x64-installer.exe").Length / 1MB
$portableSize = (Get-Item $zipFile).Length / 1MB
Write-Host "`n文件大小:"
Write-Host "  安装程序: $([math]::Round($installerSize, 2)) MB"
Write-Host "  便携�? $([math]::Round($portableSize, 2)) MB"

Write-Host "`nSHA256 校验�?"
Write-Host "  安装程序: $($installerHash.Hash)"
Write-Host "  便携�? $($portableHash.Hash)"
```

运行完整打包�?```powershell
.\packaging\scripts\build-package.ps1 -Version "0.1.0"
```

---

## 安装程序资源准备

### 1. 品牌图片规格

| 图片 | 尺寸 | 格式 | 用�?|
|------|------|------|------|
| app_icon.ico | 256 x 256 | ICO | 应用图标（来�?resources/icons/�?|

### 2. 许可协议文件

创建 `installer/license/license_zh.txt`�?
```
EnhanceVision 最终用户许可协�?
版权所�?(c) 2025 EnhanceVision Contributors

特此免费授予任何获得本软件副本和相关文档文件�?软件"）的人不受限制地处置该软件的权利...
```

创建 `installer/license/license_en.txt`�?
```
EnhanceVision End User License Agreement

Copyright (c) 2025 EnhanceVision Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy...
```

---

## NSIS 已知坑点

### 坑点 1: `LicenseLangString` 必须�?`MUI_LANGUAGE` 之后

`!insertmacro MUI_LANGUAGE` 定义�?`${LANG_SIMPCHINESE}` �?`${LANG_ENGLISH}` 等语言 ID 常量。如�?`LicenseLangString` �?`MUI_LANGUAGE` 之前使用这些常量，会导致编译错误�?
```nsis
; �?正确顺序
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"
LicenseLangString LicenseFile ${LANG_SIMPCHINESE} "license_zh.txt"
LicenseLangString LicenseFile ${LANG_ENGLISH} "license_en.txt"

; �?错误顺序 - LANG_SIMPCHINESE 未定�?LicenseLangString LicenseFile ${LANG_SIMPCHINESE} "license_zh.txt"
!insertmacro MUI_LANGUAGE "SimpChinese"
```

### 坑点 2: 许可证页面路径不能使�?`$LANGUAGE` 变量

`!insertmacro MUI_PAGE_LICENSE` 的路径在编译时解析，�?`$LANGUAGE` 是运行时变量。使�?`license_$LANGUAGE.txt` 会导致编译时找不到文件�?
```nsis
; �?正确方式 - 使用 LicenseLangString
LicenseLangString LicenseFile ${LANG_SIMPCHINESE} "license_zh.txt"
LicenseLangString LicenseFile ${LANG_ENGLISH} "license_en.txt"
!insertmacro MUI_PAGE_LICENSE "$(LicenseFile)"

; �?错误方式 - $LANGUAGE 在编译时不可�?!insertmacro MUI_PAGE_LICENSE "license_$LANGUAGE.txt"
```

### 坑点 3: `MB_ICONWARNING` 不是 NSIS 有效常量

NSIS �?MessageBox 不支�?`MB_ICONWARNING`，应使用 `MB_ICONEXCLAMATION` 替代�?
```nsis
; �?正确
MessageBox MB_ICONEXCLAMATION|MB_OK "警告信息"

; �?错误 - 编译失败
MessageBox MB_OK|MB_ICONWARNING "警告信息"
```

### 坑点 4: NSIS 脚本文件编码

如果 NSIS 脚本包含中文字符（如 LangString），必须使用 UTF-8 BOM 编码保存，或在编译时指定字符集：

```powershell
# 方式 1：指定输入字符集
makensis /INPUTCHARSET UTF8 packaging\installer\setup.nsi

# 方式 2：保存为 UTF-8 BOM 编码（NSIS 自动识别�?$content = Get-Content -Path "setup.nsi" -Raw -Encoding UTF8
$utf8Bom = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText("setup.nsi", $content, $utf8Bom)
```

### 坑点 5: 安装区段避免使用 `File /r` 通配

`File /r "..\packaging\output\*.*"` 会将打包目录中的所有文件都包含进去，包括不需要的测试文件、构建产物等。应使用逐文件列举方式精确控制打包内容。

### 坑点 6: 可选文件使用 `File /nonfatal`

对于可能不存在的文件（如 VC++ 运行库），应使用 `File /nonfatal` 参数，并结合 `${If} ${FileExists}` 条件判断：

```nsis
${If} $VCRuntimeInstalled == 0
    SetOutPath "$TEMP\vc_redist"
    File /nonfatal "redist\vc_redist.x64.exe"
    ${If} ${FileExists} "$TEMP\vc_redist\vc_redist.x64.exe"
        ExecWait '"$TEMP\vc_redist\vc_redist.x64.exe" /quiet /norestart' $0
    ${EndIf}
    RMDir /r "$TEMP\vc_redist"
${EndIf}
```

这样即使 `vc_redist.x64.exe` 文件不存在，NSIS 编译也不会失败。

### 坑点 7: PowerShell 脚本避免使用中文字符

PowerShell 脚本中的中文字符可能导致解析错误，特别是在某些系统环境下。建议：

1. **使用英文输出**：所有 `Write-Host` 输出使用英文
2. **避免中文注释**：使用英文注释或在文件开头添加 BOM 编码
3. **错误信息**：使用英文错误信息，便于调试和国际化

```powershell
# 推荐
Write-Host "Package verification passed" -ForegroundColor Green

# 避免
Write-Host "打包验证通过" -ForegroundColor Green
```

---

## 常见问题

### Q1: NSIS 编译失败

**错误**: `!include: could not find: MUI2.nsh`

**解决**: 安装 NSIS 时确保选择�?"Modern UI 2" 组件�?
### Q2: 安装程序体积过大

**解决**: 
- 检查是否包含不必要的文�?- 使用 7-Zip �?LZMA 压缩（需�?NSIS 插件�?- 考虑�?AI 模型单独提供下载

### Q3: 安装后程序无法启�?
**解决**:
- 检查所�?DLL 是否正确复制
- 检�?Qt 平台插件是否存在
- 检�?FFmpeg DLL 是否存在

### Q4: 卸载后残留文�?
**解决**: 检�?NSIS 脚本中的卸载区段，确保删除所有文件�?
### Q5: 语言设置不生�?
**解决**: 
- 检查配置文件是否正确写�?- 检查应用是否正确读取配�?- 确认翻译文件存在

### Q6: 自定义数据目录不生效

**解决**:
- 检查配置文件中 `customDataPath` 是否正确
- 确认目录权限
- 检�?`effectiveDataPath()` 返回�?
---

## 下一�?
打包完成后，请参�?[04-github-release-guide.md](04-github-release-guide.md) 进行发布�?
---

*最后更�? 2025�?
