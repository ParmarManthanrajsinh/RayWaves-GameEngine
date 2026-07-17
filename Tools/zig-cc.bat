@echo off
:: zig-cc.bat
if exist "%~dp0zig\zig.exe" (
    "%~dp0zig\zig.exe" cc %*
) else (
    zig cc %*
)
