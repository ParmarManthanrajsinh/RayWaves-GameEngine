param(
    [string]$Preset = "zig-release",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$BenchArgs
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildPath = Join-Path $Root "build/$Preset"

Push-Location $Root
try {
    if (-not (Test-Path $BuildPath)) {
        cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed" }
    }

    cmake --build $BuildPath --target raywaves_benchmarks
    if ($LASTEXITCODE -ne 0) { throw "Benchmark build failed" }

    & (Join-Path $BuildPath "raywaves_benchmarks.exe") @BenchArgs
    if ($LASTEXITCODE -ne 0) { throw "Benchmarks failed" }
}
finally {
    Pop-Location
}
