@echo off
setlocal enabledelayedexpansion

:: ============================================================================
::  run_analysis.bat — clang-tidy performance analysis + clang-format checks
::  Usage:  run_analysis.bat [command] [options]
::
::  Commands:
::    tidy          Run clang-tidy (all checks from .clang-tidy)
::    tidy-perf     Run only performance-* checks
::    tidy-fix      Run clang-tidy and apply fixes
::    format        Format all source files in-place with clang-format
::    format-check  Check formatting (exit 1 if any file is unformatted)
::    build-tidy    Build with clang-tidy enabled via CMake
::    report        Run tidy + dump summary grouped by diagnostic
::    all           Full pipeline: format-check -> tidy -> build
::    help          Show this help
::
::  Options:
::    --preset <name>    CMake preset (default: zig-debug)
::    --jobs <n>         Parallel clang-tidy jobs (default: 0 = all cores)
::    --source <dir>     Limit to specific source dir (e.g. Engine, Editor)
:: ============================================================================

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%.." || exit /b 1
set "PROJECT_ROOT=%CD%"
popd

:: --- defaults ---------------------------------------------------------------
set "LLVM_DIR=C:\Program Files\LLVM\bin"
set "PRESET=zig-debug"
set "JOBS=0"
set "SOURCE_FILTER="
set "CMD="

:: --- parse args -------------------------------------------------------------
:parse
if "%~1"=="" goto :done_parse
if /i "%~1"=="--preset"    set "PRESET=%~2"& shift & shift & goto :parse
if /i "%~1"=="--jobs"      set "JOBS=%~2"& shift & shift & goto :parse
if /i "%~1"=="--source"    set "SOURCE_FILTER=%~2"& shift & shift & goto :parse
if /i "%~1"=="help"        set "CMD=help"& shift & goto :parse
if /i "%~1"=="tidy"        set "CMD=tidy"& shift & goto :parse
if /i "%~1"=="tidy-perf"   set "CMD=tidy-perf"& shift & goto :parse
if /i "%~1"=="tidy-fix"    set "CMD=tidy-fix"& shift & goto :parse
if /i "%~1"=="format"      set "CMD=format"& shift & goto :parse
if /i "%~1"=="format-check" set "CMD=format-check"& shift & goto :parse
if /i "%~1"=="build-tidy"  set "CMD=build-tidy"& shift & goto :parse
if /i "%~1"=="report"      set "CMD=report"& shift & goto :parse
if /i "%~1"=="all"         set "CMD=all"& shift & goto :parse
echo Unknown argument: %1
exit /b 1
:done_parse

if "%CMD%"=="" set "CMD=help"

:: --- locate tools -----------------------------------------------------------
set "CLANG_TIDY="
set "CLANG_FORMAT="
if exist "%LLVM_DIR%\clang-tidy.exe"     set "CLANG_TIDY=%LLVM_DIR%\clang-tidy.exe"
if exist "%LLVM_DIR%\clang-format.exe"   set "CLANG_FORMAT=%LLVM_DIR%\clang-format.exe"

if "%CLANG_TIDY%"=="" (
    where clang-tidy.exe >nul 2>&1
    if !ERRORLEVEL! equ 0 set "CLANG_TIDY=clang-tidy.exe"
)
if "%CLANG_FORMAT%"=="" (
    where clang-format.exe >nul 2>&1
    if !ERRORLEVEL! equ 0 set "CLANG_FORMAT=clang-format.exe"
)

:: --- build dir setup --------------------------------------------------------
set "BUILD_DIR=%PROJECT_ROOT%\build\%PRESET%"
set "COMPILE_COMMANDS=%BUILD_DIR%\compile_commands.json"

:: --- helpers ----------------------------------------------------------------
goto :start_commands

:fail
    echo [FAIL] %*
    exit /b 1

:ensure_compile_commands
    if exist "%COMPILE_COMMANDS%" exit /b 0
    echo == Generating compile_commands.json via cmake preset %PRESET% ...
    cmake --preset "%PRESET%" -B "%BUILD_DIR%" 2>&1
    if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
    if not exist "%COMPILE_COMMANDS%" call :fail "compile_commands.json not generated"
    exit /b 0

:: collect source files matching filter, skip imgui/tinyfiledialogs/vendored
:collect_sources
    setlocal
    set "OUT=%TEMP%\analysis_sources_%RANDOM%.txt"
    if "%SOURCE_FILTER%"=="" (
        dir /s /b "%PROJECT_ROOT%\Engine\*.cpp" "%PROJECT_ROOT%\Editor\*.cpp" "%PROJECT_ROOT%\Game\*.cpp" "%PROJECT_ROOT%\Distribution\Templates\*.cpp" "%PROJECT_ROOT%\Tests\*.cpp" 2>nul | findstr /v "imgui tinyfiledialogs" > "%OUT%"
    ) else (
        set "FP=%PROJECT_ROOT%\%SOURCE_FILTER%"
        for %%f in ("!FP!") do if /i "%%~xf"==".cpp" (
            if exist "%%~f" echo %%~f > "%OUT%"
        )
        if not exist "%OUT%" (
            if exist "!FP!\" (
                dir /s /b "!FP!\*.cpp" 2>nul | findstr /v "imgui tinyfiledialogs" > "%OUT%"
            ) else if exist "!FP!" (
                echo !FP! > "%OUT%"
            )
        )
    )
    if not exist "%OUT%" call :fail "no source files found"
    type "%OUT%"
    endlocal & set "SOURCE_LIST=%OUT%"
    exit /b 0

