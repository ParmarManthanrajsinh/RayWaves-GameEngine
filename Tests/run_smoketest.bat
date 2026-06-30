@echo off
setlocal

:: 1. Setup x64 environment
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist %VCVARS% (
    powershell -Command "Write-Host '[ERROR] Missing %VCVARS%' -ForegroundColor Red"
    pause
    exit /b 1
)
:: Hide vcvars spam
call %VCVARS% >nul 2>&1

:: 2. Go to build dir (script is in Tests, so go up one level)
cd %~dp0..\out\build\x64-debug
if %ERRORLEVEL% NEQ 0 (
    powershell -Command "Write-Host '[ERROR] Build dir missing. Configure CMake first.' -ForegroundColor Red"
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
