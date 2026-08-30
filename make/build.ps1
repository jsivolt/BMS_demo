<#
  BMS_demo - standalone command-line build (no Eclipse, no MSYS2 make)

  Pure PowerShell driver for the S32DS GNU Arm toolchain. Mirrors the S32DS
  "Debug_FLASH" managed build: same flags, defines, include paths, linker
  script. Use this when the MSYS2 `make.exe` misbehaves (intermittent
  "cannot execute cc1.exe" / "Permission denied" spawn errors under AV).

  Examples:
     powershell -ExecutionPolicy Bypass -File make\build.ps1
     powershell -ExecutionPolicy Bypass -File make\build.ps1 -Config Release_FLASH
     powershell -ExecutionPolicy Bypass -File make\build.ps1 -Clean
     .\make\build.ps1 -S32DS "D:\NXP\S32DS.3.6.10\S32DS"
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug_FLASH','Release_FLASH','Debug_RAM','Release_RAM')]
    [string]$Config = 'Debug_FLASH',
    [string]$S32DS  = 'C:\NXP\S32DS.3.6.10\S32DS',
    [string]$Toolchain,
    [string]$Sdk,
    [switch]$Clean,
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'
$Top    = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Build  = Join-Path $PSScriptRoot "build\$Config"
$Target = 'BMS_demo'

if (-not $Toolchain) { $Toolchain = Join-Path $S32DS 'build_tools\gcc_v11.4\gcc-11.4-arm32-eabi' }
if (-not $Sdk)       { $Sdk       = Join-Path $S32DS 'software\PlatformSDK_S32K3\RTD' }
$GCC     = Join-Path $Toolchain 'bin\arm-none-eabi-gcc.exe'
$OBJCOPY = Join-Path $Toolchain 'bin\arm-none-eabi-objcopy.exe'
$SIZE    = Join-Path $Toolchain 'bin\arm-none-eabi-size.exe'
$Sysroot = Join-Path $Toolchain 'arm-none-eabi\lib'
foreach ($t in @($GCC,$OBJCOPY,$SIZE)) {
    if (-not (Test-Path $t)) { throw "Toolchain binary not found: $t  (pass -S32DS or -Toolchain)" }
}

# ---- config-specific bits -------------------------------------------------
switch ($Config) {
    'Debug_FLASH'   { $ld = 'linker_flash_s32k344.ld'; $opt = @('-Os','-ggdb3') }
    'Release_FLASH' { $ld = 'linker_flash_s32k344.ld'; $opt = @('-Os') }
    'Debug_RAM'     { $ld = 'linker_ram_s32k344.ld';   $opt = @('-Os','-ggdb3') }
    'Release_RAM'   { $ld = 'linker_ram_s32k344.ld';   $opt = @('-Os') }
}
$LdScript = Join-Path $Top "Project_Settings\Linker_Files\$ld"

if ($Clean -or $Rebuild) {
    if (Test-Path $Build) { Remove-Item -Recurse -Force $Build }
    Write-Host "  CLEAN $Build"
    if ($Clean) { return }
}
New-Item -ItemType Directory -Force -Path $Build | Out-Null

# ---- source list  (kept in step with make/Makefile) ----------------------
$srcDirs = @(
    'src','src\app','src\battery','src\battery\vAFE','src\battery\vPACK',
    'src\communication','src\control','src\drivers','src\safety','src\storage',
    'generate\src','board','RTD\src'
)
$cFiles = foreach ($d in $srcDirs) {
    Get-ChildItem -Path (Join-Path $Top $d) -Filter *.c -File -ErrorAction SilentlyContinue
}
$cFiles += Get-ChildItem -Path (Join-Path $Top 'Project_Settings\Startup_Code') -Filter *.c -File |
           Where-Object { $_.Name -in @('exceptions.c','nvic.c','startup.c','system.c') }
$sFiles = Get-ChildItem -Path (Join-Path $Top 'Project_Settings\Startup_Code') -Filter *.s -File |
          Where-Object { $_.Name -in @('Vector_Table.s','startup_cm7.s') }

# ---- flags  (verbatim from the S32DS *.args files) -----------------------
$inc = @(
    "$Top\RTD\include","$Top\generate\include","$Top\board",
    "$Top\src","$Top\src\app","$Top\src\battery","$Top\src\battery\vAFE",
    "$Top\src\battery\vPACK","$Top\src\communication","$Top\src\control",
    "$Top\src\drivers","$Top\src\safety","$Top\src\storage",
    "$Sdk\BaseNXP_TS_T40D34M70I1R0\header","$Sdk\BaseNXP_TS_T40D34M70I1R0\include",
    "$Sdk\Platform_TS_T40D34M70I1R0\include","$Sdk\Platform_TS_T40D34M70I1R0\startup\include"
) | ForEach-Object { "-I$_" }

