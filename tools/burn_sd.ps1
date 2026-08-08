<#
    burn_sd.ps1 - 向 SD 卡烧写 Exynos4412 裸机启动镜像 (BL1 + BL2 + 主程序)

    用法 (需要管理员权限, 会弹 UAC):
        powershell -ExecutionPolicy Bypass -File tools\burn_sd.ps1 -Disk 1

    前提:
      1. 先执行  powershell -ExecutionPolicy Bypass -File tools\build.ps1
         生成 build\bl2.bin 和 build\main.bin
      2. SD 卡已插入, 确认磁盘号 (Get-Disk 查看, 一般 USB 读卡器为 1)

    烧写布局 (与三星 E4412 / 迅为 iTOP-4412 标准一致):
      扇区 0     : 保留 (MBR, 不动)
      扇区 1~16  : BL1 (E4412_N.bl1.bin, 8KB)
      扇区 17~48 : BL2 (14KB 代码 + 4 字节校验和, 共 14336 字节)
      扇区 49~1072: 主程序 main.bin (填充到 512KB), 由 BL2 搬运到 DDR 0x43E00000

    BL2 格式 (来自迅为 chksum.c / U-Boot mkexynosspl):
      前 14332 字节 = 固件 (不足用 0xFF 填充), 末 4 字节 = 前 14332 字节累加校验和(小端)
#>

param(
    [int]$Disk = 1
)

$ErrorActionPreference = 'Stop'
$ResultLog = Join-Path $env:TEMP 'burn_sd_result.log'
Remove-Item $ResultLog -ErrorAction SilentlyContinue

trap {
    Add-Content -Path $ResultLog -Value ("ERROR: " + $_.Exception.Message) -Encoding utf8
    Add-Content -Path $ResultLog -Value ("AT: " + $_.InvocationInfo.PositionMessage) -Encoding utf8
    Add-Content -Path $ResultLog -Value ($_ | Format-List * -Force | Out-String) -Encoding utf8
    exit 1
}

function StepLog($msg) {
    Add-Content -Path $ResultLog -Value ("STEP: " + $msg) -Encoding ascii
}

$Root = Split-Path $PSScriptRoot -Parent
StepLog 'script started'
StepLog ("Disk param type=" + $Disk.GetType().Name + " value=" + $Disk)
# BL1 位于 tools\bl1\ 下 (若旧位置存在则兼容)
$Bl1Path    = Join-Path $PSScriptRoot 'bl1\E4412.S.BL1.SSCR.EVT1.1.bin'
if (-not (Test-Path $Bl1Path)) { $Bl1Path = Join-Path $PSScriptRoot 'E4412.S.BL1.SSCR.EVT1.1.bin' }
$Bl2RawPath = Join-Path $Root 'build\bl2.bin'
$MainPath   = Join-Path $Root 'build\main.bin'
$Bl2Path    = Join-Path $Root 'build\bl2_14k.bin'
StepLog 'paths assigned'

# ---------- 0. 管理员检查 ----------
$id = [System.Security.Principal.WindowsIdentity]::GetCurrent()
$pr = New-Object System.Security.Principal.WindowsPrincipal($id)
if (-not $pr.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host '需要管理员权限! 请用右键"以管理员身份运行", 或通过 sudo / UAC 提权执行。' -ForegroundColor Red
    Set-Content -Path $ResultLog -Value 'ERROR: not admin' -Encoding ascii
    exit 1
}
StepLog 'admin check done'

# ---------- 1. 目标盘安全检查 (直接读 MBR, 不依赖 Storage/CIM 模块) ----------
$dev = '\\.\PhysicalDrive' + $Disk
$probe = [System.IO.File]::Open($dev, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
try {
    $mbr = New-Object byte[] 512
    $n = $probe.Read($mbr, 0, 512)
    if ($n -ne 512) { throw '无法读取扇区 0' }
    $sig = ($mbr[510] -eq 0x55) -and ($mbr[511] -eq 0xAA)
    $p1start = [System.BitConverter]::ToUInt32($mbr, 0x1BE + 8)
    StepLog ("sector0 signature=" + $sig + " partition1_start_LBA=" + $p1start)
    Write-Host ("目标: PhysicalDrive" + $Disk + " | MBR有效=" + $sig + " | 分区1起始扇区=" + $p1start)
    if ($p1start -gt 0 -and $p1start -lt 49) {
        Write-Host "警告: 分区1起始扇区($p1start)与 BL1/BL2/主程序区域(1~1072)冲突, 可能已有启动镜像!" -ForegroundColor Yellow
    }
} finally {
    $probe.Close()
}
StepLog 'safety check done'

# ---------- 2. 检查输入文件 ----------
if (-not (Test-Path $Bl1Path)) { Write-Host "缺少 $Bl1Path" -ForegroundColor Red; Set-Content $ResultLog 'ERROR: BL1 missing' -Encoding ascii; exit 1 }
if (-not (Test-Path $Bl2RawPath)) { Write-Host "缺少 $Bl2RawPath, 请先执行 tools\build.ps1" -ForegroundColor Red; Set-Content $ResultLog 'ERROR: bl2 missing' -Encoding ascii; exit 1 }
if (-not (Test-Path $MainPath)) { Write-Host "缺少 $MainPath, 请先执行 tools\build.ps1" -ForegroundColor Red; Set-Content $ResultLog 'ERROR: main missing' -Encoding ascii; exit 1 }
StepLog 'input files ok'

$bl2raw = [System.IO.File]::ReadAllBytes($Bl2RawPath)
if ($bl2raw.Length -gt 14332) { Write-Host "BL2 固件超过 14K-4 限制!" -ForegroundColor Red; Set-Content $ResultLog 'ERROR: bl2 too big' -Encoding ascii; exit 1 }

$main = [System.IO.File]::ReadAllBytes($MainPath)
if ($main.Length -gt 524288) { Write-Host "主程序超过 512KB 限制!" -ForegroundColor Red; Set-Content $ResultLog 'ERROR: main too big' -Encoding ascii; exit 1 }

# ---------- 3. 生成 BL2 (14K 代码 + 4 字节校验和) ----------
$bl2 = New-Object byte[] 14336
for ($i = 0; $i -lt 14336; $i++) { $bl2[$i] = 0xFF }
[System.Array]::Copy($bl2raw, 0, $bl2, 0, $bl2raw.Length)
$sum = 0L
for ($i = 0; $i -lt 14332; $i++) { $sum += $bl2[$i] }
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$sum), 0, $bl2, 14332, 4)
[System.IO.File]::WriteAllBytes($Bl2Path, $bl2)
StepLog 'bl2 generated'
Write-Host ("BL2 生成: " + $bl2.Length + " 字节, 校验和 0x{0:X8}" -f $sum)

