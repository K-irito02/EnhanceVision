---
name: "packaging-workflow"
description: "EnhanceVision packaging workflow. Invoke when user asks to package, build installer, create portable ZIP, or run packaging scripts."
---

# EnhanceVision Packaging Workflow

Automated packaging workflow for building NSIS installer and portable ZIP.

## Prerequisites

| Requirement | Path/Command | Required |
|-------------|-------------|----------|
| NSIS 3.11 | `E:\Program Files (x86)\NSIS\makensis.exe` | Yes |
| CMake | `C:\Program Files\CMake\bin\cmake.exe` | Yes |
| Qt 6.10.2 | `E:\Qt\6.10.2\msvc2022_64` | Yes |
| VC++ Redistributable | `packaging/installer/redist/vc_redist.x64.exe` | Optional (see below) |

### VC++ Redistributable (Optional)

The VC++ redistributable is optional. If the file is not present, the installer will skip VC++ runtime installation and assume the user already has it installed. This is handled by using `File /nonfatal` in the NSIS script.

## Key Files

| File | Purpose |
|------|---------|
| `packaging/installer/setup.nsi` | NSIS installer script (UTF-8 BOM encoding) |
| `packaging/installer/redist/vc_redist.x64.exe` | VC++ 2022 runtime |
| `packaging/installer/license/license_zh.txt` | Chinese license (UTF-16 LE BOM) |
| `packaging/installer/license/license_en.txt` | English license (UTF-16 LE BOM) |
| `packaging/scripts/build-package.ps1` | One-click packaging script |
| `packaging/scripts/verify-package.ps1` | Package verification script |
| `packaging/portable/start.vbs` | Portable launcher (VBS, no console window) |
| `resources/icons/app_icon.ico` | Application icon |

## Execution Flow

```
1. Build Release → 2. Copy release tree → 3. Run windeployqt → 4. Verify → 5. NSIS installer → 6. Portable ZIP → 7. Checksums
```

## Upgrade Validation Focus

When installer storage logic changes, validate these before declaring packaging complete:

1. Upgrade install can keep old data, migrate old data, or delete old data.
2. First launch after upgrade shows the correct effective data directory in Settings.
3. Cache statistics, sessions, persisted UI state, and recovery data all match the selected effective data directory.
4. Finish-page elevated launch still preserves drag-and-drop compatibility on Windows.

## Quick Start

```powershell
# Full packaging (build + verify + installer + portable)
.\packaging\scripts\build-package.ps1 -Version "0.1.0"

# Verify only
.\packaging\scripts\verify-package.ps1 -Version "0.1.0"

# NSIS compile only
& "E:\Program Files (x86)\NSIS\makensis.exe" /INPUTCHARSET UTF8 packaging\installer\setup.nsi
```

## Step-by-Step Manual Packaging

### Step 1: Build Release

```powershell
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release
& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8
```

### Step 2: Prepare Package Directory

```powershell
$Version = "0.1.0"
$packageDir = "packaging\output\EnhanceVision-v$Version-windows-x64"
$buildDir = "build\msvc2022\Release\Release"

Remove-Item -Recurse -Force $packageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

# Copy the release runtime tree first
Copy-Item -Path "$buildDir\*" -Destination $packageDir -Recurse

# Deploy Qt runtime dependencies into the package output
& "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" `
    --release `
    --qmldir "qml" `
    --dir $packageDir `
    "$packageDir\EnhanceVision.exe"

# Copy license files
Copy-Item -Path "LICENSE" -Destination $packageDir
Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir
```

### Step 3: Verify Package

```powershell
& ".\packaging\scripts\verify-package.ps1" -Version $Version
```

### Step 4: Create NSIS Installer

```powershell
& "E:\Program Files (x86)\NSIS\makensis.exe" /INPUTCHARSET UTF8 packaging\installer\setup.nsi
```

### Step 5: Create Portable ZIP

```powershell
Copy-Item -Path "packaging\portable\start.vbs" -Destination $packageDir
Compress-Archive -Path $packageDir -DestinationPath "packaging\output\EnhanceVision-v$Version-windows-x64-portable.zip"
Remove-Item "$packageDir\start.vbs" -ErrorAction SilentlyContinue
```

### Step 6: Calculate Checksums

```powershell
Get-FileHash "packaging\output\EnhanceVision-v$Version-windows-x64-installer.exe" -Algorithm SHA256
Get-FileHash "packaging\output\EnhanceVision-v$Version-windows-x64-portable.zip" -Algorithm SHA256
```

The packaging script should also emit `*.sha256` companion files for both artifacts so distribution checks can be automated.

## Files to Exclude from Package

| Pattern | Reason |
|---------|--------|
| `*.exp`, `*.lib` | Build artifacts |
| `*Test.exe` | Test executables |
| `logs/` directory | Runtime logs |

## NSIS Known Pitfalls

1. **LicenseLangString order**: Must come AFTER `!insertmacro MUI_LANGUAGE`
2. **License page path**: Use `$(LicenseFile)` not `license_$LANGUAGE.txt`
3. **MB_ICONWARNING**: Not valid in NSIS, use `MB_ICONEXCLAMATION`
4. **File encoding**: Must use UTF-8 BOM or `/INPUTCHARSET UTF8`
5. **File /r wildcard**: Avoid in installation section, use explicit file listing
6. **License file encoding**: Must be UTF-16 LE BOM for Chinese characters to display correctly
7. **Language selection**: Use `MUI_LANGDLL_DISPLAY` only; do NOT add custom language page or override `$LANGUAGE` with system locale after `MUI_LANGDLL_DISPLAY`
8. **Uninstaller language**: Use `MUI_UNGETLANGUAGE` in `un.onInit` to match installer language
9. **Optional files**: Use `File /nonfatal` for files that may not exist (e.g., VC++ redistributable). Combine with `${If} ${FileExists}` to conditionally execute.

## PowerShell Script Encoding

PowerShell scripts in this project use English-only output to avoid encoding issues. Chinese characters in PowerShell scripts can cause parsing errors on some systems. If you need to add localized output, consider using resource files or external localization mechanisms.

## Version Update Checklist

When changing version number, update these files:

1. `packaging/installer/setup.nsi` → `!define APP_VERSION`
2. `packaging/scripts/build-package.ps1` → `$Version` default
3. `packaging/scripts/verify-package.ps1` → `$Version` default
4. `CMakeLists.txt` → `project(EnhanceVision VERSION ...)`

## Output

| File | Expected Size |
|------|--------------|
| `packaging/output/EnhanceVision-v{ver}-windows-x64-installer.exe` | ~155 MB |
| `packaging/output/EnhanceVision-v{ver}-windows-x64-portable.zip` | ~200 MB |
