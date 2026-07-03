# Distribution Script for RayWaves Game Engine
# This script creates a distribution package with the engine executable and development environment

param(
    [string]$BuildConfig = "Release",
    [string]$OutputDir = "dist",
    [switch]$IncludeCompiler = $false
)

$ErrorActionPreference = "Stop"

Write-Host "Creating distribution package..." -ForegroundColor Green
Write-Host "Build Config: $BuildConfig, Output Directory: $OutputDir" -ForegroundColor Cyan

# Note: VS environment setup is handled by the calling batch script

# Ensure we have a release build
$BuildPath = "build/zig-$($BuildConfig.ToLower())"
if (-not (Test-Path $BuildPath)) {
    Write-Host "Configuring $BuildConfig version..." -ForegroundColor Yellow
    if ($BuildConfig -ieq "Release") {
        cmake --preset zig-release
    } else {
        cmake --preset zig-debug
    }
}

Write-Host "Building targets (game runtime and editor)..." -ForegroundColor Yellow
cmake --build $BuildPath --config $BuildConfig --target game main
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

# Verify expected outputs exist before packaging
$GameExe = Join-Path $BuildPath "game.exe"
$EditorExe = Join-Path $BuildPath "main.exe"
$RaylibDll = Join-Path $BuildPath "libraylib.dll"

if (-not (Test-Path $GameExe)) { throw "Missing game.exe at $GameExe" }
if (-not (Test-Path $RaylibDll)) { throw "Missing libraylib.dll at $RaylibDll" }

# Create distribution directory structure
$DistPath = $OutputDir
if (Test-Path $DistPath) {
    Write-Host "Cleaning old distribution..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $DistPath -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Path $DistPath -Force | Out-Null
New-Item -ItemType Directory -Path "$DistPath/GameLogic" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistPath/Assets" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistPath/Core" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistPath/Core/Engine" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistPath/Documentation" -Force | Out-Null

# Create raylib directory structure
New-Item -ItemType Directory -Path "$DistPath/Core/raylib/include" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistPath/Core/raylib/lib" -Force | Out-Null
New-Item -ItemType Directory -Path "$DistPath/Core/raylib/bin" -Force | Out-Null

Write-Host "Copying executable and dependencies..." -ForegroundColor Yellow

# Try to stop running instances from the dist folder to avoid file lock errors
Stop-Process -Name "game" -ErrorAction SilentlyContinue
Stop-Process -Name "editor" -ErrorAction SilentlyContinue

# Copy game runtime as hidden engine base for exports
Copy-Item "$BuildPath/game.exe" "$DistPath/Core/runtime.exe" -Force

# Optionally include the editor (rename to editor.exe)
if (Test-Path "$BuildPath/main.exe") {
    Copy-Item "$BuildPath/main.exe" "$DistPath/editor.exe" -Force
}



# Copy static Engine library to root (needed for linking GameLogic with Zig)
Copy-Item "$BuildPath/libEngine.a" "$DistPath/Core/" -Force

# Copy raylib files to raylib folder structure
Copy-Item "$BuildPath/libraylib.dll" "$DistPath/Core/raylib/bin/" -Force
Copy-Item "$BuildPath/_deps/raylib-build/raylib/libraylib.dll.a" "$DistPath/Core/raylib/lib/libraylib.a" -Force
Copy-Item "$BuildPath/_deps/raylib-build/raylib/include/*.h" "$DistPath/Core/raylib/include/" -Force

# IMPORTANT: Copy libraylib.dll to dist root so game.exe and editor.exe can find it at runtime
Copy-Item "$BuildPath/libraylib.dll" "$DistPath/" -Force

# Copy Engine UI Assets
New-Item -ItemType Directory -Path "$DistPath/Core/EngineContent" -Force | Out-Null
Copy-Item "Assets/EngineContent/*" "$DistPath/Core/EngineContent/" -Recurse -Force

Write-Host "Creating development environment..." -ForegroundColor Yellow

# Copy Engine headers and source files (needed for GameLogic development)
Copy-Item "Engine/*.h" "$DistPath/Core/Engine/" -Force
Copy-Item "Engine/*.cpp" "$DistPath/Core/Engine/" -Force

# Copy the distribution CMakeLists.txt
Copy-Item "Distribution/dist_CMakeLists.txt" "$DistPath/Core/CMakeLists.txt" -Force

# Copy distribution documentation
Copy-Item "Documentation/README_DISTRIBUTION.md" "$DistPath/Documentation/" -Force
Copy-Item "Documentation/DISTRIBUTION_GUIDE.md" "$DistPath/Documentation/" -Force

# Copy Project Templates
if (Test-Path "Distribution/Templates") {
    Write-Host "Copying Project Templates..." -ForegroundColor Yellow
    Copy-Item "Distribution/Templates" "$DistPath/" -Recurse -Force
    # Strip local build artifacts and .raywaves folders
    $ExcludedItems = Get-ChildItem -Path "$DistPath/Templates" -Recurse -Include *.dll,*.pdb,*.lib,*.obj,.raywaves -Force
    if ($ExcludedItems) {
        Write-Host "Warning: Found and removed local build artifacts/caches from templates during packaging:" -ForegroundColor Yellow
        foreach ($item in $ExcludedItems) {
            Write-Host "  Removing: $($item.FullName)" -ForegroundColor Yellow
            Remove-Item -Path $item.FullName -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}



if ($IncludeCompiler) {
    Write-Host "Bundling Zig Compiler (Zero Install)..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "$DistPath/Core/Tools/zig" -Force | Out-Null
    Copy-Item "Tools/zig/*" "$DistPath/Core/Tools/zig/" -Recurse -Force
}

# Always copy the compiler wrappers
New-Item -ItemType Directory -Path "$DistPath/Core/Tools" -Force | Out-Null
Copy-Item "Tools/zig-c*.bat" "$DistPath/Core/Tools/" -Force

# Copy default game configuration
Copy-Item "Distribution/config.ini" "$DistPath/" -Force

Write-Host "Creating build configuration..." -ForegroundColor Yellow

Write-Host "Distribution created successfully in '$DistPath'" -ForegroundColor Green
Write-Host ""
Write-Host "Distribution contents:" -ForegroundColor Cyan
if (Test-Path "$DistPath/editor.exe") {
    Write-Host "- editor.exe (RayWaves editor/IDE)" -ForegroundColor White
}
Write-Host "- libraylib.dll (required at runtime)" -ForegroundColor White
Write-Host "- config.ini (window and game settings)" -ForegroundColor White
Write-Host "- Templates/ (project templates)" -ForegroundColor White
Write-Host "- Documentation/ (user guides and documentation)" -ForegroundColor White
Write-Host "- Core/ (engine internals)" -ForegroundColor White
Write-Host "  - raylib/ (raylib development files)" -ForegroundColor White
Write-Host "  - CMakeLists.txt (for building GameLogic)" -ForegroundColor White
Write-Host "  - Engine/ (engine headers)" -ForegroundColor White
if ($IncludeCompiler) {
    Write-Host "  - Tools/zig/ (bundled Zig compiler for zero-install hot-reloading)" -ForegroundColor White
}
