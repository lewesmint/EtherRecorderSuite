@echo off
echo Windows Shared Memory Test
echo ========================
echo.

REM Get the current directory
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%\..

REM Path to the C test program executable
set C_TEST_PROGRAM=%PROJECT_ROOT%\build\Debug\bin\test_win_shared_memory.exe

REM Check if the executable exists
if not exist "%C_TEST_PROGRAM%" (
    echo ERROR: C test program not found at %C_TEST_PROGRAM%
    echo Make sure you have built the project in Debug configuration.
    echo.
    echo You may need to build it with:
    echo   cd %PROJECT_ROOT%\build\Debug
    echo   cmake --build . --target test_win_shared_memory --config Debug
    pause
    exit /b 1
)

REM Run the C test program
echo Running C test program to create shared memory...
echo The C program will run for 60 seconds to keep the shared memory open.
echo.
start "C Test Program" "%C_TEST_PROGRAM%"

REM Wait a moment for the shared memory to be created
echo Waiting for shared memory to be created...
timeout /t 2 > nul

REM Run the Python monitoring script
echo.
echo Running Python monitoring script...
echo Using configuration from shared_memory_config.jsonc
python "%SCRIPT_DIR%\test_win_shared_memory.py"

echo.
echo Test completed.
pause
