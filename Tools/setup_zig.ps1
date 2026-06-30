# setup_zig.ps1
$ErrorActionPreference = "Stop"

$ToolsDir = Join-Path $PSScriptRoot "Tools"
if (-not (Test-Path $ToolsDir)) {
    New-Item -ItemType Directory -Path $ToolsDir | Out-Null
}

Write-Host "Downloading Zig 0.13.0..." -ForegroundColor Cyan
$ZigZip = Join-Path $ToolsDir "zig.zip"
$ZigUrl = "https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip"
curl.exe -L -o "$ZigZip" "$ZigUrl"

Write-Host "Extracting Zig..." -ForegroundColor Cyan
tar.exe -xf "$ZigZip" -C "$ToolsDir"
Remove-Item $ZigZip

# Rename the extracted folder to just 'zig'
$ExtractedDir = Join-Path $ToolsDir "zig-windows-x86_64-0.13.0"
$TargetDir = Join-Path $ToolsDir "zig"
if (Test-Path $TargetDir) {
    Remove-Item -Recurse -Force $TargetDir
}
Rename-Item -Path $ExtractedDir -NewName "zig"

Write-Host "Downloading rcedit..." -ForegroundColor Cyan
$RceditUrl = "https://github.com/electron/rcedit/releases/download/v2.0.0/rcedit-x64.exe"
$RceditExe = Join-Path $ToolsDir "rcedit.exe"
curl.exe -L -o "$RceditExe" "$RceditUrl"

Write-Host "Setup Complete! Tools are ready." -ForegroundColor Green
