@echo off
setlocal enabledelayedexpansion

rem v1: Debug_FLASH only, mirrors flash.bat's scope (see /memories/repo/build.md).
rem Uses pegdbserver_console's own standalone flash mode (erase/program/verify + resume) -
rem no gdb client needed at all, unlike the Segger flow. Verified live end-to-end on
rem S32K344 + PEmicro Multilink Universal FX Rev C.
set "CONFIG=Debug_FLASH"
set "ELF=%CONFIG%\BMS_demo.elf"
set "PEGDBSERVER=C:\NXP\S32DS.3.6.10\eclipse\plugins\com.pemicro.debug.gdbjtag.pne_6.1.8.202603121731\win32\pegdbserver_console.exe"
set "DEVICE=NXP_S32K3xx_S32K344"
set "IFACE=USBMULTILINK"
set "HWPORT=USB1"
set "SPEED=5000"

echo ========================================
echo  S32K344 BMS Flash (PEmicro)
echo ========================================
echo Config    : %CONFIG%
echo ELF       : %ELF%
echo Device    : %DEVICE%
echo Interface : %IFACE%
echo Speed     : %SPEED% kHz
echo ========================================
echo.

if not exist "%CONFIG%\" (
    echo [FAIL] Build config folder not found: %CONFIG%
    goto :fail_pre
)
if not exist "%ELF%" (
    echo [FAIL] ELF not found: %ELF%
    echo        Run build.bat first.
    goto :fail_pre
)
if not exist "%PEGDBSERVER%" (
    echo [FAIL] pegdbserver_console.exe not found: %PEGDBSERVER%
    echo        Check the S32DS install path / PEmicro plugin version.
    goto :fail_pre
)

echo Programming (erase/program/verify) and resuming...
"%PEGDBSERVER%" -device=%DEVICE% -startserver -interface=%IFACE% -port=%HWPORT% -speed=%SPEED% ^
    -flashobjectfile="%CD%\%ELF%" -programmingtype=0 -quitafterprogramming -runafterprogramming
set "PE_RC=%ERRORLEVEL%"

echo.
if "%PE_RC%"=="0" (
    echo ========================================
    echo  FLASH SUCCESS
    echo ========================================
) else (
    echo ========================================
    echo  FLASH FAILED
    echo  Error code: %PE_RC%
    echo ========================================
)
exit /b %PE_RC%

:fail_pre
exit /b 1
