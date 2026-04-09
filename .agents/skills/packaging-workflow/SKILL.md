---
name: "packaging-workflow"
description: "EnhanceVision packaging workflow. Invoke when user asks to package, build installer, create portable ZIP, or run packaging scripts."
---

# EnhanceVision Packaging Workflow

Automated packaging workflow for building NSIS installer and portable ZIP.

## Prerequisites

| Requirement | Path/Command |
|-------------|-------------|
| NSIS 3.11 | `E:\Program Files (x86)\NSIS\makensis.exe` |
| CMake | `C:\Program Files\CMake\bin\cmake.exe` |
| Qt 6.10.2 | `E:\Qt\6.10.2\msvc2022_64` |
| VC++ Redistributable | `installer/redist/vc_redist.x64.exe` |

## Key Files

| File | Purpose |
|------|---------|
| `installer/setup.nsi` | NSIS installer script (UTF-8 BOM encoding) |
| `installer/redist/vc_redist.x64.exe` | VC++ 2022 runtime |
| `installer/license/license_zh.txt` | Chinese license (UTF-16 LE BOM) |
| `installer/license/license_en.txt` | English license (UTF-16 LE BOM) |
| `scripts/build-package.ps1` | One-click packaging script |
| `scripts/verify-package.ps1` | Package verification script |
| `portable/start.vbs` | Portable launcher (VBS, no console window) |
| `resources/icons/app_icon.ico` | Application icon |

## Execution Flow

```
1. Build Release → 2. Prepare package dir → 3. Verify → 4. NSIS installer → 5. Portable ZIP → 6. Checksums
```

## Quick Start

```powershell
# Full packaging (build + verify + installer + portable)
.\scripts\build-package.ps1 -Version "0.1.0"

# Verify only
.\scripts\verify-package.ps1 -Version "0.1.0"

# NSIS compile only
& "E:\Program Files (x86)\NSIS\makensis.exe" /INPUTCHARSET UTF8 installer\setup.nsi
```

## NSIS Known Pitfalls

1. **LicenseLangString order**: Must come AFTER `!insertmacro MUI_LANGUAGE`
2. **License page path**: Use `$(LicenseFile)` not `license_$LANGUAGE.txt`
3. **MB_ICONWARNING**: Not valid in NSIS, use `MB_ICONEXCLAMATION`
4. **File encoding**: Must use UTF-8 BOM or `/INPUTCHARSET UTF8`
5. **File /r wildcard**: Avoid in installation section, use explicit file listing
6. **License file encoding**: Must be UTF-16 LE BOM for Chinese characters to display correctly
7. **Language selection**: Use `MUI_LANGDLL_DISPLAY` only; do NOT add custom language page or override `$LANGUAGE` with system locale after `MUI_LANGDLL_DISPLAY`
8. **Uninstaller language**: Use `MUI_UNGETLANGUAGE` in `un.onInit` to match installer language

## Version Update Checklist

When changing version number, update these files:

1. `installer/setup.nsi` → `!define APP_VERSION`
2. `scripts/build-package.ps1` → `$Version` default
3. `scripts/verify-package.ps1` → `$Version` default
4. `CMakeLists.txt` → `project(EnhanceVision VERSION ...)`

## Output

| File | Expected Size |
|------|--------------|
| `package/EnhanceVision-v{ver}-windows-x64-installer.exe` | ~150 MB |
| `package/EnhanceVision-v{ver}-windows-x64-portable.zip` | ~180 MB |
