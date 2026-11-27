@echo off
rem Makes the folder to hand to a friend: dist\TamaPoke Installer\
rem   TamaPokeInstaller.exe + the release it installs (copied from web\) + READ ME.txt
rem
rem Run tools\build_web.sh first if the firmware changed: the .exe installs
rem exactly what web\ holds, the same release the web installer serves.
setlocal
cd /d "%~dp0"
rem By full path: with NoDefaultCurrentDirectoryInExePath set, cmd will not
rem run a bare "build_exe.bat" from the current folder.
if not exist dist\TamaPokeInstaller.exe call "%~dp0build_exe.bat" || exit /b 1
set OUT=dist\TamaPoke Installer
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%\firmware" || exit /b 1
copy /y dist\TamaPokeInstaller.exe "%OUT%\" >nul
copy /y ..\..\web\manifest.json "%OUT%\" >nul
copy /y ..\..\web\paks.json "%OUT%\" >nul
copy /y ..\..\web\firmware\bootloader.bin "%OUT%\firmware\" >nul
copy /y ..\..\web\firmware\partitions.bin "%OUT%\firmware\" >nul
copy /y ..\..\web\firmware\boot_app0.bin "%OUT%\firmware\" >nul
copy /y ..\..\web\firmware\app.bin.enc "%OUT%\firmware\" >nul
copy /y ..\..\web\sprites-*.pak "%OUT%\" >nul
copy /y "READ ME.txt" "%OUT%\" >nul
echo packaged: %~dp0%OUT%
