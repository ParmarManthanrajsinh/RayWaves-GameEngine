param(
    [string]$Preset = "zig-release",
    [string]$Target = ""
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

Push-Location $Root
try {
    if (-not (Test-Path "build/$Preset")) {
        cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed" }
    }

    if ($Target) {
        cmake --build "build/$Preset" --target $Target
    }
    else {
        cmake --build "build/$Preset"
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}
finally {
    Pop-Location
}
