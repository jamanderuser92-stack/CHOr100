@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio nicht gefunden. Bitte zuerst installieren, siehe ANLEITUNG.txt
  pause & exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 || (pause & exit /b 1)
cd /d "%~dp0"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || (pause & exit /b 1)
cmake --build build --target Chor100_VST3 || (pause & exit /b 1)
echo.
echo FERTIG! Kopiere den Ordner Chor100.vst3 nach C:\Program Files\Common Files\VST3
explorer "%~dp0build\Chor100_artefacts\Release\VST3"
pause
