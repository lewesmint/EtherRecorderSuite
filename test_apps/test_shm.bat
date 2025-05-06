@echo off
echo Compiling test_create_shm.c...
cl /nologo /W4 test_create_shm.c /link /out:test_create_shm.exe

echo Compiling test_open_shm.c...
cl /nologo /W4 test_open_shm.c /link /out:test_open_shm.exe

echo.
echo Compilation complete.
echo.
echo To test:
echo 1. Run test_create_shm.exe in one command prompt
echo 2. While that is running, run test_open_shm.exe in another command prompt
echo.
echo You can specify a custom name and size:
echo   test_create_shm.exe MySharedMemory 4096
echo   test_open_shm.exe MySharedMemory 4096
echo.