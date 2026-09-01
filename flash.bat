@echo off
setlocal enabledelayedexpansion

rem v1: Debug_FLASH only. RAM configs need a different load/pc-set sequence (see /memories/repo/build.md).
set "CONFIG=Debug_FLASH"
set "ELF=%CONFIG%\BMS_demo.elf"
set "JLINK_GDBSERVER=C:\NXP\S32DS.3.6.10\Drivers\Segger\JLinkGDBServerCL.exe"
set "JLINKSCRIPT=C:\NXP\S32DS.3.6.10\Drivers\Segger\JlinkScripts\S32K3xx\S32K344.jlinkscript"
set "GDB_EXE=C:\NXP\S32DS.3.6.10\S32DS\tools\gdb-arm\arm32-eabi\bin\arm-none-eabi-gdb.exe"
set "DEVICE=S32K344"
set "IFACE=SWD"
set "SPEED=1000"
set "PORT=2331"

echo ========================================
echo  S32K344 BMS Flash
echo ========================================
echo Config : %CONFIG%
echo ELF    : %ELF%
echo Device : %DEVICE%
echo IF     : %IFACE%
echo Speed  : %SPEED% kHz
echo Port   : %PORT%
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
if not exist "%JLINK_GDBSERVER%" (
    echo [FAIL] JLinkGDBServerCL.exe not found: %JLINK_GDBSERVER%
    echo        Check the S32DS install path.
    goto :fail_pre
)
if not exist "%JLINKSCRIPT%" (
    echo [FAIL] J-Link script not found: %JLINKSCRIPT%
    echo        Check the S32DS install path.
    goto :fail_pre
)
if not exist "%GDB_EXE%" (
    echo [FAIL] arm-none-eabi-gdb.exe not found: %GDB_EXE%
    echo        Check the S32DS install path.
    goto :fail_pre
)

echo Starting J-Link GDB Server...
start "JLinkGDBServer" /min "%JLINK_GDBSERVER%" -select USB -device %DEVICE% -endian little -if %IFACE% -speed %SPEED% -port %PORT% -jlinkscriptfile "%JLINKSCRIPT%" -singlerun -strict -timeout 8000 -nogui

timeout /t 2 >nul

echo Connecting GDB...
echo Programming...
"%GDB_EXE%" -batch ^
    -ex "target remote localhost:%PORT%" ^
    -ex "monitor reset" ^
    -ex "load" ^
    -ex "monitor reset" ^
    -ex "monitor go" ^
    -ex "detach" ^
    -ex "quit" ^
    "%ELF%"
set "GDB_RC=%ERRORLEVEL%"

echo.
if "%GDB_RC%"=="0" (
    echo ========================================
    echo  FLASH SUCCESS
    echo ========================================
) else (
    echo ========================================
    echo  FLASH FAILED
    echo  Error code: %GDB_RC%
    echo ========================================
)
exit /b %GDB_RC%

:fail_pre
exit /b 1