# ---------- 3b. 主程序填充到 512KB (0xFF), 从扇区 49 开始 ----------
$mainPad = New-Object byte[] 524288
for ($i = 0; $i -lt 524288; $i++) { $mainPad[$i] = 0xFF }
[System.Array]::Copy($main, 0, $mainPad, 0, $main.Length)
Write-Host ("主程序: " + $main.Length + " 字节, 填充到 " + $mainPad.Length + " 字节")
StepLog 'main padded'

# ---------- 4. 原始扇区写入 + 读回校验 (FileStream 直接打开物理盘) ----------
$bl1 = [System.IO.File]::ReadAllBytes($Bl1Path)

$fs = [System.IO.File]::Open($dev, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
StepLog 'device opened for write'
try {
    # BL1: 扇区 1~16 (偏移 512)
    $fs.Seek(512, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fs.Write($bl1, 0, $bl1.Length)
    $fs.Flush($true)

    # BL2: 扇区 17~44 (偏移 8704)
    $fs.Seek(8704, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fs.Write($bl2, 0, $bl2.Length)
    $fs.Flush($true)

    # 主程序: 扇区 49~1072 (偏移 25088), 共 512KB
    $fs.Seek(25088, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fs.Write($mainPad, 0, $mainPad.Length)
    $fs.Flush($true)
    Write-Host '写入完成, 正在读回校验...'
    StepLog 'write done'
} finally {
    $fs.Close()
}

$fs2 = [System.IO.File]::Open($dev, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
StepLog 'device reopened for readback'
try {
    $r1 = New-Object byte[] $bl1.Length
    $r2 = New-Object byte[] $bl2.Length
    $rm = New-Object byte[] $mainPad.Length
    $fs2.Seek(512, [System.IO.SeekOrigin]::Begin) | Out-Null
    $n1 = $fs2.Read($r1, 0, $r1.Length)
    $fs2.Seek(8704, [System.IO.SeekOrigin]::Begin) | Out-Null
    $n2 = $fs2.Read($r2, 0, $r2.Length)
    $fs2.Seek(25088, [System.IO.SeekOrigin]::Begin) | Out-Null
    $nm = $fs2.Read($rm, 0, $rm.Length)

    $m1 = ($n1 -eq $r1.Length)
    $m2 = ($n2 -eq $r2.Length)
    $mm = ($nm -eq $rm.Length)
    for ($i = 0; $m1 -and $i -lt $r1.Length; $i++) { if ($r1[$i] -ne $bl1[$i]) { $m1 = $false } }
    for ($i = 0; $m2 -and $i -lt $r2.Length; $i++) { if ($r2[$i] -ne $bl2[$i]) { $m2 = $false } }
    for ($i = 0; $mm -and $i -lt $rm.Length; $i++) { if ($rm[$i] -ne $mainPad[$i]) { $mm = $false } }

    Write-Host ("BL1_match=" + $m1 + " BL2_match=" + $m2 + " MAIN_match=" + $mm)
    StepLog ("readback bl1=" + $m1 + " bl2=" + $m2 + " main=" + $mm)
    if ($m1 -and $m2 -and $mm) {
        Write-Host 'BURN VERIFY OK - 烧录成功! 把 SD 卡插到开发板, 拨码开关设为 SD 启动后上电。' -ForegroundColor Green
        Set-Content -Path $ResultLog -Value 'BURN_OK' -Encoding ascii
    } else {
        Write-Host 'BURN VERIFY FAILED!' -ForegroundColor Red
        Set-Content -Path $ResultLog -Value 'BURN_VERIFY_FAILED' -Encoding ascii
        exit 1
    }
} finally {
    $fs2.Close()
}
