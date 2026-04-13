; ============================================================================
; EnhanceVision 安装程序脚本
; 使用 NSIS 3.x 编译
; ============================================================================

; ----------------------------------------------------------------------------
; 包含文件
; ----------------------------------------------------------------------------
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!include "WordFunc.nsh"
!include "nsDialogs.nsh"

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

!define MIN_DISK_SPACE_MB 500

!define VC_REDIST_MIN_VERSION "14.32.31326.0"

; ----------------------------------------------------------------------------
; 安装程序信息
; ----------------------------------------------------------------------------
Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\output\EnhanceVision-v${APP_VERSION}-windows-x64-installer.exe"
InstallDir "$LOCALAPPDATA\Programs\${APP_NAME}"
InstallDirRegKey HKLM "${APP_REGISTRY_KEY}" "Install_Dir"
RequestExecutionLevel admin

SetCompressor /SOLID lzma
SetCompressorDictSize 64

; ----------------------------------------------------------------------------
; 界面设置
; ----------------------------------------------------------------------------
!define MUI_ICON "..\..\resources\icons\app_icon.ico"
!define MUI_UNICON "..\..\resources\icons\app_icon.ico"

!define MUI_LANGDLL_REGISTRY_ROOT "HKLM"
!define MUI_LANGDLL_REGISTRY_KEY "${APP_REGISTRY_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

!define MUI_FINISHPAGE_RUN "$INSTDIR\EnhanceVision.exe"
!define MUI_FINISHPAGE_RUN_TEXT "$(FinishRunText)"
!define MUI_FINISHPAGE_LINK "$(FinishLinkText)"
!define MUI_FINISHPAGE_LINK_LOCATION "${APP_URL}"

; ----------------------------------------------------------------------------
; 页面定义
; ----------------------------------------------------------------------------
!insertmacro MUI_PAGE_LICENSE "$(LicenseFile)"
Page custom DataDirectoryPage DataDirectoryPageLeave
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; ----------------------------------------------------------------------------
; 多语言支持（必须在 LicenseLangString 之前）
; ----------------------------------------------------------------------------
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; ----------------------------------------------------------------------------
; 多语言许可证（在 MUI_LANGUAGE 之后）
; ----------------------------------------------------------------------------
LicenseLangString LicenseFile ${LANG_SIMPCHINESE} "license\license_zh.txt"
LicenseLangString LicenseFile ${LANG_ENGLISH} "license\license_en.txt"

; ----------------------------------------------------------------------------
; 语言字符串 - 简体中文
; ----------------------------------------------------------------------------
LangString FinishRunText ${LANG_SIMPCHINESE} "运行 EnhanceVision"
LangString FinishLinkText ${LANG_SIMPCHINESE} "访问项目主页"
LangString ErrorNot64Bit ${LANG_SIMPCHINESE} "此程序需要 64 位 Windows 系统。"
LangString ErrorDiskSpace ${LANG_SIMPCHINESE} "磁盘空间不足！至少需要 ${MIN_DISK_SPACE_MB} MB 可用空间。"
LangString ErrorVCRuntime ${LANG_SIMPCHINESE} "未检测到 Visual C++ 运行库，将自动安装。"
LangString WarningVulkan ${LANG_SIMPCHINESE} "未检测到 Vulkan 支持。AI 增强功能将使用 CPU 模式，性能可能较慢。建议更新显卡驱动以获得最佳体验。"
LangString DataDirTitle ${LANG_SIMPCHINESE} "数据存储位置"
LangString DataDirText ${LANG_SIMPCHINESE} "请设置安装目录和默认导出路径。应用数据目录将自动创建于安装目录下。"
LangString InstallDirLabel ${LANG_SIMPCHINESE} "安装目录"
LangString InstallDirHint ${LANG_SIMPCHINESE} "程序文件安装位置"
LangString AppDataDirLabel ${LANG_SIMPCHINESE} "应用数据目录（跟随安装目录）"
LangString AppDataDirHint ${LANG_SIMPCHINESE} "用于保存处理结果、缩略图、日志等数据"
LangString ExportDirLabel ${LANG_SIMPCHINESE} "默认导出路径"
LangString ExportDirHint ${LANG_SIMPCHINESE} "处理后文件的默认保存位置"
LangString InstallDirProtectedWarn ${LANG_SIMPCHINESE} "当前安装目录位于受保护目录，程序可以继续安装，但建议改为普通可写目录以减少权限问题。$\r$\n$\r$\n当前目录：$INSTDIR$\r$\n应用数据目录将自动使用：$DataDirPath"
LangString DataDirBrowse ${LANG_SIMPCHINESE} "浏览..."
LangString InvalidDirEmpty ${LANG_SIMPCHINESE} "目录不能为空。"
LangString InvalidDirProtected ${LANG_SIMPCHINESE} "所选目录位于 Windows 受保护目录下，请改为普通可写目录。"
LangString InvalidDirNotAbsolute ${LANG_SIMPCHINESE} "所选目录必须是绝对路径。"
LangString InvalidDirCreateFailed ${LANG_SIMPCHINESE} "无法创建或写入所选目录，请检查权限后重试。"
 LangString SectionMain ${LANG_SIMPCHINESE} "主程序（必需）"
 LangString SectionMainDesc ${LANG_SIMPCHINESE} "安装 EnhanceVision 主程序和核心组件"
