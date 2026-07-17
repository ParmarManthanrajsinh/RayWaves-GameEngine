# setup_zig.ps1
param(
    [switch]$SkipZig = $false,
    [switch]$SkipRcEdit = $false,
    [switch]$SkipNinja = $false,
    [switch]$SkipCMake = $false
)
$ErrorActionPreference = "Stop"

$ToolsDir = $PSScriptRoot

if (-not $SkipZig) {
    $ZigTargetDir = Join-Path $ToolsDir "zig"
    if (-not (Test-Path (Join-Path $ZigTargetDir "zig.exe"))) {
        Write-Host "Downloading Zig 0.16.0..." -ForegroundColor Cyan
        $ZigZip = Join-Path $ToolsDir "zig.zip"
        $ZigUrl = "https://ziglang.org/download/0.16.0/zig-x86_64-windows-0.16.0.zip"
        curl.exe -L -o "$ZigZip" "$ZigUrl"

        Write-Host "Extracting Zig..." -ForegroundColor Cyan
        tar.exe -xf "$ZigZip" -C "$ToolsDir"
        Remove-Item $ZigZip

        $ExtractedDir = Join-Path $ToolsDir "zig-x86_64-windows-0.16.0"
        if (Test-Path $ZigTargetDir) {
            Remove-Item -Recurse -Force $ZigTargetDir
        }
        
        $retryCount = 0
        while ($true) {
            try {
                Rename-Item -Path $ExtractedDir -NewName "zig" -ErrorAction Stop
                break
            } catch {
                if ($retryCount -ge 10) {
                    throw
                }
                $retryCount++
                Start-Sleep -Milliseconds 500
            }
        }
    } else {
        Write-Host "Zig is already downloaded." -ForegroundColor Green
    }
}

if (-not $SkipRcEdit) {
    $RceditExe = Join-Path $ToolsDir "rcedit.exe"
    if (-not (Test-Path $RceditExe)) {
        Write-Host "Downloading rcedit..." -ForegroundColor Cyan
        $RceditUrl = "https://github.com/electron/rcedit/releases/download/v2.0.0/rcedit-x64.exe"
        curl.exe -L -o "$RceditExe" "$RceditUrl"
    } else {
        Write-Host "rcedit is already downloaded." -ForegroundColor Green
    }
}

if (-not $SkipNinja) {
    $NinjaTarget = Join-Path $ToolsDir "ninja\ninja.exe"
    if (-not (Test-Path $NinjaTarget)) {
        Write-Host "Downloading Ninja 1.13.2..." -ForegroundColor Cyan
        $NinjaZip = Join-Path $ToolsDir "ninja.zip"
        $NinjaUrl = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip"
        curl.exe -L -o "$NinjaZip" "$NinjaUrl"

        New-Item -ItemType Directory -Force -Path (Join-Path $ToolsDir "ninja") | Out-Null
        Write-Host "Extracting Ninja..." -ForegroundColor Cyan
        tar.exe -xf "$NinjaZip" -C (Join-Path $ToolsDir "ninja")
        Remove-Item $NinjaZip
    } else {
        Write-Host "Ninja is already downloaded." -ForegroundColor Green
    }
}

if (-not $SkipCMake) {
    $CMakeTargetDir = Join-Path $ToolsDir "cmake"
    if (-not (Test-Path (Join-Path $CMakeTargetDir "bin\cmake.exe"))) {
        Write-Host "Downloading CMake 4.3.4..." -ForegroundColor Cyan
        $CMakeZip = Join-Path $ToolsDir "cmake.zip"
        $CMakeUrl = "https://github.com/Kitware/CMake/releases/download/v4.3.4/cmake-4.3.4-windows-x86_64.zip"
        curl.exe -L -o "$CMakeZip" "$CMakeUrl"

        Write-Host "Extracting CMake..." -ForegroundColor Cyan
        tar.exe -xf "$CMakeZip" -C "$ToolsDir"
        Remove-Item $CMakeZip

        $ExtractedDir = Join-Path $ToolsDir "cmake-4.3.4-windows-x86_64"
        if (Test-Path $CMakeTargetDir) {
            Remove-Item -Recurse -Force $CMakeTargetDir
        }
        $retry = 0
        do {
            Start-Sleep -Milliseconds 200
            Rename-Item -Path $ExtractedDir -NewName "cmake" -ErrorAction SilentlyContinue
            $retry++
        } while ((Test-Path $ExtractedDir) -and $retry -lt 10)
    } else {
        Write-Host "CMake is already downloaded." -ForegroundColor Green
    }
}

Write-Host "Setup Complete! Tools are ready." -ForegroundColor Green
