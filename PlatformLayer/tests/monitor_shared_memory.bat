@echo off
echo Windows Shared Memory Monitor
echo ===========================
echo.

REM Get the current directory
set SCRIPT_DIR=%~dp0

REM Run the monitor using configuration from shared_memory_config.jsonc
echo Starting shared memory monitor...
echo Using configuration from shared_memory_config.jsonc
echo Make sure the C test program is running in another window.
echo.
python "%SCRIPT_DIR%\test_win_shared_memory.py"

pause
