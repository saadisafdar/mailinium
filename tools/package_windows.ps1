# =============================================================================
# package_windows.ps1 — Mailinium Windows Distribution Packaging Script
# =============================================================================
# Usage:
#   .\tools\package_windows.ps1 [-Config Release] [-SkipTests]
# =============================================================================

param (
    [string]$Config = "Release",
    [string]$OutputDir = "dist",
    [switch]$SkipTests = $false
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path "$ScriptDir\.."
Set-Location $ProjectRoot

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  Mailinium Windows Packaging & Distribution Tool ($Config)" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

function Find-ToolExecutable {
    param ([string]$ToolName)
    $cmd = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$ToolName.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$ToolName.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$ToolName.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$ToolName.exe",
        "C:\Program Files\CMake\bin\$ToolName.exe",
        "C:\Qt\Tools\CMake_64\bin\$ToolName.exe"
    )
    foreach ($cand in $candidates) {
        if (Test-Path $cand) {
            return $cand
        }
    }
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        if ($vsPath) {
            $cmakeInVs = "$vsPath\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$ToolName.exe"
            if (Test-Path $cmakeInVs) {
                return $cmakeInVs
            }
        }
    }
    return $ToolName
}

$cmakeExe = Find-ToolExecutable "cmake"
$ctestExe = Find-ToolExecutable "ctest"

# 1. Build project
Write-Host "`n[1/5] Building Mailinium ($Config configuration)..." -ForegroundColor Yellow
$buildArgs = @("--build", "build", "--config", $Config)
Write-Host "Running: $cmakeExe $($buildArgs -join ' ')"
& $cmakeExe @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 2. Run Test Suite
if (-not $SkipTests) {
    Write-Host "`n[2/5] Running verification test suite ($Config configuration)..." -ForegroundColor Yellow
    $testArgs = @("--test-dir", "build", "-C", $Config, "--output-on-failure")
    Write-Host "Running: $ctestExe $($testArgs -join ' ')"
    & $ctestExe @testArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Tests failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
} else {
    Write-Host "`n[2/5] Skipping test suite as requested (-SkipTests)." -ForegroundColor DarkGray
}

# 3. Verify executable exists
$binDir = "$ProjectRoot\build\bin\$Config"
$exePath = "$binDir\Mailinium.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found at $exePath"
    exit 1
}

# 4. Assemble standalone package
$version = "0.1.0"
$packageBaseName = "Mailinium-v$version-windows-x64"
$distPath = "$ProjectRoot\$OutputDir"
$packageDir = "$distPath\$packageBaseName"

Write-Host "`n[3/5] Assembling standalone distribution in $packageDir..." -ForegroundColor Yellow

if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

# Copy main binary
Copy-Item $exePath $packageDir -Force

# Copy all runtime DLLs from build/bin/$Config
Get-ChildItem -Path $binDir -Filter "*.dll" | ForEach-Object {
    Copy-Item $_.FullName $packageDir -Force
}

# Copy plugin directories (platforms, sqldrivers, styles, tls, imageformats, iconengines, etc.)
Get-ChildItem -Path $binDir -Directory | ForEach-Object {
    $dirName = $_.Name
    # Exclude Visual Studio / build artifact directories
    if ($dirName -notin @(".system_generated", "CMakeFiles")) {
        Copy-Item -Recurse $_.FullName "$packageDir\$dirName" -Force
    }
}

# Copy documentation files
if (Test-Path "$ProjectRoot\README.md") {
    Copy-Item "$ProjectRoot\README.md" $packageDir -Force
}
if (Test-Path "$ProjectRoot\LICENSE") {
    Copy-Item "$ProjectRoot\LICENSE" $packageDir -Force
}

# 5. Verification check
Write-Host "`n[4/5] Verifying bundle integrity..." -ForegroundColor Yellow

$requiredItems = @(
    "Mailinium.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "Qt6Network.dll",
    "Qt6Sql.dll",
    "platforms\qwindows.dll",
    "sqldrivers\qsqlite.dll"
)

$missingItems = @()
foreach ($item in $requiredItems) {
    $fullPath = Join-Path $packageDir $item
    if (-not (Test-Path $fullPath)) {
        $missingItems += $item
    }
}

if ($missingItems.Count -gt 0) {
    Write-Error "Bundle integrity check FAILED! Missing files:`n$($missingItems -join "`n")"
    exit 1
} else {
    Write-Host "All required runtime dependencies and plugins verified!" -ForegroundColor Green
}

# 6. Create ZIP archive
$zipPath = "$distPath\$packageBaseName.zip"
Write-Host "`n[5/5] Creating ZIP archive: $zipPath..." -ForegroundColor Yellow

if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path "$packageDir\*" -DestinationPath $zipPath -CompressionLevel Optimal

$zipSize = (Get-Item $zipPath).Length / 1MB
$totalFiles = (Get-ChildItem -Recurse $packageDir -File).Count

Write-Host "`n======================================================================" -ForegroundColor Green
Write-Host "  Mailinium Standalone Package Created Successfully!" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green
Write-Host "  Location: $zipPath"
Write-Host "  Archive Size: $([math]::Round($zipSize, 2)) MB"
Write-Host "  Bundled Files: $totalFiles"
Write-Host "======================================================================`n"
