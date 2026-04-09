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
OutFile "..\package\EnhanceVision-v${APP_VERSION}-windows-x64-installer.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${APP_REGISTRY_KEY}" "Install_Dir"
RequestExecutionLevel admin

SetCompressor /SOLID lzma
SetCompressorDictSize 64

; ----------------------------------------------------------------------------
; 界面设置
; ----------------------------------------------------------------------------
!define MUI_ICON "..\resources\icons\app_icon.ico"
!define MUI_UNICON "..\resources\icons\app_icon.ico"

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
!insertmacro MUI_PAGE_DIRECTORY
Page custom DataDirectoryPage
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
LicenseLangString LicenseFile ${LANG_SIMPCHINESE} "..\installer\license\license_zh.txt"
LicenseLangString LicenseFile ${LANG_ENGLISH} "..\installer\license\license_en.txt"

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
    Call CheckVCRuntime
    Call CheckVulkanSupport
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

    ${NSD_CreateRadioButton} 20 20 100% 12u "$(DataDirDefault)"
    Pop $1
    ${NSD_AddStyle} $1 ${WS_GROUP}
    ${NSD_OnClick} $1 OnSelectDefaultDataDir
    ${NSD_Check} $1

    ${NSD_CreateLabel} 40 40 100% 12u "$INSTDIR\data"
    Pop $2

    ${NSD_CreateRadioButton} 20 70 100% 12u "$(DataDirCustom)"
    Pop $3
    ${NSD_OnClick} $3 OnSelectCustomDataDir

    ${NSD_CreateDirRequest} 40 90 280 12u "$INSTDIR\data"
    Pop $4

    ${NSD_CreateBrowseButton} 330 90 50 12u "$(DataDirBrowse)"
    Pop $5
    ${NSD_OnClick} $5 OnBrowseDataDir

    StrCpy $UseDefaultDataDir 1
    StrCpy $DataDirPath "$INSTDIR\data"

    nsDialogs::Show
FunctionEnd

Function OnSelectDefaultDataDir
    StrCpy $UseDefaultDataDir 1
    StrCpy $DataDirPath "$INSTDIR\data"
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

    SetOutPath $INSTDIR

    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\EnhanceVision.exe"

    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Core.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Gui.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Qml.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Quick.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Impl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Basic.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2BasicStyleImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Fusion.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2FusionStyleImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Material.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2MaterialStyleImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Imagine.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2ImagineStyleImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2Universal.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2UniversalStyleImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2WindowsStyleImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickControls2FluentWinUI3StyleImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickTemplates2.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickLayouts.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickShapes.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickEffects.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickDialogs2.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickDialogs2QuickImpl.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickDialogs2Utils.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QuickWidgets.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Network.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6OpenGL.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Multimedia.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6MultimediaQuick.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlModels.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlCore.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlMeta.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6QmlWorkerScript.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6LabsFolderListModel.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6LabsQmlModels.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Widgets.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Svg.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Sql.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\Qt6Quick3DUtils.dll"

    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\D3Dcompiler_47.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\dxcompiler.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\dxil.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\opengl32sw.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\icuuc.dll"

    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\avcodec-62.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\avformat-62.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\avutil-60.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\swscale-9.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\swresample-6.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\avdevice-62.dll"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\avfilter-11.dll"

    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\LICENSE"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\THIRD_PARTY_LICENSES.md"
    File "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\README.txt"

    SetOutPath $INSTDIR\models
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\models\*.*"

    SetOutPath $INSTDIR\translations
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\translations\*.*"

    SetOutPath $INSTDIR\qml
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\qml\*.*"

    SetOutPath $INSTDIR\platforms
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\platforms\*.*"

    SetOutPath $INSTDIR\styles
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\styles\*.*"

    SetOutPath $INSTDIR\imageformats
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\imageformats\*.*"

    SetOutPath $INSTDIR\iconengines
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\iconengines\*.*"

    SetOutPath $INSTDIR\generic
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\generic\*.*"

    SetOutPath $INSTDIR\networkinformation
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\networkinformation\*.*"

    SetOutPath $INSTDIR\tls
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\tls\*.*"

    SetOutPath $INSTDIR\sqldrivers
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\sqldrivers\*.*"

    SetOutPath $INSTDIR\multimedia
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\multimedia\*.*"

    SetOutPath $INSTDIR\qmltooling
    File /r "..\package\EnhanceVision-v${APP_VERSION}-windows-x64\qmltooling\*.*"

    ${If} $VCRuntimeInstalled == 0
        SetOutPath "$TEMP\vc_redist"
        File "..\installer\redist\vc_redist.x64.exe"
        ExecWait '"$TEMP\vc_redist\vc_redist.x64.exe" /quiet /norestart' $0
        RMDir /r "$TEMP\vc_redist"
    ${EndIf}

    CreateDirectory "$DataDirPath"
    SetShellVarContext current
    CreateDirectory "$LOCALAPPDATA\EnhanceVision"
    WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "behavior" "customDataPath" "$DataDirPath"

    ${If} $LANGUAGE == 2052
        WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "appearance" "language" "zh_CN"
    ${Else}
        WriteINIStr "$LOCALAPPDATA\EnhanceVision\settings.ini" "appearance" "language" "en_US"
    ${EndIf}

    ${If} $PreviousVersion != ""
        IfFileExists "$TEMP\EnhanceVision_backup\settings.ini" 0 +3
        CopyFiles "$TEMP\EnhanceVision_backup\settings.ini" "$LOCALAPPDATA\EnhanceVision\"
        RMDir /r "$TEMP\EnhanceVision_backup"
    ${EndIf}

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "Version" "${APP_VERSION}"
    WriteRegStr HKLM "${APP_REGISTRY_KEY}" "DataDir" "$DataDirPath"

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
