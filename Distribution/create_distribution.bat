@echo off
echo Creating RayWaves Game Engine Distribution Package...
echo.

if not exist "build\zig-release\main.exe" (
    echo [INFO] No release build found. Building first...
    cmake --preset zig-release
    if errorlevel 1 (
        echo ERROR: CMake configuration failed!
        if "%~2" NEQ "nopause" pause
        exit /b 1
    )
    
    cmake --build build\zig-release --target main game GameLogic
    if errorlevel 1 (
        echo ERROR: Build failed!
        if "%~2" NEQ "nopause" pause
        exit /b 1
    )
)

echo Creating distribution using PowerShell script...
powershell.exe -ExecutionPolicy Bypass -File ".\Distribution\distribute.ps1" -OutputDir "dist" -BuildConfig "Release"

if errorlevel 1 (
    echo ERROR: Distribution failed.
    if "%~2" NEQ "nopause" pause
    exit /b 1
)

echo Distribution created successfully!
if "%~2" NEQ "nopause" pause
