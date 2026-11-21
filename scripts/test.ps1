param(
    [ValidateSet("all", "unit", "integration", "smoke")]
    [string]$Suite = "all",
    [string]$Preset = "zig-debug"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildPath = Join-Path $Root "build/$Preset"

Push-Location $Root
try {
    $cache = Join-Path $BuildPath "CMakeCache.txt"
    if (Test-Path $cache) {
        $compiler = Select-String -Path $cache -Pattern '^CMAKE_CXX_COMPILER:STRING=(.*)$' |
            Select-Object -First 1
        if ($compiler -and $compiler.Matches[0].Groups[1].Value -match '/usr/bin/g\+\+|zig-cxx\.sh') {
            throw "$BuildPath was configured on Linux. Run scripts/clean.ps1 -Preset $Preset."
        }
    }

    if (-not (Test-Path $BuildPath)) {
        cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed" }
    }

    $targets = @("raywaves_tests_unit", "raywaves_tests_integration", "raywaves_smoke")
    cmake --build $BuildPath --target @targets
    if ($LASTEXITCODE -ne 0) { throw "Test build failed" }

    $label = if ($Suite -eq "all") { "unit|integration|smoke" } else { $Suite }
    ctest --test-dir $BuildPath -L $label --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
}
finally {
    Pop-Location
}
