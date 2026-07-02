@echo off
:: zig-cxx.bat
if exist "%~dp0zig\zig.exe" (
    "%~dp0zig\zig.exe" c++ %*
) else (
    zig c++ %*
)
