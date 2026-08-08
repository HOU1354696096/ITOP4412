<#
    build.ps1 - 一键编译 Exynos4412 裸机工程 (BL2 + 主程序)

    用法:
        powershell -ExecutionPolicy Bypass -File tools\build.ps1

    产出 (build 目录):
        bl2.bin      BL2 镜像, 运行在 IRAM 0x02023400 (含时钟+DDR+搬运)
        bl2_14k.bin  BL2 + 4 字节校验和 (14336 字节, 烧录用)
        main.bin     主程序镜像, 运行在 DDR 0x43E00000 (LCD/串口等)

    工具链: xPack arm-none-eabi-gcc (自动在 tools\toolchain 下寻找, 无需手动配置)
#>

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$Root = Split-Path $PSScriptRoot -Parent
$Build = Join-Path $Root 'build'
New-Item -ItemType Directory -Force -Path $Build | Out-Null

# ================= 工具链定位 (相对路径优先, 换电脑不用改本文件) =================
# 1) 首选: 工程自带的绿色工具链  tools\toolchain\xpack-arm-none-eabi-gcc-*\bin
#           (用 tools\toolchain\download_toolchain.ps1 一键下载解压到此处)
# 2) 次选: 系统 PATH 里的 arm-none-eabi-gcc
# 3) 兜底: 作者本机旧绝对路径 (命中会打印强烈警告, 提醒迁移到 tools\toolchain)
# -----------------------------------------------------------------------------
$Toolchain = $null
$TcCandidates = Get-ChildItem (Join-Path $Root 'tools\toolchain') -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'bin\arm-none-eabi-gcc.exe') }
if ($TcCandidates) {
    $Toolchain = Join-Path ($TcCandidates | Select-Object -First 1).FullName 'bin'
    Write-Host "工具链: $Toolchain (工程自带)" -ForegroundColor Green
} else {
    $cmdGcc = Get-Command arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue
    if ($cmdGcc) {
        $Toolchain = Split-Path $cmdGcc.Source -Parent
        Write-Host "工具链: $Toolchain (来自系统 PATH)" -ForegroundColor Green
    } else {
        # 作者机器上的旧位置, 仅作兜底, 强烈建议迁移
        $LegacyTc = 'C:\Users\HOUBANCHAO\AppData\Local\Temp\armtoolchain\xpack\xpack-arm-none-eabi-gcc-15.2.1-1.1\bin'
        if (Test-Path (Join-Path $LegacyTc 'arm-none-eabi-gcc.exe')) {
            $Toolchain = $LegacyTc
            Write-Warning "正在使用【旧绝对路径】工具链: $LegacyTc"
            Write-Warning "强烈建议运行 tools\toolchain\download_toolchain.ps1,"
            Write-Warning "把工具链放到工程 tools\toolchain\ 下, 否则换电脑/换目录后无法编译!"
        } else {
            Write-Host '找不到 arm-none-eabi-gcc 交叉编译器!' -ForegroundColor Red
            Write-Host '请先运行:  powershell -ExecutionPolicy Bypass -File tools\toolchain\join_toolchain.ps1' -ForegroundColor Yellow
            Write-Host '         (仓库自带分片, 离线恢复; 分片缺失时用 download_toolchain.ps1 在线下载)' -ForegroundColor Yellow
            exit 1
        }
    }
}

$CC      = Join-Path $Toolchain 'arm-none-eabi-gcc.exe'
$LD      = Join-Path $Toolchain 'arm-none-eabi-ld.exe'
$OBJCOPY = Join-Path $Toolchain 'arm-none-eabi-objcopy.exe'
$OBJDUMP = Join-Path $Toolchain 'arm-none-eabi-objdump.exe'
$SIZE    = Join-Path $Toolchain 'arm-none-eabi-size.exe'

if (-not (Test-Path $CC)) {
    Write-Host "找不到工具链: $CC" -ForegroundColor Red
    exit 1
}

$CFLAGS  = @('-mcpu=cortex-a9', '-marm', '-mfloat-abi=soft', '-O2', '-g',
              '-Wall', '-Wextra', '-ffreestanding', '-fno-builtin',
              '-fno-common', '-nostdlib', '-nostartfiles',
              '-I', (Join-Path $Root 'Libraries\inc'),
              '-I', (Join-Path $Root 'User'),
              '-I', (Join-Path $Root 'User\App'))
$ASFLAGS = @('-mcpu=cortex-a9', '-marm', '-g')

function Invoke-Checked {
    param([string]$Exe, [string[]]$ArgList)
    & $Exe @ArgList
    if ($LASTEXITCODE -ne 0) {
        throw "命令失败: $Exe $($ArgList -join ' ')"
    }
}

function Compile-Src {
    param([string]$Src, [string]$Out, [string[]]$Extra)
    $abs = Join-Path $Root $Src
    if ($Src -match '\.S$') {
        Invoke-Checked $CC (@($ASFLAGS) + $Extra + @('-c', '-o', $Out, $abs))
    } else {
        Invoke-Checked $CC (@($CFLAGS) + $Extra + @('-c', '-o', $Out, $abs))
    }
}