LangString SectionShortcuts ${LANG_SIMPCHINESE} "开始菜单快捷方式"
LangString SectionShortcutsDesc ${LANG_SIMPCHINESE} "在开始菜单创建程序组"
LangString SectionDesktop ${LANG_SIMPCHINESE} "桌面快捷方式"
LangString SectionDesktopDesc ${LANG_SIMPCHINESE} "在桌面创建快捷方式"
LangString UninstallConfirm ${LANG_SIMPCHINESE} "确定要卸载 EnhanceVision 吗？"
LangString UninstallKeepData ${LANG_SIMPCHINESE} "是否保留用户数据？"
LangString PrevVersionDetected ${LANG_SIMPCHINESE} "检测到已安装 EnhanceVision $PreviousVersion$\r$\n$\r$\n是否覆盖安装？（用户数据将被保留）"

; ----------------------------------------------------------------------------
; 语言字符串 - 英文
; ----------------------------------------------------------------------------
LangString FinishRunText ${LANG_ENGLISH} "Run EnhanceVision"
LangString FinishLinkText ${LANG_ENGLISH} "Visit project homepage"
LangString ErrorNot64Bit ${LANG_ENGLISH} "This program requires a 64-bit Windows system."
LangString ErrorDiskSpace ${LANG_ENGLISH} "Insufficient disk space! At least ${MIN_DISK_SPACE_MB} MB is required."
LangString ErrorVCRuntime ${LANG_ENGLISH} "Visual C++ runtime not detected. It will be installed automatically."
LangString WarningVulkan ${LANG_ENGLISH} "Vulkan support not detected. AI enhancement will use CPU mode, which may be slower. It is recommended to update your graphics driver for the best experience."
LangString DataDirTitle ${LANG_ENGLISH} "Data Storage Location"
LangString DataDirText ${LANG_ENGLISH} "Set the install directory and default export path. The application data directory will be created automatically under the install directory."
LangString InstallDirLabel ${LANG_ENGLISH} "Install Directory"
LangString InstallDirHint ${LANG_ENGLISH} "Program files location"
LangString AppDataDirLabel ${LANG_ENGLISH} "Application Data Directory (follows install directory)"
LangString AppDataDirHint ${LANG_ENGLISH} "Stores processing results, thumbnails, logs and other data"
LangString ExportDirLabel ${LANG_ENGLISH} "Default Export Path"
LangString ExportDirHint ${LANG_ENGLISH} "Default save location for processed files"
LangString InstallDirProtectedWarn ${LANG_ENGLISH} "The selected install directory is in a protected location. Installation can continue, but using a regular writable directory is recommended to reduce permission issues.$\r$\n$\r$\nCurrent install directory: $INSTDIR$\r$\nApplication data directory will use: $DataDirPath"
LangString DataDirBrowse ${LANG_ENGLISH} "Browse..."
LangString InvalidDirEmpty ${LANG_ENGLISH} "The directory cannot be empty."
LangString InvalidDirProtected ${LANG_ENGLISH} "The selected directory is inside a Windows protected directory. Choose a regular writable directory instead."
LangString InvalidDirNotAbsolute ${LANG_ENGLISH} "The selected directory must be an absolute path."
LangString InvalidDirCreateFailed ${LANG_ENGLISH} "The selected directory cannot be created or written. Check permissions and try again."
LangString InvalidInstallDirProtected ${LANG_ENGLISH} "The install directory cannot be inside Windows protected directories when application data is stored alongside the app. Choose a regular writable directory."
LangString InvalidInstallDirProtected ${LANG_SIMPCHINESE} "当应用数据与程序同路径存放时，安装目录不能位于 Windows 受保护目录下，请选择普通可写目录。"
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
Var ExportDirPath
Var PreviousVersion
Var VulkanSupported
Var VCRuntimeInstalled
Var PreviousDataDirPath
Var PreviousExportDirPath
Var ExportDirInputHandle
Var DataDirDisplayHandle
Var DataDirPathIniSafe
Var ExportDirPathIniSafe
Var PreviousDataDirPathIniSafe
Var InstallDirInputHandle

