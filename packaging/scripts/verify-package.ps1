param(
    [string]$Version = "0.1.0"
)

$packageDir = "packaging\output\EnhanceVision-v$Version-windows-x64"
$buildDir = "build\msvc2022\Release\Release"
$errors = @()
$warnings = @()

if (-not (Test-Path $packageDir)) {
    Write-Host "[ERROR] Package directory not found: $packageDir" -ForegroundColor Red
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
    "qt.conf",
    "LICENSE",
    "THIRD_PARTY_LICENSES.md",
    "README.txt"
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path "$packageDir\$file")) {
        $errors += "Missing file: $file"
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
        $errors += "Missing DLL: $dll"
    }
}

if (-not (Test-Path "$packageDir\models")) {
    $errors += "Missing models directory"
} else {
    $modelFiles = Get-ChildItem -Path "$packageDir\models" -Recurse -Filter "*.param"
    if ($modelFiles.Count -eq 0) {
        $errors += "No AI model files in models directory"
    }
}

if (-not (Test-Path "$packageDir\translations")) {
    $warnings += "Missing translations directory"
}

$requiredDirs = @("platforms", "qml", "styles", "imageformats")
foreach ($dir in $requiredDirs) {
    if (-not (Test-Path "$packageDir\$dir")) {
        $errors += "Missing directory: $dir"
    }
}

if (-not (Test-Path $buildDir)) {
    $warnings += "Build directory not found, skipping consistency check"
} else {
    $expectedEntries = Get-ChildItem -Path $buildDir | Where-Object { $excludedRuntimeEntries -notcontains $_.Name } | Select-Object -ExpandProperty Name
    $packagedEntries = Get-ChildItem -Path $packageDir | Select-Object -ExpandProperty Name

    foreach ($name in $expectedEntries) {
        if (-not (Test-Path (Join-Path $packageDir $name))) {
            $errors += "Missing runtime entry in package: $name"
        }
    }

    foreach ($name in $packagedEntries) {
        if (($excludedRuntimeEntries -contains $name) -or (-not ($expectedEntries -contains $name) -and $requiredFiles -notcontains $name)) {
            $warnings += "Unaligned package entry: $name"
        }
    }
}

$totalSize = (Get-ChildItem -Path $packageDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host "`nPackage size: $([math]::Round($totalSize, 2)) MB" -ForegroundColor Cyan

Write-Host "`n--- Verification Results ---" -ForegroundColor Cyan

if ($warnings.Count -gt 0) {
    Write-Host "`nWarnings:" -ForegroundColor Yellow
    $warnings | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
}

if ($errors.Count -eq 0) {
    Write-Host "`nPackage verification passed" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`nPackage verification failed:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
