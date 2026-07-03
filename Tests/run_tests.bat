@echo off
setlocal
cd /D "%~dp0"

echo === Build tests ===
cmake --build ..\build\zig-release --target tests
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Unit Tests ===
..\build\zig-release\tests.exe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo === Smoke Test ===
ctest --test-dir ..\build\zig-release -R smoke