; ----------------------------------------------------------------------------
; 安装初始化
; ----------------------------------------------------------------------------
Function .onInit
    !insertmacro MUI_LANGDLL_DISPLAY

    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorNot64Bit)"
        Abort
    ${EndIf}

    Call CheckDiskSpace
    Call CheckPreviousVersion
    Call InitializeDirectoryDefaults
    Call CheckVCRuntime
    Call CheckVulkanSupport
FunctionEnd

; ----------------------------------------------------------------------------
; 数据目录选择页面
; ----------------------------------------------------------------------------
Function DataDirectoryPage
    !insertmacro MUI_HEADER_TEXT "$(DataDirTitle)" "$(DataDirText)"

    Call SyncDataDirWithInstallDir

    nsDialogs::Create 1018
    Pop $0

    ${If} $0 == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 20 14 100% 12u "$(InstallDirLabel)"
    Pop $0

    ${NSD_CreateDirRequest} 20 30 300 13u "$INSTDIR"
    Pop $InstallDirInputHandle

    ${NSD_CreateBrowseButton} 330 30 50 13u "$(DataDirBrowse)"
    Pop $0
    ${NSD_OnClick} $0 OnBrowseInstallDir

    ${NSD_CreateLabel} 20 50 100% 24u "$(InstallDirHint)"
    Pop $0

    ${NSD_CreateLabel} 20 80 100% 12u "$(AppDataDirLabel)"
    Pop $0

    ${NSD_CreateText} 20 96 360 13u "$DataDirPath"
    Pop $DataDirDisplayHandle
    EnableWindow $DataDirDisplayHandle 0

    ${NSD_CreateLabel} 20 116 100% 24u "$(AppDataDirHint)"
    Pop $0

    ${NSD_CreateLabel} 20 146 100% 12u "$(ExportDirLabel)"
    Pop $0

    ${NSD_CreateDirRequest} 20 162 300 13u "$ExportDirPath"
    Pop $ExportDirInputHandle

    ${NSD_CreateBrowseButton} 330 162 50 13u "$(DataDirBrowse)"
    Pop $0
    ${NSD_OnClick} $0 OnBrowseExportDir

    ${NSD_CreateLabel} 20 182 100% 24u "$(ExportDirHint)"
    Pop $0

    nsDialogs::Show
FunctionEnd

Function OnBrowseInstallDir
    nsDialogs::SelectFolderDialog "" "$INSTDIR"
    Pop $0
    ${If} $0 != error
        StrCpy $INSTDIR "$0"
        ${NSD_SetText} $InstallDirInputHandle "$INSTDIR"
        Call SyncDataDirWithInstallDir
    ${EndIf}
