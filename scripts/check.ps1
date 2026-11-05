param(
    [string]$Preset = "zig-debug"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

Push-Location $Root
try {
    $clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($clangFormat) {
        Write-Host "Checking formatting..." -ForegroundColor Cyan
        & (Join-Path $Root "Tools/run_analysis.bat") format-check --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "Formatting check failed" }
    }
    else {
        Write-Warning "clang-format not found; skipping format check"
    }

    & (Join-Path $Root "scripts/check-docs.ps1") -Root $Root
    if ($LASTEXITCODE -ne 0) { throw "Documentation check failed" }

    & (Join-Path $Root "scripts/test.ps1") -Preset $Preset -Suite all
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}
