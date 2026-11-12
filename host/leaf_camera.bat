@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
python "%~dp0leaf_camera_gui.py"
if errorlevel 1 pause