FunctionEnd

Function OnBrowseExportDir
    nsDialogs::SelectFolderDialog "" "$ExportDirPath"
    Pop $0
    ${If} $0 != error
        StrCpy $ExportDirPath "$0"
        ${NSD_SetText} $ExportDirInputHandle "$ExportDirPath"
    ${EndIf}
FunctionEnd

Function DataDirectoryPageLeave
    ${NSD_GetText} $InstallDirInputHandle $INSTDIR

    Push "$INSTDIR"
    Call ValidateInstallDirectory
    Pop $0
    ${If} $0 != ""
        MessageBox MB_OK|MB_ICONSTOP $0
        Abort
    ${EndIf}

    Call SyncDataDirWithInstallDir
    ${NSD_GetText} $ExportDirInputHandle $ExportDirPath

    Push $ExportDirPath
    Call ValidateDirectorySelection
    Pop $0
    ${If} $0 != ""
        MessageBox MB_OK|MB_ICONSTOP $0
        Abort
    ${EndIf}

    Push "$INSTDIR"
    Call IsProtectedInstallDirectory
    Pop $0
    ${If} $0 == "1"
        MessageBox MB_OK|MB_ICONEXCLAMATION "$(InstallDirProtectedWarn)"
    ${EndIf}
FunctionEnd

; ----------------------------------------------------------------------------
; 系统检测函数
; ----------------------------------------------------------------------------
Function CheckDiskSpace
    ${DriveSpace} "C:" "/D=F /S=M" $0
    ${If} $0 < ${MIN_DISK_SPACE_MB}
        MessageBox MB_OK|MB_ICONSTOP "$(ErrorDiskSpace)"
        Abort
    ${EndIf}
FunctionEnd

Function CheckPreviousVersion
    ReadRegStr $PreviousVersion HKLM "${APP_REGISTRY_KEY}" "Version"

    ${If} $PreviousVersion != ""
        MessageBox MB_OKCANCEL|MB_ICONQUESTION "$(PrevVersionDetected)" IDOK continue
        Abort

        continue:
    ${EndIf}
FunctionEnd

Function InitializeDirectoryDefaults
    SetShellVarContext current

    Push "$INSTDIR"
    Call ResolveDataDirFromInstallDir
    Pop $DataDirPath
    Call ResolveDefaultExportDir
    Pop $ExportDirPath
    StrCpy $PreviousDataDirPath ""
    StrCpy $PreviousExportDirPath ""

    IfFileExists "$LOCALAPPDATA\EnhanceVision\settings.ini" 0 done
    ReadINIStr $0 "$LOCALAPPDATA\EnhanceVision\settings.ini" "behavior" "customDataPath"
    ${If} $0 != ""
        Push $0
        Call FromIniSafePath
        Pop $0
        StrCpy $PreviousDataDirPath "$0"
    ${EndIf}

    ReadINIStr $0 "$LOCALAPPDATA\EnhanceVision\settings.ini" "behavior" "defaultSavePath"
    ${If} $0 != ""
        Push $0
        Call FromIniSafePath
        Pop $0
        StrCpy $ExportDirPath "$0"
        StrCpy $PreviousExportDirPath "$0"
    ${EndIf}

    ${If} $PreviousDataDirPath == ""
        StrCpy $PreviousDataDirPath "$LOCALAPPDATA\EnhanceVision\EnhanceVision"
    ${EndIf}

    done:
FunctionEnd

Function ResolveDefaultExportDir
    ; Prefer D: when available, otherwise fallback to C:.
    ; This avoids assumptions about optional drives such as E:.
    IfFileExists "D:\\" 0 +3
    StrCpy $0 "D:\EnhanceVision\Exports"
    Goto done

    IfFileExists "C:\\" 0 +3
    StrCpy $0 "C:\EnhanceVision\Exports"
    Goto done

    StrCpy $0 "$PROFILE\Pictures\EnhanceVision"

    done:
    Push $0