$defs = @('-DD_CACHE_ENABLE','-DI_CACHE_ENABLE','-DENABLE_FPU','-DMPU_ENABLE',
          '-DGCC','-DCPU_S32K344','-DCPU_CORTEX_M7')
$arch = @('-mcpu=cortex-m7','-mthumb','-mlittle-endian','-mfloat-abi=hard',
          '-mfpu=fpv5-sp-d16','-specs=nano.specs','-specs=nosys.specs')
$warn = @('-pedantic','-Wall','-Wextra','-Wunused','-Wstrict-prototypes',
          '-Wsign-compare','-Werror=implicit-function-declaration','-Wundef','-Wdouble-promotion')
$cflags = @('-std=c99') + $opt + $arch + $defs + $warn + @(
          '-funsigned-char','-fomit-frame-pointer','-fno-short-enums',
          '-funsigned-bitfields','-fno-common',"--sysroot=$Sysroot")
$asflags = @('-x','assembler-with-cpp','-ggdb3') + $arch +
           @("-I$Top\RTD\include","-I$Top\board","--sysroot=$Sysroot")

# ---- helper: run a tool, retry once on the MSYS-style transient spawn fail
function Invoke-Tool([string]$exe, [string[]]$toolArgs) {
    for ($try = 1; $try -le 2; $try++) {
        & $exe @toolArgs
        if ($LASTEXITCODE -eq 0) { return }
        if ($try -eq 1) { Start-Sleep -Milliseconds 200; continue }
        throw "$([IO.Path]::GetFileName($exe)) failed (exit $LASTEXITCODE)"
    }
}

function Obj-Path($srcFull) {
    $rel = $srcFull.Substring($Top.Length).TrimStart('\')
    $o   = Join-Path $Build ([IO.Path]::ChangeExtension($rel, '.o'))
    New-Item -ItemType Directory -Force -Path (Split-Path $o) | Out-Null
    return $o
}
function Newer($src, $obj) {
    return -not (Test-Path $obj) -or (Get-Item $src).LastWriteTimeUtc -gt (Get-Item $obj).LastWriteTimeUtc
}

$objs = New-Object System.Collections.Generic.List[string]
$built = 0

foreach ($f in $cFiles) {
    $o = Obj-Path $f.FullName
    $objs.Add($o)
    if ($Rebuild -or (Newer $f.FullName $o)) {
        Write-Host "  CC    $($f.FullName.Substring($Top.Length).TrimStart('\'))"
        Invoke-Tool $GCC ($cflags + $inc + @('-c','-o',$o,$f.FullName))
        $built++
    }
}
foreach ($f in $sFiles) {
    $o = Obj-Path $f.FullName
    $objs.Add($o)
    if ($Rebuild -or (Newer $f.FullName $o)) {
        Write-Host "  AS    $($f.FullName.Substring($Top.Length).TrimStart('\'))"
        Invoke-Tool $GCC ($asflags + @('-c','-o',$o,$f.FullName))
        $built++
    }
}

$elf = Join-Path $Build "$Target.elf"
$map = Join-Path $Build "$Target.map"
if ($built -gt 0 -or -not (Test-Path $elf)) {
    Write-Host "  LD    $elf"
    $ldflags = @('-nostartfiles','--entry=Reset_Handler','-ggdb3') + $arch +
               @('-T',$LdScript,"-Wl,-Map,$map","--sysroot=$Sysroot")
    # object list goes through a GCC response file (@file) - same as S32DS -
    # so the command line can't blow the Windows length limit.
    $rsp = Join-Path $Build 'link.rsp'
    ($objs | ForEach-Object { '"' + ($_ -replace '\\','/') + '"' }) -join "`n" | Set-Content -Encoding ascii $rsp
    Invoke-Tool $GCC ($ldflags + @("@$rsp") + @('-lc','-lm','-lgcc','-o',$elf))
    Write-Host "  HEX   $Target.hex"; Invoke-Tool $OBJCOPY @('-O','ihex',$elf,(Join-Path $Build "$Target.hex"))
    Write-Host "  BIN   $Target.bin"; Invoke-Tool $OBJCOPY @('-O','binary',$elf,(Join-Path $Build "$Target.bin"))
} else {
    Write-Host "  (up to date)"
}
Write-Host ''
& $SIZE --format=berkeley $elf