# ---------------- 1. BL2 (IRAM 0x02023400) ----------------
Write-Host '==> 编译 BL2 ...'
$Bl2Dir = Join-Path $Build 'bl2'
New-Item -ItemType Directory -Force -Path $Bl2Dir | Out-Null

$Bl2Srcs = @(
    'startup\start.S',
    'startup\aeabi_div.S',
    'User\App\system_4412.c',
    'Libraries\src\exynos4412_clock.c',
    'Libraries\src\exynos4412_ddr.c',
    'Libraries\src\exynos4412_gpio.c',
    'Libraries\src\exynos4412_uart.c'
)

$Bl2Objs = @()
foreach ($src in $Bl2Srcs) {
    $obj = Join-Path $Bl2Dir (($src -replace '[\\/]', '_') -replace '\.(c|S)$', '.o')
    Compile-Src $src $obj @('-DEXYNOS4412_BOOT_SD')
    $Bl2Objs += $obj
}

$Bl2Elf = Join-Path $Build 'bl2.elf'
$Bl2Bin = Join-Path $Build 'bl2.bin'
$LdArgs = @('-T', (Join-Path $Root 'startup\exynos4412.lds'),
            '--defsym', '__LINK_BASE=0x02023400',
            '-Map', (Join-Path $Build 'bl2.map'),
            '-o', $Bl2Elf) + $Bl2Objs
Invoke-Checked $LD $LdArgs
Invoke-Checked $OBJCOPY @('-O', 'binary', $Bl2Elf, $Bl2Bin)

$bl2Size = (Get-Item $Bl2Bin).Length
Write-Host ("BL2 大小: {0} 字节 (上限 14332)" -f $bl2Size)
if ($bl2Size -gt 14332) { throw 'BL2 超过 14KB 限制!' }

# 生成 14336 字节的 BL2 烧录镜像 (前 14332 字节固件 + 4 字节累加校验和)
$bl2_14k = New-Object byte[] 14336
for ($i = 0; $i -lt 14336; $i++) { $bl2_14k[$i] = 0xFF }
$bl2Raw = [System.IO.File]::ReadAllBytes($Bl2Bin)
[System.Array]::Copy($bl2Raw, 0, $bl2_14k, 0, $bl2Raw.Length)
$sum = 0L
for ($i = 0; $i -lt 14332; $i++) { $sum += $bl2_14k[$i] }
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$sum), 0, $bl2_14k, 14332, 4)
$Bl2_14kPath = Join-Path $Build 'bl2_14k.bin'
[System.IO.File]::WriteAllBytes($Bl2_14kPath, $bl2_14k)
Write-Host ("BL2 烧录镜像: {0} 字节, 校验和 0x{1:X8}" -f $bl2_14k.Length, $sum)

# ---------------- 2. 主程序 (DDR 0x43E00000) ----------------
Write-Host '==> 编译主程序 (DDR)...'
$MainDir = Join-Path $Build 'main'
New-Item -ItemType Directory -Force -Path $MainDir | Out-Null

$MainSrcs = @(
    'startup\main_start.S',
    'startup\aeabi_div.S',
    'User\main.c',
    'User\App\led.c',
    'User\App\panel.c',
    'User\App\debug.c',
    'Libraries\src\exynos4412_lcd.c',
    'Libraries\src\exynos4412_buzzer.c',
    'Libraries\src\exynos4412_key.c',
    'Libraries\src\exynos4412_clock.c',
    'Libraries\src\exynos4412_gpio.c',
    'Libraries\src\exynos4412_uart.c'
)

$MainObjs = @()
foreach ($src in $MainSrcs) {
    $obj = Join-Path $MainDir (($src -replace '[\\/]', '_') -replace '\.(c|S)$', '.o')
    Compile-Src $src $obj @()
    $MainObjs += $obj
}

$MainElf = Join-Path $Build 'main.elf'
$MainBin = Join-Path $Build 'main.bin'
$LdArgs = @('-T', (Join-Path $Root 'startup\main.lds'),
            '-Map', (Join-Path $Build 'main.map'),
            '-o', $MainElf) + $MainObjs
Invoke-Checked $LD $LdArgs
Invoke-Checked $OBJCOPY @('-O', 'binary', $MainElf, $MainBin)

$mainSize = (Get-Item $MainBin).Length
Write-Host ("主程序大小: {0} 字节 (上限 524288)" -f $mainSize)
if ($mainSize -gt 524288) { throw '主程序超过 512KB 限制!' }

# ---------------- 3. 反汇编与体积信息 ----------------
Invoke-Checked $OBJDUMP @('-D', $Bl2Elf) | Out-File (Join-Path $Build 'bl2.dis') -Encoding ascii
Invoke-Checked $OBJDUMP @('-D', $MainElf) | Out-File (Join-Path $Build 'main.dis') -Encoding ascii
& $SIZE $Bl2Elf $MainElf

Write-Host ''
Write-Host '编译完成! 接下来执行:' -ForegroundColor Green
Write-Host '    powershell -ExecutionPolicy Bypass -File tools\burn_sd.ps1 -Disk 1' -ForegroundColor Green
