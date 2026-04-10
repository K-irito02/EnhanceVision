param(
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

Write-Host "=== EnhanceVision Packaging Script ===" -ForegroundColor Cyan
Write-Host "Version: $Version" -ForegroundColor Cyan

# 1. Build Release
Write-Host "`n[1/6] Building Release version..." -ForegroundColor Yellow
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }

& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# 2. Prepare package directory
Write-Host "`n[2/6] Preparing package directory..." -ForegroundColor Yellow
$packageDir = "packaging\output\EnhanceVision-v$Version-windows-x64"
Remove-Item -Recurse -Force $packageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

$buildDir = "build\msvc2022\Release\Release"
$excludedRuntimeEntries = @(
    "EnhanceVision.exp",
    "EnhanceVision.lib",
    "EnhanceVisionMessageStatusResolverTest.exe",
    "EnhanceVisionSessionPruneUtilsTest.exe",
    "Qt6Test.dll",
    "logs"
)

Get-ChildItem -Path $buildDir | ForEach-Object {
    if ($excludedRuntimeEntries -contains $_.Name) {
        return
    }

    Copy-Item -Path $_.FullName -Destination $packageDir -Recurse
}

# Copy qt.conf for Qt runtime path configuration
$qtConf = "packaging\qt.conf"
if (Test-Path $qtConf) {
    Copy-Item -Path $qtConf -Destination $packageDir
    Write-Host "  Copied qt.conf" -ForegroundColor DarkGray
}

$windeployqt = "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found: $windeployqt"
}

Write-Host "  Syncing Qt runtime dependencies..." -ForegroundColor DarkGray
& $windeployqt --release --qmldir (Resolve-Path "qml") (Join-Path (Resolve-Path $packageDir) "EnhanceVision.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# Copy license files
if (Test-Path "LICENSE") {
    Copy-Item -Path "LICENSE" -Destination $packageDir
    Write-Host "  Copied LICENSE" -ForegroundColor DarkGray
} else {
    Write-Host "  WARNING: LICENSE file not found" -ForegroundColor Yellow
}

if (Test-Path "THIRD_PARTY_LICENSES.md") {
    Copy-Item -Path "THIRD_PARTY_LICENSES.md" -Destination $packageDir
    Write-Host "  Copied THIRD_PARTY_LICENSES.md" -ForegroundColor DarkGray
} else {
    Write-Host "  WARNING: THIRD_PARTY_LICENSES.md file not found" -ForegroundColor Yellow
}

# Create README.txt
$readmeContent = @"
EnhanceVision v$Version

Desktop image and video enhancement tool based on Qt 6 + QML.

Quick Start:
1. Double-click EnhanceVision.exe to launch
2. Add image or video files
3. Select Shader mode or AI mode
4. Adjust parameters and export

For more information: https://github.com/K-irito02/EnhanceVision

License: MIT License
"@
$readmeContent | Out-File -FilePath "$packageDir\README.txt" -Encoding UTF8

# 3. Verify package
Write-Host "`n[3/6] Verifying package..." -ForegroundColor Yellow
& ".\packaging\scripts\verify-package.ps1" -Version $Version
if ($LASTEXITCODE -ne 0) { throw "Package verification failed" }

# 4. Create installer
Write-Host "`n[4/6] Creating installer..." -ForegroundColor Yellow
& "E:\Program Files (x86)\NSIS\makensis.exe" /INPUTCHARSET UTF8 packaging\installer\setup.nsi
if ($LASTEXITCODE -ne 0) { throw "Installer creation failed" }

# 5. Create portable version
Write-Host "`n[5/6] Creating portable version..." -ForegroundColor Yellow
$zipFile = "packaging\output\EnhanceVision-v$Version-windows-x64-portable.zip"
Remove-Item -Force $zipFile -ErrorAction SilentlyContinue

$portableDir = "packaging\output\EnhanceVision-v$Version-windows-x64"
$startVbs = "packaging\portable\start.vbs"
Copy-Item -Path $startVbs -Destination $portableDir

Compress-Archive -Path $portableDir -DestinationPath $zipFile

Remove-Item "$portableDir\start.vbs" -ErrorAction SilentlyContinue

# 6. Calculate checksums
Write-Host "`n[6/6] Calculating checksums..." -ForegroundColor Yellow
$installerExe = "packaging\output\EnhanceVision-v$Version-windows-x64-installer.exe"
if (Test-Path $installerExe) {
    $installerHash = Get-FileHash $installerExe -Algorithm SHA256
    $installerSize = (Get-Item $installerExe).Length / 1MB
    Write-Host "  Installer: $([math]::Round($installerSize, 2)) MB" -ForegroundColor Cyan
    Write-Host "  SHA256: $($installerHash.Hash)" -ForegroundColor Gray
}

$portableHash = Get-FileHash $zipFile -Algorithm SHA256
$portableSize = (Get-Item $zipFile).Length / 1MB
Write-Host "  Portable: $([math]::Round($portableSize, 2)) MB" -ForegroundColor Cyan
Write-Host "  SHA256: $($portableHash.Hash)" -ForegroundColor Gray

Write-Host "`n=== Packaging Complete ===" -ForegroundColor Green
Write-Host "Installer: $installerExe"
Write-Host "Portable: $zipFile"
