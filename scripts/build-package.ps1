param(
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

Write-Host "=== EnhanceVision 打包脚本 ===" -ForegroundColor Cyan
Write-Host "版本: $Version" -ForegroundColor Cyan

# 1. 构建 Release 版本
Write-Host "`n[1/6] 构建 Release 版本..." -ForegroundColor Yellow
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-msvc-2022-release
if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }

& "C:\Program Files\CMake\bin\cmake.exe" --build build/msvc2022/Release --config Release -j 8
if ($LASTEXITCODE -ne 0) { throw "构建失败" }

# 2. 准备打包目录
Write-Host "`n[2/6] 准备打包目录..." -ForegroundColor Yellow
$packageDir = "package\EnhanceVision-v$Version-windows-x64"
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

$windeployqt = "E:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "未找到 windeployqt: $windeployqt"
}

Write-Host "  同步 Qt 运行时依赖到打包目录..." -ForegroundColor DarkGray
& $windeployqt --release --qmldir (Resolve-Path "qml") (Join-Path (Resolve-Path $packageDir) "EnhanceVision.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt 运行失败" }

# 复制许可证文件
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
Write-Host "`n[3/6] 验证打包..." -ForegroundColor Yellow
& ".\scripts\verify-package.ps1" -Version $Version
if ($LASTEXITCODE -ne 0) { throw "打包验证失败" }

# 4. 创建安装程序
Write-Host "`n[4/6] 创建安装程序..." -ForegroundColor Yellow
& "E:\Program Files (x86)\NSIS\makensis.exe" /INPUTCHARSET UTF8 installer\setup.nsi
if ($LASTEXITCODE -ne 0) { throw "创建安装程序失败" }

# 5. 创建便携版
Write-Host "`n[5/6] 创建便携版..." -ForegroundColor Yellow
$zipFile = "package\EnhanceVision-v$Version-windows-x64-portable.zip"
Remove-Item -Force $zipFile -ErrorAction SilentlyContinue

$portableDir = "package\EnhanceVision-v$Version-windows-x64"
$startVbs = "portable\start.vbs"
Copy-Item -Path $startVbs -Destination $portableDir

Compress-Archive -Path $portableDir -DestinationPath $zipFile

Remove-Item "$portableDir\start.vbs" -ErrorAction SilentlyContinue

# 6. 计算校验和
Write-Host "`n[6/6] 计算校验和..." -ForegroundColor Yellow
$installerExe = "package\EnhanceVision-v$Version-windows-x64-installer.exe"
if (Test-Path $installerExe) {
    $installerHash = Get-FileHash $installerExe -Algorithm SHA256
    $installerSize = (Get-Item $installerExe).Length / 1MB
    Write-Host "  安装程序: $([math]::Round($installerSize, 2)) MB" -ForegroundColor Cyan
    Write-Host "  SHA256: $($installerHash.Hash)" -ForegroundColor Gray
}

$portableHash = Get-FileHash $zipFile -Algorithm SHA256
$portableSize = (Get-Item $zipFile).Length / 1MB
Write-Host "  便携版: $([math]::Round($portableSize, 2)) MB" -ForegroundColor Cyan
Write-Host "  SHA256: $($portableHash.Hash)" -ForegroundColor Gray

Write-Host "`n=== 打包完成 ===" -ForegroundColor Green
Write-Host "安装程序: $installerExe"
Write-Host "便携版: $zipFile"