FunctionEnd

Function SyncDataDirWithInstallDir
    Push "$INSTDIR"
    Call ResolveDataDirFromInstallDir
    Pop $DataDirPath
    ${If} $DataDirDisplayHandle != ""
        ${NSD_SetText} $DataDirDisplayHandle "$DataDirPath"
    ${EndIf}
FunctionEnd

Function ResolveDataDirFromInstallDir
    Exch $0

    StrCpy $1 "$0\data"
    StrCpy $R0 $0
    System::Call 'kernel32::CharUpperBuff(t r0, i ${NSIS_MAX_STRLEN})'

    StrCpy $2 $R0 2
    StrCpy $3 $2 1 1
    ${If} $3 != ":"
        StrCpy $0 "$1"
        Goto done
    ${EndIf}

    StrCpy $R1 "$2\PROGRAM FILES"
    StrLen $4 $R1
    StrCpy $5 $R0 $4
    ${If} $5 == $R1
        StrCpy $0 "$2\EnhanceVision\data"
        Goto done
    ${EndIf}

    StrCpy $R1 "$2\PROGRAM FILES (X86)"
    StrLen $4 $R1
    StrCpy $5 $R0 $4
    ${If} $5 == $R1
        StrCpy $0 "$2\EnhanceVision\data"
        Goto done
    ${EndIf}

    StrCpy $R1 "$2\WINDOWS"
    StrLen $4 $R1
    StrCpy $5 $R0 $4
    ${If} $5 == $R1
        StrCpy $0 "$2\EnhanceVision\data"
        Goto done
    ${EndIf}

    StrCpy $0 "$1"

    done:
    Exch $0
FunctionEnd

Function ToIniSafePath
    Exch $0
    StrCpy $1 ""

    loop:
        StrCpy $2 $0 1
        ${If} $2 == ""
            Goto done
        ${EndIf}
        ${If} $2 == "\"
            StrCpy $2 "/"
        ${EndIf}
        StrCpy $1 "$1$2"
        StrCpy $0 $0 "" 1
        Goto loop

    done:
    StrCpy $0 $1
    Exch $0
FunctionEnd

Function FromIniSafePath
    Exch $0
    StrCpy $1 ""

    loop:
        StrCpy $2 $0 1
        ${If} $2 == ""
            Goto done
        ${EndIf}
        ${If} $2 == "/"
            StrCpy $2 "\"
        ${EndIf}
        StrCpy $1 "$1$2"
        StrCpy $0 $0 "" 1
        Goto loop

    done:
    StrCpy $0 $1
    Exch $0
FunctionEnd

Function IsProtectedInstallDirectory
    Exch $0
    StrCpy $R0 $0
    System::Call 'kernel32::CharUpperBuff(t r0, i ${NSIS_MAX_STRLEN})'

    StrCpy $1 $R0 2
    StrCpy $2 $1 1 1
    ${If} $2 != ":"
        StrCpy $0 "0"
        Goto done
    ${EndIf}

    StrCpy $R1 "$1\PROGRAM FILES"
    StrLen $3 $R1
    StrCpy $4 $R0 $3
    ${If} $4 == $R1
        StrCpy $0 "1"
        Goto done
    ${EndIf}

    StrCpy $R1 "$1\PROGRAM FILES (X86)"
    StrLen $3 $R1
    StrCpy $4 $R0 $3
    ${If} $4 == $R1
        StrCpy $0 "1"
        Goto done
    ${EndIf}

    StrCpy $R1 "$1\WINDOWS"
    StrLen $3 $R1
    StrCpy $4 $R0 $3
    ${If} $4 == $R1
        StrCpy $0 "1"
        Goto done
    ${EndIf}

    StrCpy $0 "0"
    done:
    Exch $0
FunctionEnd

