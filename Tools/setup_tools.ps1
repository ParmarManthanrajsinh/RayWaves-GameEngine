# setup_tools.ps1 - fetch the pinned build toolchain on Windows.
#
# Mirrors Tools/setup_tools.sh. Tool versions live in Tools/toolchain.ini so
# Windows and Linux always download the same releases.
param(
    [switch]$SkipZig = $false,
    [switch]$SkipRcEdit = $false,
    [switch]$SkipNinja = $false,
    [switch]$SkipCMake = $false
)

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VersionsFile = Join-Path $ToolsDir "toolchain.ini"

if (-not (Test-Path $VersionsFile)) {
    throw "Missing $VersionsFile"
}

$Versions = @{}
Get-Content $VersionsFile | ForEach-Object {
    $line = $_.Trim()
    if ($line -and -not $line.StartsWith("#") -and $line.Contains("=")) {
        $parts = $line.Split("=", 2)
        $Versions[$parts[0].Trim()] = $parts[1].Trim()
    }
}

foreach ($key in @("zig_version", "cmake_version", "ninja_version", "rcedit_version")) {
    if (-not $Versions.ContainsKey($key)) { throw "'$key' missing from $VersionsFile" }
}

$ZigVersion = $Versions["zig_version"]
$CMakeVersion = $Versions["cmake_version"]
$NinjaVersion = $Versions["ninja_version"]
$RcEditVersion = $Versions["rcedit_version"]

function Download-File([string]$Url, [string]$Destination) {
    Write-Host "Downloading $(Split-Path -Leaf $Destination)..." -ForegroundColor Cyan
    & curl.exe -fL --retry 3 --retry-delay 1 -o $Destination $Url
    if ($LASTEXITCODE -ne 0) { throw "Failed to download $Url" }
}

function Rename-Directory([string]$From, [string]$To) {
    if (-not (Test-Path $From)) { return }
    if (Test-Path $To) { Remove-Item -Recurse -Force $To }
    $leaf = Split-Path -Leaf $To
    $retry = 0
    while ($true) {
        try {
            Rename-Item -Path $From -NewName $leaf -ErrorAction Stop
            break
        }
        catch {
            if ($retry -ge 10) { throw }
            $retry++
            Start-Sleep -Milliseconds 500
        }
    }
}

if (-not $SkipZig) {
    $ZigTargetDir = Join-Path $ToolsDir "zig"
    if (-not (Test-Path (Join-Path $ZigTargetDir "zig.exe"))) {
        if (Get-Command zig -ErrorAction SilentlyContinue) {
            Write-Host "Using Zig from PATH." -ForegroundColor Green
        }
        else {
            $ZigZip = Join-Path $ToolsDir "zig.zip"
            $ZigUrl = "https://ziglang.org/download/$ZigVersion/zig-x86_64-windows-$ZigVersion.zip"
            Download-File $ZigUrl $ZigZip

            Write-Host "Extracting Zig..." -ForegroundColor Cyan
            & tar.exe -xf $ZigZip -C $ToolsDir
            if ($LASTEXITCODE -ne 0) { throw "Failed to extract Zig" }
            Remove-Item $ZigZip

            Rename-Directory (Join-Path $ToolsDir "zig-x86_64-windows-$ZigVersion") $ZigTargetDir
        }
    }
    else {
        Write-Host "Zig is already downloaded." -ForegroundColor Green
    }
}

if (-not $SkipRcEdit) {
    $RceditExe = Join-Path $ToolsDir "rcedit.exe"
    if (-not (Test-Path $RceditExe)) {
        $RceditUrl = "https://github.com/electron/rcedit/releases/download/v$RcEditVersion/rcedit-x64.exe"
        Download-File $RceditUrl $RceditExe
    }
    else {
        Write-Host "rcedit is already downloaded." -ForegroundColor Green
    }
}

if (-not $SkipNinja) {
    $NinjaTarget = Join-Path $ToolsDir "ninja\ninja.exe"
    if (-not (Test-Path $NinjaTarget)) {
        $NinjaZip = Join-Path $ToolsDir "ninja.zip"
        $NinjaUrl = "https://github.com/ninja-build/ninja/releases/download/v$NinjaVersion/ninja-win.zip"
        Download-File $NinjaUrl $NinjaZip

        New-Item -ItemType Directory -Force -Path (Join-Path $ToolsDir "ninja") | Out-Null
        Write-Host "Extracting Ninja..." -ForegroundColor Cyan
        & tar.exe -xf $NinjaZip -C (Join-Path $ToolsDir "ninja")
        if ($LASTEXITCODE -ne 0) { throw "Failed to extract Ninja" }
        Remove-Item $NinjaZip
    }
    else {
        Write-Host "Ninja is already downloaded." -ForegroundColor Green
    }
}

if (-not $SkipCMake) {
    $CMakeTargetDir = Join-Path $ToolsDir "cmake"
    if (-not (Test-Path (Join-Path $CMakeTargetDir "bin\cmake.exe"))) {
        $CMakeZip = Join-Path $ToolsDir "cmake.zip"
        $CMakeUrl = "https://github.com/Kitware/CMake/releases/download/v$CMakeVersion/cmake-$CMakeVersion-windows-x86_64.zip"
        Download-File $CMakeUrl $CMakeZip

        Write-Host "Extracting CMake..." -ForegroundColor Cyan
        & tar.exe -xf $CMakeZip -C $ToolsDir
        if ($LASTEXITCODE -ne 0) { throw "Failed to extract CMake" }
        Remove-Item $CMakeZip

        Rename-Directory (Join-Path $ToolsDir "cmake-$CMakeVersion-windows-x86_64") $CMakeTargetDir
    }
    else {
        Write-Host "CMake is already downloaded." -ForegroundColor Green
    }
}

Write-Host "Setup complete. Tools are ready." -ForegroundColor Green
