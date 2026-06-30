@echo off
REM Quick build script for RayWaves GameLogic.dll
REM This script uses the bundled Zig compiler for a zero-install experience

echo Building GameLogic.dll...

if not exist "Core\Tools\zig\zig.exe" (
    echo ERROR: Bundled Zig compiler not found at Core\Tools\zig\zig.exe
    if "%1" NEQ "nopause" pause
    exit /b 1
)

echo Compiling GameLogic with Zig...

setlocal EnableDelayedExpansion
set SRC_FILES=
for %%f in (GameLogic\*.cpp) do set SRC_FILES=!SRC_FILES! %%f

Core\Tools\zig\zig.exe c++ -shared -o GameLogic.dll !SRC_FILES! -ICore\Engine -ICore\raylib\include -LCore -LCore\raylib\lib -lEngine -lraylib -std=c++23 -msse4.2 -O2
endlocal

if errorlevel 1 (
    echo ERROR: Build failed!
    if "%1" NEQ "nopause" pause
    exit /b 1
)

echo.
echo GameLogic.dll built successfully!
echo The engine will automatically detect and reload the changes.
echo.
if "%1"=="nopause" goto :eof
pause