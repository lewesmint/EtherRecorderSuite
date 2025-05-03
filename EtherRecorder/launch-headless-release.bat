@echo off
echo Starting EtherRecorder (Release) in headless mode...

:: Use start with /b to start the process in the background without creating a new window
:: The /min flag minimizes the window (if one appears)
:: Use cmd /c to allow the command to execute and return control
start "" /b cmd /c "build\windows-arm64\Release\EtherRecorder.exe --headless"

echo EtherRecorder is now running in the background.
echo Use 'tasklist | findstr EtherRecorder' to check if it's running.
echo Use 'taskkill /F /IM EtherRecorder.exe' to terminate it.
