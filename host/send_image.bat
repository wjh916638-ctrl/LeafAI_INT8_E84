@echo off
chcp 65001 >nul
setlocal
set "SCRIPT=%~dp0leaf_send.py"
if "%~1"=="" (
  echo Usage: drag a leaf image onto this file, or:
  echo    send_image.bat path\to\leaf.jpg [COM7]
  echo Samples: %~dp0..\samples
  pause
  exit /b 1
)
set "PORT=%~2"
if "%PORT%"=="" set "PORT=COM7"
python "%SCRIPT%" --port %PORT% --image "%~1"
pause
