param(
    [string]$Version = "0.1.0"
)

$packageDir = "package\EnhanceVision-v$Version-windows-x64"
$buildDir = "build\msvc2022\Release\Release"
$errors = @()
$warnings = @()

if (-not (Test-Path $packageDir)) {
    Write-Host "❌ 打包目录不存在: $packageDir" -ForegroundColor Red
    exit 1
}

$excludedRuntimeEntries = @(
    "EnhanceVision.exp",
    "EnhanceVision.lib",
    "EnhanceVisionMessageStatusResolverTest.exe",
    "EnhanceVisionSessionPruneUtilsTest.exe",
    "Qt6Test.dll",
    "logs"
)

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

if (-not (Test-Path $buildDir)) {
    $warnings += "构建目录不存在，跳过与 Release 运行目录的一致性对比"
} else {
    $expectedEntries = Get-ChildItem -Path $buildDir | Where-Object { $excludedRuntimeEntries -notcontains $_.Name } | Select-Object -ExpandProperty Name
    $packagedEntries = Get-ChildItem -Path $packageDir | Select-Object -ExpandProperty Name

    foreach ($name in $expectedEntries) {
        if (-not (Test-Path (Join-Path $packageDir $name))) {
            $errors += "打包缺少运行时条目: $name"
        }
    }

    foreach ($name in $packagedEntries) {
        if (($excludedRuntimeEntries -contains $name) -or (-not ($expectedEntries -contains $name) -and $requiredFiles -notcontains $name)) {
            $warnings += "存在未对齐的打包条目: $name"
        }
    }
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
