@echo off
setlocal enabledelayedexpansion

rem ---------------------------------------------------------------------------
rem  S32DS install root. Override with:  set S32DS_ROOT=D:\path\to\S32DS  & build.bat
rem  Otherwise: use S32DS_ROOT if set, else pick the newest C:\NXP\S32DS.*\S32DS
rem ---------------------------------------------------------------------------
if not defined S32DS_ROOT (
    for /f "delims=" %%D in ('dir /b /ad /o-n "C:\NXP\S32DS.*" 2^>nul') do (
        if not defined S32DS_ROOT if exist "C:\NXP\%%D\S32DS\build_tools" set "S32DS_ROOT=C:\NXP\%%D\S32DS"
    )
)
if not defined S32DS_ROOT (
    echo ERROR: S32 Design Studio not found under C:\NXP\S32DS.*
    echo        Set S32DS_ROOT to the ...\S32DS folder and retry.
    exit /b 1
)

set "MSYS_BASH=%S32DS_ROOT%\build_tools\msys32\usr\bin\bash.exe"
if not exist "%MSYS_BASH%" (
    echo ERROR: MSYS bash not found at %MSYS_BASH%
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem  Discover the bare-metal Arm GCC: build_tools\gcc_v*\gcc-*-arm32-eabi\bin
rem  (newest version dir first). No hardcoded version number.
rem ---------------------------------------------------------------------------
set "TOOLCHAIN_WIN="
for /f "delims=" %%D in ('dir /b /ad /o-n "%S32DS_ROOT%\build_tools\gcc_v*" 2^>nul') do (
    for /d %%E in ("%S32DS_ROOT%\build_tools\%%D\gcc-*-arm32-eabi") do (
        if not defined TOOLCHAIN_WIN if exist "%%E\bin\arm-none-eabi-gcc.exe" set "TOOLCHAIN_WIN=%%E\bin"
    )
)
if not defined TOOLCHAIN_WIN (
    echo ERROR: no arm-none-eabi GCC found under %S32DS_ROOT%\build_tools\gcc_v*
    exit /b 1
)

rem this script's own folder is the workspace root, so builds work after moving/copying the repo
set "WORKSPACE_DIR=%~dp0"
set "WORKSPACE_DIR=%WORKSPACE_DIR:~0,-1%"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug_FLASH"

set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=all"

echo S32DS     : %S32DS_ROOT%
echo Toolchain : %TOOLCHAIN_WIN%
echo Config    : %CONFIG%   Target: %TARGET%
echo.

rem cygpath (run inside bash) turns the Windows toolchain path into /c/... form
"%MSYS_BASH%" -lc "export PATH=\"$(cygpath -u '%TOOLCHAIN_WIN%'):$PATH\" && cd \"$(cygpath -u '%WORKSPACE_DIR%')/%CONFIG%\" && make -j%NUMBER_OF_PROCESSORS% %TARGET%"

exit /b %ERRORLEVEL%
