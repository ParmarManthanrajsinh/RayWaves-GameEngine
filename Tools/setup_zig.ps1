# setup_zig.ps1
param(
    [switch]$SkipZig = $false,
    [switch]$SkipRcEdit = $false
)
$ErrorActionPreference = "Stop"

$ToolsDir = $PSScriptRoot

if (-not $SkipZig) {
    $ZigTargetDir = Join-Path $ToolsDir "zig"
    if (-not (Test-Path (Join-Path $ZigTargetDir "zig.exe"))) {
        Write-Host "Downloading Zig 0.13.0..." -ForegroundColor Cyan
        $ZigZip = Join-Path $ToolsDir "zig.zip"
        $ZigUrl = "https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip"
        curl.exe -L -o "$ZigZip" "$ZigUrl"

        Write-Host "Extracting Zig..." -ForegroundColor Cyan
        tar.exe -xf "$ZigZip" -C "$ToolsDir"
        Remove-Item $ZigZip

        # Rename the extracted folder to just 'zig'
        $ExtractedDir = Join-Path $ToolsDir "zig-windows-x86_64-0.13.0"
        if (Test-Path $ZigTargetDir) {
            Remove-Item -Recurse -Force $ZigTargetDir
        }
        Rename-Item -Path $ExtractedDir -NewName "zig"
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

Write-Host "Setup Complete! Tools are ready." -ForegroundColor Green
