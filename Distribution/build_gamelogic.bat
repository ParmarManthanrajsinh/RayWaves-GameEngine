@echo off
REM Quick build script for RayWaves GameLogic.dll
REM This script helps users rebuild their game logic for hot-reloading
REM IMPORTANT: Must use MSVC to match the editor executable's ABI

echo Building GameLogic.dll...

REM Check if build directory exists
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

REM Always (re)configure CMake with Visual Studio generator to ensure MSVC ABI
echo Configuring CMake (Visual Studio / MSVC)...
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    echo Make sure Visual Studio 2022 is installed.
    if not "%1"=="nopause" pause
    exit /b 1
)

REM Build GameLogic in Release mode
echo Building GameLogic target (Release)...
cmake --build . --target GameLogic --config Release
if errorlevel 1 (
    echo ERROR: Build failed!
    if not "%1"=="nopause" pause
    exit /b 1
)

cd ..
echo.
echo GameLogic.dll built successfully!
echo The engine will automatically detect and reload the changes.
echo.
if "%1"=="nopause" goto :eof
pause