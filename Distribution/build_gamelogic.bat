@echo off
REM Quick build script for RayWaves GameLogic.dll
REM This script uses the bundled or system Zig compiler for a zero-install experience

echo Building GameLogic.dll...

set ZIG_COMPILER=

if exist "%~dp0Core\Tools\zig\zig.exe" (
    set ZIG_COMPILER="%~dp0Core\Tools\zig\zig.exe"
    echo Using bundled Zig compiler.
) else (
    where zig >nul 2>nul && (
        set ZIG_COMPILER=zig
        echo Using system Zig compiler.
    )
)

if "%ZIG_COMPILER%"=="" (
    echo ERROR: No Zig compiler found!
    echo Please install Zig system-wide or ask the developer to export the game with the '-IncludeCompiler' flag.
    if "%1" NEQ "nopause" pause
    exit /b 1
)

echo Compiling GameLogic with Zig...

setlocal EnableDelayedExpansion
set SRC_FILES=
for %%f in (GameLogic\*.cpp) do set SRC_FILES=!SRC_FILES! %%f
for %%f in (Core\Engine\*.cpp) do set SRC_FILES=!SRC_FILES! %%f

%ZIG_COMPILER% c++ -shared -o GameLogic.dll !SRC_FILES! -ICore\Engine -ICore\raylib\include -LCore\raylib\lib -lraylib -ldwmapi -std=c++23 -msse4.2 -O2
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