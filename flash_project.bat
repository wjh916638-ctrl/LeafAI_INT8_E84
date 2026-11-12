@echo off
setlocal

set "PROJECT=%~dp0"
set "OPENOCD_ROOT=D:\RT-ThreadStudio\repo\Extract\Debugger_Support_Packages\Infineon\OpenOCD-Infineon\2.0.0"
set "HEX=%PROJECT%Debug\rtthread.hex"

if exist "%HEX%" goto hex_ready
set "HEX=%PROJECT%prebuilt\rtthread.hex"

:hex_ready
set "HEX_TCL=%HEX:\=/%"
set "LOG=%TEMP%\LeafAI_final_flash.log"
set "OPENOCD=%OPENOCD_ROOT%\bin\openocd.exe"
set "QSPI_TCL=D:/RT-ThreadStudio/repo/Extract/Debugger_Support_Packages/Infineon/OpenOCD-Infineon/2.0.0/flm/cypress/cat1d/PSE84_SMIF.FLM"
set /a ATTEMPT=0

if not exist "%OPENOCD%" goto openocd_missing
if not exist "%HEX%" goto hex_missing

:flash_retry
set /a ATTEMPT+=1
echo Flashing LeafAI firmware, attempt %ATTEMPT% of 4...

"%OPENOCD%" -s "%OPENOCD_ROOT%\scripts" -s "%OPENOCD_ROOT%\flm\cypress\cat1d" ^
  -f "interface/kitprog3.cfg" -f "target/infineon/pse84xgxs2.cfg" ^
  -c "set QSPI_FLASHLOADER {%QSPI_TCL%}" -c "transport select swd" ^
  -c "cat1d.cm55 configure -rtos auto" -c "init" -c "reset halt" ^
  -c "flash write_image erase {%HEX_TCL%};" -c "targets cat1d.cm33" ^
  -c "reset run" -c "shutdown" > "%LOG%" 2>&1

findstr /C:"wrote " "%LOG%" >nul
if not errorlevel 1 goto flash_ok

if %ATTEMPT% LSS 4 (
    echo Target was not ready. Check or replug KitProg3, then retrying...
    timeout /t 4 /nobreak >nul
    goto flash_retry
)

echo.
echo Flash failed. OpenOCD log:
type "%LOG%"
pause
exit /b 1

:flash_ok
echo.
echo Flash completed successfully. The board has been reset and started.
pause
exit /b 0

:openocd_missing
echo ERROR: OpenOCD-Infineon 2.0.0 was not found.
echo %OPENOCD%
pause
exit /b 2

:hex_missing
echo ERROR: No firmware HEX file was found.
echo Build the project first, or use prebuilt\rtthread.hex.
pause
exit /b 3
