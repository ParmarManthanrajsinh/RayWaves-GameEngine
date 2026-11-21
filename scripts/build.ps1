param(
    [string]$Preset = "zig-release",
    [string]$Target = ""
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

Push-Location $Root
try {
    $cache = "build/$Preset/CMakeCache.txt"
    if (Test-Path $cache) {
        $compiler = Select-String -Path $cache -Pattern '^CMAKE_CXX_COMPILER:STRING=(.*)$' |
            Select-Object -First 1
        if ($compiler -and $compiler.Matches[0].Groups[1].Value -match '/usr/bin/g\+\+|zig-cxx\.sh') {
            throw "build/$Preset was configured on Linux. Run scripts/clean.ps1 -Preset $Preset."
        }
    }

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