:: --- commands ---------------------------------------------------------------
:start_commands

if "%CMD%"=="help" (
    echo.
    echo == RayWaves Analysis Tool ==
    echo.
    echo Usage:  run_analysis.bat [command] [options]
    echo.
    echo Commands:
    echo   tidy           Run clang-tidy ^(all checks from .clang-tidy^)
    echo   tidy-perf      Run only performance-* checks
    echo   tidy-fix       Run clang-tidy and apply fixes
    echo   format         Format all source files in-place
    echo   format-check   Check formatting ^(exit 1 if any unformatted^)
    echo   build-tidy     Configure and build with clang-tidy enabled
    echo   report         Run tidy + dump summary grouped by diagnostic
    echo   all            Full pipeline: format-check -^> tidy -^> build
    echo.
    echo Options:
    echo   --preset ^<name^>     CMake preset ^(default: zig-debug^)
    echo   --jobs ^<n^>          Parallel clang-tidy jobs ^(default: 0 = all cores^)
    echo   --source ^<dir^>      Limit to specific source dir
    echo.
    if not "!CLANG_TIDY!"=="" (  echo   clang-tidy : found at !CLANG_TIDY! ) else ( echo   clang-tidy : NOT FOUND )
    if not "!CLANG_FORMAT!"=="" ( echo   clang-format: found at !CLANG_FORMAT! ) else ( echo   clang-format: NOT FOUND )
    echo   compile_commands: !COMPILE_COMMANDS!
    if exist "!COMPILE_COMMANDS!" (echo                  ^(exists^)) else (echo                  ^(missing - will auto-generate^))
    echo.
    goto :end
)

:: ---- ensure we have the tools we need --------------------------------------
if "%CMD%"=="tidy"      call :require_tidy
if "%CMD%"=="tidy-perf" call :require_tidy
if "%CMD%"=="tidy-fix"  call :require_tidy
if "%CMD%"=="report"    call :require_tidy
if "%CMD%"=="all"       call :require_tidy & call :require_format
if "%CMD%"=="format"    call :require_format
if "%CMD%"=="format-check" call :require_format

:: ---- build-tidy ------------------------------------------------------------
if "%CMD%"=="build-tidy" (
    echo == Configuring with clang-tidy enabled ...
    cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -DENABLE_CLANG_TIDY=ON 2>&1
    if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
    echo == Building ...
    cmake --build "%BUILD_DIR%" -- -j%JOBS%
    exit /b !ERRORLEVEL!
)

:: ---- format ----------------------------------------------------------------
if "%CMD%"=="format" (
    echo == Formatting all source files ...
    for %%f in (
        "%PROJECT_ROOT%\Engine\*.cpp"    "%PROJECT_ROOT%\Engine\*.h"
        "%PROJECT_ROOT%\Editor\*.cpp"    "%PROJECT_ROOT%\Editor\*.h"
        "%PROJECT_ROOT%\Editor\Panels\*.cpp" "%PROJECT_ROOT%\Editor\Panels\*.h"
        "%PROJECT_ROOT%\Editor\terminal\*.cpp" "%PROJECT_ROOT%\Editor\terminal\*.h"
        "%PROJECT_ROOT%\Game\*.cpp"      "%PROJECT_ROOT%\Game\*.h"
        "%PROJECT_ROOT%\Tests\*.cpp"     "%PROJECT_ROOT%\Tests\*.h"
        "%PROJECT_ROOT%\Distribution\Templates\DemoGame\GameLogic\*.cpp"
        "%PROJECT_ROOT%\Distribution\Templates\DemoGame\GameLogic\*.h"
        "%PROJECT_ROOT%\Distribution\Templates\Platformer2D\GameLogic\*.cpp"
        "%PROJECT_ROOT%\Distribution\Templates\Platformer2D\GameLogic\*.h"
        "%PROJECT_ROOT%\Distribution\Templates\Empty\GameLogic\*.cpp"
        "%PROJECT_ROOT%\Distribution\Templates\Empty\GameLogic\*.h"
    ) do (
        if exist "%%f" (
            "%CLANG_FORMAT%" -i -style=file "%%f"
        )
    )
    echo == Formatting done.
    goto :end
)

