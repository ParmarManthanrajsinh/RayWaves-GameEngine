@echo off
setlocal
cd /D "%~dp0"

echo === Build ===
cmake --build ..\build\zig-release --target tests
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Unit Tests ===
..\build\zig-release\tests.exe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Smoke Test ===
ctest --test-dir ..\build\zig-release -R smoke
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Full Build ===
cmake --build ..\build\zig-release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Launch ===
start "" "..\build\zig-release\RayWaves.exe"