Function ValidateDirectorySelection
    Exch $0

    ${If} $0 == ""
        StrCpy $0 "$(InvalidDirEmpty)"
        Goto done
    ${EndIf}

    StrCpy $1 $0 2
    ${If} $1 != "\\"
        StrCpy $1 $0 3
        ${If} $1 == ""
            StrCpy $0 "$(InvalidDirNotAbsolute)"
            Goto done
        ${EndIf}
        StrCpy $2 $1 1 1
        ${If} $2 != ":"
            StrCpy $0 "$(InvalidDirNotAbsolute)"
            Goto done
        ${EndIf}
    ${EndIf}

    StrCpy $R0 $0
    System::Call 'kernel32::CharUpperBuff(t r0, i ${NSIS_MAX_STRLEN})'

    StrCpy $R1 $PROGRAMFILES64
    System::Call 'kernel32::CharUpperBuff(t r1, i ${NSIS_MAX_STRLEN})'
    StrLen $1 $R1
    StrCpy $2 $R0 $1
    ${If} $2 == $R1
        StrCpy $0 "$(InvalidDirProtected)"
        Goto done
    ${EndIf}

    StrCpy $R1 $PROGRAMFILES32
    System::Call 'kernel32::CharUpperBuff(t r1, i ${NSIS_MAX_STRLEN})'
    StrLen $1 $R1
    StrCpy $2 $R0 $1
    ${If} $2 == $R1
        StrCpy $0 "$(InvalidDirProtected)"
        Goto done
    ${EndIf}

    StrCpy $R1 $WINDIR
    System::Call 'kernel32::CharUpperBuff(t r1, i ${NSIS_MAX_STRLEN})'
    StrLen $1 $R1
    StrCpy $2 $R0 $1
    ${If} $2 == $R1
        StrCpy $0 "$(InvalidDirProtected)"
        Goto done
    ${EndIf}

    StrCpy $3 $R0 2
    StrCpy $4 $3 1 1
    ${If} $4 == ":"
        StrCpy $R1 "$3\PROGRAM FILES"
        StrLen $1 $R1
        StrCpy $2 $R0 $1
        ${If} $2 == $R1
            StrCpy $0 "$(InvalidDirProtected)"
            Goto done
        ${EndIf}

        StrCpy $R1 "$3\PROGRAM FILES (X86)"
        StrLen $1 $R1
        StrCpy $2 $R0 $1
        ${If} $2 == $R1
            StrCpy $0 "$(InvalidDirProtected)"
            Goto done
        ${EndIf}

        StrCpy $R1 "$3\WINDOWS"
        StrLen $1 $R1
        StrCpy $2 $R0 $1
        ${If} $2 == $R1
            StrCpy $0 "$(InvalidDirProtected)"
            Goto done
        ${EndIf}
    ${EndIf}

    CreateDirectory "$0"
    IfFileExists "$0\NUL" 0 create_failed

    ClearErrors
    FileOpen $1 "$0\.enhancevision_write_test.tmp" w
    IfErrors create_failed
    FileClose $1
    Delete "$0\.enhancevision_write_test.tmp"
    StrCpy $0 ""
    Goto done

    create_failed:
    StrCpy $0 "$(InvalidDirCreateFailed)"

    done:
    Exch $0
FunctionEnd

Function .onVerifyInstDir
    Push "$INSTDIR"
    Call ValidateInstallDirectory
    Pop $0
    ${If} $0 != ""
        MessageBox MB_OK|MB_ICONSTOP $0
        Abort
    ${EndIf}
FunctionEnd

Function ValidateInstallDirectory
    Exch $0

    ${If} $0 == ""
        StrCpy $0 "$(InvalidDirEmpty)"
        Goto done
    ${EndIf}

    StrCpy $1 $0 2
    ${If} $1 != "\\"
        StrCpy $1 $0 3
        ${If} $1 == ""
            StrCpy $0 "$(InvalidDirNotAbsolute)"
            Goto done
        ${EndIf}
        StrCpy $2 $1 1 1
        ${If} $2 != ":"
            StrCpy $0 "$(InvalidDirNotAbsolute)"
            Goto done
        ${EndIf}
    ${EndIf}

    CreateDirectory "$0"
    IfFileExists "$0\NUL" 0 create_failed

    ClearErrors
    FileOpen $1 "$0\.enhancevision_install_write_test.tmp" w
    IfErrors create_failed
    FileClose $1
    Delete "$0\.enhancevision_install_write_test.tmp"
    StrCpy $0 ""
    Goto done

    create_failed:
    StrCpy $0 "$(InvalidDirCreateFailed)"

    done:
    Exch $0
