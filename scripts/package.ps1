param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [string]$OutputDir = "dist",
    [switch]$IncludeCompiler,
    [switch]$Zip,
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$forward = @{
    Config    = $Config
    OutputDir = $OutputDir
}
if ($IncludeCompiler) { $forward.IncludeCompiler = $true }
if ($Zip) { $forward.Zip = $true }
if ($Version) { $forward.Version = $Version }

& (Join-Path $Root "packaging/package.ps1") @forward
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
