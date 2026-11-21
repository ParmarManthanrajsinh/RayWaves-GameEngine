# clean.ps1 - remove a CMake build directory.
param(
    [string]$Preset = "",
    [switch]$All
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ($All -or -not $Preset) {
    $path = Join-Path $Root "build"
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
    Write-Host "Removed build/"
    exit 0
}

$path = Join-Path $Root "build/$Preset"
if (Test-Path $path) { Remove-Item -Recurse -Force $path }
Write-Host "Removed build/$Preset"