FunctionEnd

Function MigrateLeafDirectory
    Exch $1
    Exch
    Exch $0

    ${If} $0 == ""
        Goto done
    ${EndIf}
    ${If} $1 == ""
        Goto done
    ${EndIf}
    ${If} $0 == $1
        Goto done
    ${EndIf}
    IfFileExists "$0\NUL" 0 done

    CreateDirectory "$1"
    CopyFiles /SILENT "$0\*.*" "$1"
    RMDir /r "$0"

    done:
    Pop $0
    Pop $1
FunctionEnd

Function MigrateRuntimeData
    ${If} $PreviousDataDirPath == ""
        Return
    ${EndIf}
    ${If} $PreviousDataDirPath == $DataDirPath
        Return
    ${EndIf}
    IfFileExists "$PreviousDataDirPath\NUL" 0 done

    CreateDirectory "$DataDirPath"

    Push "$PreviousDataDirPath\ai\images"
    Push "$DataDirPath\ai\images"
    Call MigrateLeafDirectory

    Push "$PreviousDataDirPath\ai\videos"
    Push "$DataDirPath\ai\videos"
    Call MigrateLeafDirectory

    Push "$PreviousDataDirPath\shader\images"
    Push "$DataDirPath\shader\images"
    Call MigrateLeafDirectory

    Push "$PreviousDataDirPath\shader\videos"
    Push "$DataDirPath\shader\videos"
    Call MigrateLeafDirectory

    Push "$PreviousDataDirPath\thumbnails"
    Push "$DataDirPath\thumbnails"
    Call MigrateLeafDirectory

    Push "$PreviousDataDirPath\system"
    Push "$DataDirPath\system"
    Call MigrateLeafDirectory

    Push "$PreviousDataDirPath\logs"
    Push "$DataDirPath\logs"
    Call MigrateLeafDirectory

    CopyFiles /SILENT "$PreviousDataDirPath\thumbnail_meta.db*" "$DataDirPath"
    Delete "$PreviousDataDirPath\thumbnail_meta.db"
    Delete "$PreviousDataDirPath\thumbnail_meta.db-shm"
    Delete "$PreviousDataDirPath\thumbnail_meta.db-wal"

    RMDir "$PreviousDataDirPath\ai"
    RMDir "$PreviousDataDirPath\shader"
    RMDir "$PreviousDataDirPath"

    done:
FunctionEnd

Function CheckVCRuntime
    StrCpy $VCRuntimeInstalled 0

    ReadRegDword $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
    ${If} $0 == 1
        StrCpy $VCRuntimeInstalled 1
    ${EndIf}

    ${If} $VCRuntimeInstalled == 0
        MessageBox MB_OK|MB_ICONINFORMATION "$(ErrorVCRuntime)"
    ${EndIf}
FunctionEnd

Function CheckVulkanSupport
    StrCpy $VulkanSupported 0

    ${If} ${FileExists} "$SYSDIR\vulkan-1.dll"
        StrCpy $VulkanSupported 1
    ${EndIf}

    ${If} $VulkanSupported == 0
        StrCpy $0 "$(WarningVulkan)"
        MessageBox MB_ICONEXCLAMATION|MB_OK $0
    ${EndIf}
FunctionEnd

; ----------------------------------------------------------------------------
; 安装区段
; ----------------------------------------------------------------------------

