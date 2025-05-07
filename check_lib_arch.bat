@echo off
echo Checking architecture of library...

:: Path to your library
set LIB_PATH=build\windows-x64\Debug\PlatformLayer.lib

:: Use lib.exe to check the architecture
lib /list %LIB_PATH%

echo.
echo Look for "Machine Type:" in the output
echo "8664h" means x64
echo "AA64h" means ARM64
echo "014Ch" means x86 (32-bit)