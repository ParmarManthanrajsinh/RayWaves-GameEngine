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
$BuildPath = Join-Path $Root "build/zig-$($Config.ToLower())"
$DistPath = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $Root $OutputDir }

if (-not $Version) {
    # Single source of truth: project(RayWaves VERSION x.y.z) in CMakeLists.txt.
    $CMakeLists = Join-Path $Root "CMakeLists.txt"
    $versionMatch = Select-String -Path $CMakeLists `
        -Pattern 'project\s*\(\s*RayWaves\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' |
        Select-Object -First 1
    $Version = if ($versionMatch) { $versionMatch.Matches[0].Groups[1].Value } else { "0.1.0" }
}

Write-Host "Packaging RayWaves $Version ($Config)" -ForegroundColor Cyan
Write-Host "  build:  $BuildPath"
Write-Host "  output: $DistPath"

function Require-Path([string]$Path, [string]$Label) {
    if (-not (Test-Path $Path)) {
        throw "Missing $Label at $Path"
    }
}

function Copy-Path([string]$From, [string]$To) {
    Require-Path $From $From
    if (Test-Path $From -PathType Container) {
        New-Item -ItemType Directory -Force -Path $To | Out-Null
        Copy-Item (Join-Path $From "*") $To -Recurse -Force
    }
    else {
        $parent = Split-Path -Parent $To
        if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        Copy-Item $From $To -Force
    }
}

# Configure and build the two host executables.
if (-not (Test-Path $BuildPath)) {
    Write-Host "Configuring preset zig-$($Config.ToLower())..." -ForegroundColor Yellow
    $presetArgs = @("--preset", "zig-$($Config.ToLower())")
    if ($Config -ieq "Release") { $presetArgs += "-DRAYWAVES_DISTRIBUTION_BUILD=ON" }
    & cmake @presetArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
}

Write-Host "Building runtime and editor..." -ForegroundColor Yellow
& cmake --build $BuildPath --config $Config --target game main
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$GameExe = Join-Path $BuildPath "game.exe"
$EditorExe = Join-Path $BuildPath "RayWaves.exe"
$RaylibDll = Join-Path $BuildPath "libraylib.dll"
$RaylibImport = Join-Path $BuildPath "raylib/lib/libraylib.a"
if (-not (Test-Path $RaylibImport)) {
    $RaylibImport = Join-Path $BuildPath "_deps/raylib-build/raylib/libraylib.dll.a"
}
$RaylibInclude = Join-Path $BuildPath "raylib/include"
if (-not (Test-Path $RaylibInclude)) {
    $RaylibInclude = Join-Path $BuildPath "_deps/raylib-build/raylib/include"
}

Require-Path $GameExe "runtime executable"
Require-Path $EditorExe "editor executable"
Require-Path $RaylibDll "raylib runtime DLL"
Require-Path $RaylibImport "raylib import library"
Require-Path $RaylibInclude "raylib headers"

# Stop running copies that would lock output files.
Get-Process -Name "game", "RayWaves" -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and ($_.Path.StartsWith($BuildPath) -or $_.Path.StartsWith($DistPath)) } |
    Stop-Process -Force -ErrorAction SilentlyContinue

if (Test-Path $DistPath) {
    Write-Host "Cleaning old distribution..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $DistPath
}

$CorePath = Join-Path $DistPath "Core"
New-Item -ItemType Directory -Force -Path $CorePath | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $CorePath "raylib/include") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $CorePath "raylib/lib") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $CorePath "raylib/bin") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $DistPath "docs") | Out-Null

Write-Host "Copying binaries..." -ForegroundColor Yellow
Copy-Item $GameExe (Join-Path $CorePath "runtime.exe") -Force
Copy-Item $EditorExe (Join-Path $DistPath "RayWaves.exe") -Force
Copy-Item $RaylibDll (Join-Path $DistPath "libraylib.dll") -Force
Copy-Item $RaylibDll (Join-Path $CorePath "raylib/bin/libraylib.dll") -Force
Copy-Item $RaylibImport (Join-Path $CorePath "raylib/lib/libraylib.a") -Force
Copy-Item (Join-Path $RaylibInclude "*.h") (Join-Path $CorePath "raylib/include") -Force

Write-Host "Copying engine sources and project template..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path (Join-Path $CorePath "Engine") | Out-Null
Copy-Item (Join-Path $Root "Engine/*.h") (Join-Path $CorePath "Engine") -Force
Copy-Item (Join-Path $Root "Engine/*.cpp") (Join-Path $CorePath "Engine") -Force
Copy-Item (Join-Path $Root "packaging/Project.CMakeLists.in") (Join-Path $CorePath "Project.CMakeLists.in") -Force

Copy-Path (Join-Path $Root "templates") (Join-Path $DistPath "templates")
Copy-Path (Join-Path $Root "EngineContent") (Join-Path $CorePath "EngineContent")
Copy-Path (Join-Path $Root "docs") (Join-Path $DistPath "docs")
Copy-Item (Join-Path $Root "packaging/config.ini") (Join-Path $DistPath "config.ini") -Force
if (Test-Path (Join-Path $Root "LICENSE")) {
    Copy-Item (Join-Path $Root "LICENSE") (Join-Path $DistPath "LICENSE") -Force
}
Set-Content -Path (Join-Path $DistPath "VERSION") -Value $Version -Encoding ascii

Write-Host "Copying toolchain..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path (Join-Path $CorePath "Tools") | Out-Null
Copy-Item (Join-Path $Root "Tools/zig-c*.bat") (Join-Path $CorePath "Tools") -Force
Copy-Item (Join-Path $Root "Tools/setup_zig.ps1") (Join-Path $CorePath "Tools") -Force
if (Test-Path (Join-Path $Root "Tools/rcedit.exe")) {
    Copy-Item (Join-Path $Root "Tools/rcedit.exe") (Join-Path $CorePath "Tools") -Force
}
if (Test-Path (Join-Path $Root "Tools/ninja/ninja.exe")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $CorePath "Tools/ninja") | Out-Null
    Copy-Item (Join-Path $Root "Tools/ninja/ninja.exe") (Join-Path $CorePath "Tools/ninja") -Force
}
if (Test-Path (Join-Path $Root "Tools/cmake/bin/cmake.exe")) {
    Copy-Path (Join-Path $Root "Tools/cmake") (Join-Path $CorePath "Tools/cmake")
}
if ($IncludeCompiler -and (Test-Path (Join-Path $Root "Tools/zig"))) {
    Copy-Path (Join-Path $Root "Tools/zig") (Join-Path $CorePath "Tools/zig")
}

# Remove build artifacts that may have been copied from templates.
Get-ChildItem (Join-Path $DistPath "templates") -Recurse -Include *.dll, *.pdb, *.lib, *.obj -Force -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem (Join-Path $DistPath "templates") -Recurse -Directory -Filter .raywaves -Force -ErrorAction SilentlyContinue |
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Validating package..." -ForegroundColor Yellow
$required = @(
    "RayWaves.exe",
    "libraylib.dll",
    "config.ini",
    "VERSION",
    "Core/runtime.exe",
    "Core/Project.CMakeLists.in",
    "Core/raylib/include/raylib.h",
    "Core/raylib/lib/libraylib.a",
    "docs/README.md"
)
foreach ($relative in $required) {
    $full = Join-Path $DistPath $relative
    if (-not (Test-Path $full)) { throw "Package validation failed: missing $relative" }
}

$archivePath = $null
if ($Zip) {
    $archivePath = Join-Path $Root "dist/RayWaves-$Version-win64.zip"
    if (Test-Path $archivePath) { Remove-Item $archivePath -Force }
    Compress-Archive -Path (Join-Path $DistPath "*") -DestinationPath $archivePath -Force
    $hash = (Get-FileHash $archivePath -Algorithm SHA256).Hash.ToLower()
    Set-Content -Path "$archivePath.sha256" -Value "$hash  $(Split-Path -Leaf $archivePath)" -Encoding ascii
    Write-Host "Archive: $archivePath" -ForegroundColor Green
    Write-Host "SHA256:  $hash" -ForegroundColor Green
}

Write-Host "Distribution ready: $DistPath" -ForegroundColor Green
if ($archivePath) { Write-Host "Zip: $archivePath" -ForegroundColor Green }