if "%CMD%"=="format-check" (
    echo == Checking formatting ...
    set "UNFORMATTED=0"
    for %%f in (
        "%PROJECT_ROOT%\Engine\*.cpp"    "%PROJECT_ROOT%\Engine\*.h"
        "%PROJECT_ROOT%\Editor\*.cpp"    "%PROJECT_ROOT%\Editor\*.h"
        "%PROJECT_ROOT%\Editor\Panels\*.cpp" "%PROJECT_ROOT%\Editor\Panels\*.h"
        "%PROJECT_ROOT%\Editor\terminal\*.cpp" "%PROJECT_ROOT%\Editor\terminal\*.h"
        "%PROJECT_ROOT%\Game\*.cpp"      "%PROJECT_ROOT%\Game\*.h"
        "%PROJECT_ROOT%\Tests\*.cpp"     "%PROJECT_ROOT%\Tests\*.h"
        "%PROJECT_ROOT%\Distribution\Templates\DemoGame\GameLogic\*.cpp"
        "%PROJECT_ROOT%\Distribution\Templates\DemoGame\GameLogic\*.h"
        "%PROJECT_ROOT%\Distribution\Templates\Platformer2D\GameLogic\*.cpp"
        "%PROJECT_ROOT%\Distribution\Templates\Platformer2D\GameLogic\*.h"
        "%PROJECT_ROOT%\Distribution\Templates\Empty\GameLogic\*.cpp"
        "%PROJECT_ROOT%\Distribution\Templates\Empty\GameLogic\*.h"
    ) do (
        if exist "%%f" (
            "%CLANG_FORMAT%" -n -style=file -Werror "%%f" 2>&1 | findstr /i "error" >nul
            if !ERRORLEVEL! equ 0 (
                echo [UNFORMATTED] %%f
                set "UNFORMATTED=1"
            )
        )
    )
    if "!UNFORMATTED!"=="1" (
        echo == Format check FAILED. Run 'format' to fix.
        exit /b 1
    )
    echo == All files are properly formatted.
    goto :end
)

:: ---- tidy / tidy-perf / tidy-fix / report ----------------------------------
if "%CMD%"=="tidy" (
    set "TIDY_CHECKS="
    call :run_tidy
    exit /b !ERRORLEVEL!
)
if "%CMD%"=="tidy-perf" (
    set "TIDY_CHECKS=--checks=performance-*"
    call :run_tidy
    exit /b !ERRORLEVEL!
)
if "%CMD%"=="tidy-fix" (
    set "TIDY_CHECKS=--fix"
    call :run_tidy
    exit /b !ERRORLEVEL!
)
if "%CMD%"=="report" (
    set "TIDY_CHECKS="
    set "REPORT_MODE=1"
    call :run_tidy
    exit /b !ERRORLEVEL!
)
goto :eof

:run_tidy
    call :ensure_compile_commands
    call :collect_sources

    set "TIDY_LOG=%BUILD_DIR%\tidy_report.txt"

    echo == Running clang-tidy on sources listed in !SOURCE_LIST! ...
    echo == Log: !TIDY_LOG!

    >"%TIDY_LOG%" 2>&1 (
        for /f "usebackq delims=" %%f in ("%SOURCE_LIST%") do (
            "%CLANG_TIDY%" -p "%BUILD_DIR%" -quiet %TIDY_CHECKS% "%%f"
        )
    )
    set "TIDY_EXIT=!ERRORLEVEL!"

    if defined REPORT_MODE (
        set "WARN_FILE=%TEMP%\tidy_warnings_!RANDOM!"
        findstr /r "warning:" "%TIDY_LOG%" > "!WARN_FILE!" 2>nul
        for %%? in ("!WARN_FILE!") do set "WARN_SIZE=%%~z?"
        if defined WARN_SIZE if !WARN_SIZE! gtr 0 (
            echo.
            echo == Grouped by check ==
            sort "!WARN_FILE!"
        ) else (
            echo No warnings found. Clean!
        )
        del "!WARN_FILE!" 2>nul
        echo.
        echo Full log: !TIDY_LOG!
    )

    exit /b

:: ---- all -------------------------------------------------------------------
if "%CMD%"=="all" (
    call :ensure_compile_commands
    echo.
    echo == PHASE 1: Format check ==
    call "%SCRIPT_DIR%run_analysis.bat" format-check --preset "%PRESET%"
    if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
    echo.
    echo == PHASE 2: clang-tidy ==
    call "%SCRIPT_DIR%run_analysis.bat" tidy --preset "%PRESET%"
    if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
    echo.
    echo == PHASE 3: Build ==
    cmake --build "%BUILD_DIR%" -- -j%JOBS%
    exit /b !ERRORLEVEL!
)

:: --- subroutines ------------------------------------------------------------
:require_tidy
    if "%CLANG_TIDY%"=="" call :fail "clang-tidy not found. Install LLVM or add to PATH."
    exit /b

:require_format
    if "%CLANG_FORMAT%"=="" call :fail "clang-format not found. Install LLVM or add to PATH."
    exit /b

:: --- cleanup ----------------------------------------------------------------
:end
    if defined SOURCE_LIST (
        if exist "%SOURCE_LIST%" del "%SOURCE_LIST%" 2>nul
    )
    endlocal
    exit /b 0
