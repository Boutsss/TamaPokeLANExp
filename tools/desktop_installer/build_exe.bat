@echo off
rem Builds TamaPokeInstaller.exe (one file, no console window) into
rem tools\desktop_installer\dist\. Needs Python 3.10+ on PATH as "py".
rem
rem The .exe holds the PROGRAM only. To hand it to someone, put it in a folder
rem with the release files it installs -- exactly what web\ holds:
rem   manifest.json, paks.json, firmware\*, sprites-*.pak
rem tools\desktop_installer\package.bat does that.
setlocal
cd /d "%~dp0"
if not exist .venv (
  py -3 -m venv .venv || exit /b 1
  .venv\Scripts\python -m pip install -q pyserial esptool cryptography pyinstaller || exit /b 1
)
rem --collect-data esptool: the flasher stubs it uploads to the chip are data
rem files, and an .exe without them cannot flash anything.
.venv\Scripts\python -m PyInstaller --noconfirm --onefile --windowed ^
  --name TamaPokeInstaller --collect-data esptool --hidden-import serial.tools.list_ports ^
  tamapoke_installer.py || exit /b 1
echo built: %~dp0dist\TamaPokeInstaller.exe
