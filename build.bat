@echo off
setlocal

set "MSYS_BASH=C:\NXP\S32DS.3.6.10\S32DS\build_tools\msys32\usr\bin\bash.exe"
set "TOOLCHAIN_BIN=/c/NXP/S32DS.3.6.10/S32DS/build_tools/gcc_v10.2/gcc-10.2-arm32-eabi/bin"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug_FLASH"

set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=all"

if not exist "%MSYS_BASH%" (
    echo ERROR: MSYS bash not found at %MSYS_BASH%
    exit /b 1
)

"%MSYS_BASH%" -lc "export PATH=\"%TOOLCHAIN_BIN%:$PATH\" && cd /c/S32K344/workspace/BMS_demo/%CONFIG% && make -j28 %TARGET%"

exit /b %ERRORLEVEL%
