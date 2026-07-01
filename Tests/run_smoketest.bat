@echo off
setlocal


:: 2. Go to root dir and configure CMake
cd %~dp0..
echo --- Configuring CMake with Zig ---
cmake --preset zig-debug
if %ERRORLEVEL% NEQ 0 (
    powershell -Command "Write-Host '[ERROR] CMake config failed.' -ForegroundColor Red"
    pause
    exit /b 1
)

cd build\zig-debug
if %ERRORLEVEL% NEQ 0 (
    powershell -Command "Write-Host '[ERROR] Build dir missing.' -ForegroundColor Red"
    pause
    exit /b 1
)

:: 3. Build test
echo --- Building Smoke Test ---
cmake --build . --target smoketest
if %ERRORLEVEL% NEQ 0 (
    powershell -Command "Write-Host '[ERROR] Build failed!' -ForegroundColor Red"
    pause
    exit /b %ERRORLEVEL%
)

:: 4. Run test
echo --- Running Smoke Test ---
smoketest.exe
if %ERRORLEVEL% NEQ 0 (
    powershell -Command "Write-Host '[ERROR] Test crashed or failed!' -ForegroundColor Red"
    pause
    exit /b %ERRORLEVEL%
)

powershell -Command "Write-Host '[SUCCESS] Test passed! All good.' -ForegroundColor Green"
pause
