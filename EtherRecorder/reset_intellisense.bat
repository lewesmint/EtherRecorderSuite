@echo off 
echo Deleting VS Code IntelliSense database... 
if exist "C:\Users\mintz\AppData\Local\Temp\ipch" rmdir /s /q "C:\Users\mintz\AppData\Local\Temp\ipch" 
echo Reset complete. Please reload VS Code. 
