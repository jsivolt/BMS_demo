@echo off
REM ============================================================================
REM  BMS_demo - one-command build
REM
REM    make\build.cmd                 build Debug_FLASH
REM    make\build.cmd Release_FLASH   build a named config
REM    make\build.cmd Debug_RAM
REM    make\build.cmd clean           remove make\build
REM    make\build.cmd rebuild         force full rebuild
REM
REM  Wraps make\build.ps1 (pure PowerShell + the S32DS GNU Arm toolchain).
REM  No Eclipse, no MSYS2 make.
REM ============================================================================
setlocal

set "SCRIPT=%~dp0build.ps1"
set "ARG=%~1"
if "%ARG%"=="" set "ARG=Debug_FLASH"

set "PS_ARGS=-Config Debug_FLASH"

if /I "%ARG%"=="clean"   ( set "PS_ARGS=-Config Debug_FLASH -Clean"   & goto run )
if /I "%ARG%"=="rebuild" ( set "PS_ARGS=-Config Debug_FLASH -Rebuild" & goto run )
set "PS_ARGS=-Config %ARG%"

:run
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %PS_ARGS%
exit /b %ERRORLEVEL%
