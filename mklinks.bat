@echo off
setlocal
set "BSP=D:\RT-ThreadStudio\repo\Local\Board_Support_Packages\Infineon\PSOC_E84-EDGI-TALK\1.4.0"
cd /d "%~dp0"

if not exist "%BSP%\rt-thread" goto bsp_missing

if not exist "rt-thread" mklink /J rt-thread "%BSP%\rt-thread"
if not exist "libraries" mklink /J libraries "%BSP%\libraries"
if not exist "libs" mklink /J libs "%BSP%\projects\libs"
if not exist "tools" mklink /J tools "%BSP%\tools"
exit /b 0

:bsp_missing
echo ERROR: PSoC E84 Edgi-Talk BSP 1.4.0 was not found.
echo %BSP%
pause
exit /b 1
