@echo off
setlocal

set "MSYS_BASH=C:\NXP\S32DS.3.6.10\S32DS\build_tools\msys32\usr\bin\bash.exe"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug_FLASH"

if not exist "%MSYS_BASH%" (
    echo ERROR: MSYS bash not found at %MSYS_BASH%
    exit /b 1
)

rem Only remove actual build outputs; never "make clean" (rm -rf ./*), which
rem also wipes the Eclipse/CDT-generated .args response files that plain make
rem cannot regenerate on its own.
"%MSYS_BASH%" -lc "cd /c/S32K344/workspace/BMS_demo/%CONFIG% && find . -type f \( -name '*.o' -o -name '*.d' -o -name '*.elf' -o -name '*.map' -o -name '*.siz' \) -delete"

exit /b %ERRORLEVEL%
