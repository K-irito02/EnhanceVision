param(
    [string]$Version = "0.1.0"
)

$packageDir = "package\EnhanceVision-v$Version-windows-x64"
$errors = @()
$warnings = @()

if (-not (Test-Path $packageDir)) {
    Write-Host "❌ 打包目录不存在: $packageDir" -ForegroundColor Red
    exit 1
}

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

$requiredDlls = @(
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Quick.dll",
    "Qt6Qml.dll",
    "Qt6QuickControls2.dll",
    "avcodec-62.dll",
    "avformat-62.dll",
    "avutil-60.dll",
    "swscale-9.dll",
    "D3Dcompiler_47.dll"
)

foreach ($dll in $requiredDlls) {
    if (-not (Test-Path "$packageDir\$dll")) {
        $errors += "缺少 DLL: $dll"
    }
}

if (-not (Test-Path "$packageDir\models")) {
    $errors += "缺少 models 目录"
} else {
    $modelFiles = Get-ChildItem -Path "$packageDir\models" -Recurse -Filter "*.param"
    if ($modelFiles.Count -eq 0) {
        $errors += "models 目录中没有 AI 模型文件"
    }
}

if (-not (Test-Path "$packageDir\translations")) {
    $warnings += "缺少 translations 目录"
}

$requiredDirs = @("platforms", "qml", "styles", "imageformats")
foreach ($dir in $requiredDirs) {
    if (-not (Test-Path "$packageDir\$dir")) {
        $errors += "缺少目录: $dir"
    }
}

$excludedFiles = @(
    "EnhanceVision.exp",
    "EnhanceVision.lib",
    "EnhanceVisionMessageStatusResolverTest.exe",
    "EnhanceVisionSessionPruneUtilsTest.exe",
    "ffmpeg.exe",
    "ffplay.exe",
    "ffprobe.exe",
    "avcodec-61.dll",
    "avformat-61.dll",
    "avutil-59.dll",
    "swscale-8.dll",
    "swresample-5.dll"
)

foreach ($file in $excludedFiles) {
    if (Test-Path "$packageDir\$file") {
        $warnings += "不应包含的文件: $file"
    }
}

if (Test-Path "$packageDir\logs") {
    $warnings += "不应包含 logs 目录"
}

$totalSize = (Get-ChildItem -Path $packageDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host "`n📦 打包大小: $([math]::Round($totalSize, 2)) MB" -ForegroundColor Cyan

Write-Host "`n--- 验证结果 ---" -ForegroundColor Cyan

if ($warnings.Count -gt 0) {
    Write-Host "`n⚠️ 警告:" -ForegroundColor Yellow
    $warnings | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
}

if ($errors.Count -eq 0) {
    Write-Host "`n✅ 打包验证通过" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n❌ 打包验证失败:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
