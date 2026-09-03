@echo off
rem CMD #1: start J-Link GDB Server and leave it running.
set "JLINK_GDBSERVER=C:\NXP\S32DS.3.6.10\Drivers\Segger\JLinkGDBServerCL.exe"
set "JLINKSCRIPT=C:\NXP\S32DS.3.6.10\Drivers\Segger\JlinkScripts\S32K3xx\S32K344.jlinkscript"
set "DEVICE=S32K344"
set "IFACE=SWD"
set "SPEED=4000"
set "PORT=2331"

if not exist "%JLINK_GDBSERVER%" (
    echo [FAIL] JLinkGDBServerCL.exe not found: %JLINK_GDBSERVER%
    exit /b 1
)

"%JLINK_GDBSERVER%" -device %DEVICE% -if %IFACE% -speed %SPEED% -port %PORT% -jlinkscriptfile "%JLINKSCRIPT%"
