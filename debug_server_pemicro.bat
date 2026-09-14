@echo off
rem CMD #1: start PEmicro GDB Server and leave it running.
rem PEmicro-specific: unlike the Segger flow, this server resets the target ONCE AT ITS
rem OWN STARTUP (not per gdb client connect) - see /memories/repo/build.md. If you want to
rem attach to an already-running board without ever resetting it (not even once), pass
rem "attach" as the first arg:
rem   debug_server_pemicro.bat attach
set "PEGDBSERVER=C:\NXP\S32DS.3.6.10\eclipse\plugins\com.pemicro.debug.gdbjtag.pne_6.1.8.202603121731\win32\pegdbserver_console.exe"
set "DEVICE=NXP_S32K3xx_S32K344"
set "IFACE=USBMULTILINK"
set "HWPORT=USB1"
set "SPEED=5000"
set "SERVERPORT=7224"
set "MIPORT=6224"
set "ATTACHFLAG="
rem lower speed in attach mode: reattaching right after a STANDBY wake is less reliable at 5000 kHz
if /i "%~1"=="attach" (
    set "ATTACHFLAG=-attachonly"
    set "SPEED=1000"
)

if not exist "%PEGDBSERVER%" (
    echo [FAIL] pegdbserver_console.exe not found: %PEGDBSERVER%
    exit /b 1
)

"%PEGDBSERVER%" -device=%DEVICE% -startserver %ATTACHFLAG% -interface=%IFACE% -port=%HWPORT% -speed=%SPEED% -serverport=%SERVERPORT% -gdbmiport=%MIPORT%
