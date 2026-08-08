<#
    verify_sd.ps1 - 读回 SD 卡并对比 build 目录镜像, 确认烧录内容
    用法: powershell -ExecutionPolicy Bypass -File tools\verify_sd.ps1 -Disk 1
    需要管理员权限。
#>
param([int]$Disk = 1)

$ErrorActionPreference = 'Stop'
$log = Join-Path $PSScriptRoot 'verify_sd.log'
Remove-Item $log -ErrorAction SilentlyContinue
function Log($m) { Add-Content -Path $log -Value $m -Encoding UTF8; Write-Host $m }

$Root = Split-Path $PSScriptRoot -Parent
$dev = '\\.\PhysicalDrive' + $Disk

$fs = [System.IO.File]::Open($dev, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
try {
    # 磁盘大小
    $len = $fs.Length
    Log ("磁盘大小: " + [math]::Round($len / 1GB, 2) + " GB")

    # MBR
    $mbr = New-Object byte[] 512
    $fs.Read($mbr, 0, 512) | Out-Null
    $sig = ($mbr[510] -eq 0x55) -and ($mbr[511] -eq 0xAA)
    $p1start = [System.BitConverter]::ToUInt32($mbr, 0x1BE + 8)
    $p1type = $mbr[0x1BE + 4]
    Log ("MBR有效=" + $sig + " 分区1起始=" + $p1start + " 类型=" + ('{0:X2}' -f $p1type))

    # BL1: 扇区 1~16
    $bl1 = New-Object byte[] 8192
    $fs.Seek(512, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fs.Read($bl1, 0, 8192) | Out-Null
    $bl1ref = [System.IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'bl1\E4412.S.BL1.SSCR.EVT1.1.bin'))
    $m1 = $true
    for ($i = 0; $i -lt 8192; $i++) { if ($bl1[$i] -ne $bl1ref[$i]) { $m1 = $false; break } }
    Log ("BL1(扇区1-16) 匹配=" + $m1)

    # BL2: 扇区 17~44 (14336 字节)
    $bl2 = New-Object byte[] 14336
    $fs.Seek(8704, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fs.Read($bl2, 0, 14336) | Out-Null
    $bl2ref = [System.IO.File]::ReadAllBytes((Join-Path $Root 'build\bl2_14k.bin'))
    $m2 = $true
    for ($i = 0; $i -lt 14336; $i++) { if ($bl2[$i] -ne $bl2ref[$i]) { $m2 = $false; break } }
    Log ("BL2(扇区17-44) 匹配=" + $m2)
    Log ("  BL2 卡上前4字节: " + (($bl2[0..3] | ForEach-Object { $_.ToString('X2') }) -join ' '))
    Log ("  BL2 卡上校验和:  " + (($bl2[14332..14335] | ForEach-Object { $_.ToString('X2') }) -join ' '))

    # 主程序头部: 扇区 49~52 (前 2048 字节)
    $mainHead = New-Object byte[] 2048
    $fs.Seek(25088, [System.IO.SeekOrigin]::Begin) | Out-Null
    $fs.Read($mainHead, 0, 2048) | Out-Null
    $mainRef = [System.IO.File]::ReadAllBytes((Join-Path $Root 'build\main.bin'))
    $mm = $true
    for ($i = 0; $i -lt 2048; $i++) { if ($mainHead[$i] -ne $mainRef[$i]) { $mm = $false; break } }
    Log ("MAIN头(扇区49起) 匹配=" + $mm)
    Log ("  MAIN 卡上前4字节: " + (($mainHead[0..3] | ForEach-Object { $_.ToString('X2') }) -join ' '))
    Log ("  MAIN 应上4字节:   " + (($mainRef[0..3] | ForEach-Object { $_.ToString('X2') }) -join ' '))
} finally {
    $fs.Close()
}
Log "=== 完成 ==="