Section "$(SectionMain)" SecMain
    SectionIn RO

    ${If} $PreviousVersion != ""
        IfFileExists "$LOCALAPPDATA\EnhanceVision\settings.ini" 0 +3
        CreateDirectory "$TEMP\EnhanceVision_backup"
        CopyFiles "$LOCALAPPDATA\EnhanceVision\settings.ini" "$TEMP\EnhanceVision_backup\"
    ${EndIf}

    SetShellVarContext current
    SetOutPath $INSTDIR

    File /r /x "start.vbs" "..\output\EnhanceVision-v${APP_VERSION}-windows-x64\*.*"

    ${If} $VCRuntimeInstalled == 0
        SetOutPath "$TEMP\vc_redist"
        File /nonfatal "redist\vc_redist.x64.exe"
        ${If} ${FileExists} "$TEMP\vc_redist\vc_redist.x64.exe"
            ExecWait '"$TEMP\vc_redist\vc_redist.x64.exe" /quiet /norestart' $0
        ${EndIf}
        RMDir /r "$TEMP\vc_redist"
    ${EndIf}

    CreateDirectory "$LOCALAPPDATA\EnhanceVision"
    ${If} $PreviousVersion != ""
        IfFileExists "$TEMP\EnhanceVision_backup\settings.ini" 0 +2
        CopyFiles /SILENT "$TEMP\EnhanceVision_backup\settings.ini" "$LOCALAPPDATA\EnhanceVision\"
    ${EndIf}

    CreateDirectory "$DataDirPath"
    CreateDirectory "$ExportDirPath"

    Push "$DataDirPath"
    Call ToIniSafePath
    Pop $DataDirPathIniSafe
    Push "$ExportDirPath"
    Call ToIniSafePath
    Pop $ExportDirPathIniSafe
    Push "$PreviousDataDirPath"
    Call ToIniSafePath
    Pop $PreviousDataDirPathIniSafe

    WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "behavior" "customDataPath" "$DataDirPathIniSafe"
    ${If} $PreviousDataDirPath != ""
    ${AndIf} $PreviousDataDirPath != $DataDirPath
        WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "behavior" "previousDataPath" "$PreviousDataDirPathIniSafe"
    ${Else}
        WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "behavior" "previousDataPath" ""
    ${EndIf}
    WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "behavior" "defaultSavePath" "$ExportDirPathIniSafe"

    ${If} $LANGUAGE == 2052
        WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "appearance" "language" "zh_CN"
    ${Else}
        WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "appearance" "language" "en_US"
    ${EndIf}

    ${If} $PreviousVersion != ""
        Call MigrateRuntimeData
        RMDir /r "$TEMP\EnhanceVision_backup"
    ${EndIf}

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Version" "${APP_VERSION}"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "DataDir" "$DataDirPath"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "ExportDir" "$ExportDirPath"

    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "URLInfoAbout" "${APP_URL}"
    WriteRegStr HKLM "${APP_UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\EnhanceVision.exe"

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "${APP_UNINSTALL_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "$(SectionShortcuts)" SecShortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "$(SectionDesktop)" SecDesktop
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\EnhanceVision.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "$(SectionMainDesc)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} "$(SectionShortcutsDesc)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "$(SectionDesktopDesc)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ----------------------------------------------------------------------------
; 卸载区段
; ----------------------------------------------------------------------------
Section "Uninstall"
    MessageBox MB_YESNO|MB_ICONQUESTION "$(UninstallKeepData)" IDYES keepdata

    RMDir /r "$LOCALAPPDATA\EnhanceVision"

    keepdata:

    RMDir /r "$INSTDIR"

    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"

    DeleteRegKey HKLM "${APP_UNINSTALL_KEY}"
    DeleteRegKey HKLM "${APP_REGISTRY_KEY}"
SectionEnd

; ----------------------------------------------------------------------------
; 卸载初始化
; ----------------------------------------------------------------------------
Function un.onInit
    !insertmacro MUI_UNGETLANGUAGE

    MessageBox MB_OKCANCEL|MB_ICONQUESTION "$(UninstallConfirm)" IDOK +2
    Abort
FunctionEnd

