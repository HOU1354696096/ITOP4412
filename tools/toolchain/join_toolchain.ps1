<#
    join_toolchain.ps1 - 从 git 仓库内置分片恢复并解压交叉编译器

    背景:
        xPack arm-none-eabi-gcc 15.2.1-1.1 安装包约 320MB, 超过 GitHub
        单文件 100MB 限制, 因此切成了 5 个分片放在 tools\toolchain\parts\
        (随 git 仓库一起提交, 链接永不过期)。本脚本把分片合并回 zip,
        校验 SHA256 后解压到 tools\toolchain\。

    用法:
        powershell -ExecutionPolicy Bypass -File tools\toolchain\join_toolchain.ps1

    产物:
        tools\toolchain\xpack-arm-none-eabi-gcc-15.2.1-1.1\bin\arm-none-eabi-gcc.exe
        tools\toolchain\xpack-windows-build-tools-4.4.1-3\bin\make.exe

    注意:
        - 若 parts 目录缺失, 说明克隆不完整, 请改用 download_toolchain.ps1 在线下载。
        - 解压后 tools\toolchain\ 下约 1.4GB, 这些目录被 .gitignore 排除, 不入库。
#>

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$TcDir  = $PSScriptRoot
$Parts  = Join-Path $TcDir 'parts'
$DlDir  = Join-Path $TcDir 'downloads'
New-Item -ItemType Directory -Force -Path $DlDir | Out-Null

# 官方安装包的 SHA256 (用于校验分片合并结果, 防止下载/克隆损坏)
$Expected = @{
    'xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip' =
        'BAE6A3D1667697CE750C3B13D6D26D80973ECEDC2CC87BF04869E83447FD93EA'
    'xpack-windows-build-tools-4.4.1-3-win32-x64.zip' =
        '113D4DFDBBC56DC9B865C9F75D38CD0DA82F0D7094187E6F7A803FE6EEF1D218'
}

function Get-Zip {
    param([string]$ZipName)
    $out = Join-Path $DlDir $ZipName
    $partFiles = Get-ChildItem $Parts -Filter ($ZipName + '.part*') -ErrorAction SilentlyContinue |
        Sort-Object Name
    if ($partFiles) {
        Write-Host "==> 合并 $ZipName 分片 ($($partFiles.Count) 个) ..." -ForegroundColor Cyan
        $fs = [System.IO.File]::Create($out)
        foreach ($f in $partFiles) {
            $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
            $fs.Write($bytes, 0, $bytes.Length)
        }
        $fs.Close()
        return $out
    }
    # 没有分片时: parts 里若直接放着单文件 (如 make 安装包) 就复制过来
    $single = Join-Path $Parts $ZipName
    if (Test-Path $single) {
        Write-Host "==> 复制 $ZipName (无需分片) ..." -ForegroundColor Cyan
        Copy-Item $single $out -Force
        return $out
    }
    return $null
}

foreach ($name in $Expected.Keys) {
    $zip = Join-Path $DlDir $name
    $outDir = $TcDir
    if ($name -like 'xpack-arm-none-eabi-gcc*') {
        $checkExe = Join-Path $outDir 'xpack-arm-none-eabi-gcc-15.2.1-1.1\bin\arm-none-eabi-gcc.exe'
    } else {
        $checkExe = Join-Path $outDir 'xpack-windows-build-tools-4.4.1-3\bin\make.exe'
    }

    # 已解压过就直接跳过
    if (Test-Path $checkExe) {
        Write-Host "已存在: $checkExe , 跳过。" -ForegroundColor Green
        continue
    }

    $joined = Get-Zip $name
    if ($joined -and (Test-Path $joined)) {
        $h = (Get-FileHash $joined -Algorithm SHA256).Hash
        if ($h -ne $Expected[$name]) {
            Write-Host "分片校验失败! 期望 $($Expected[$name])" -ForegroundColor Red
            Write-Host "实际 $h" -ForegroundColor Red
            Write-Host '分片可能损坏, 请重新克隆仓库, 或改用 download_toolchain.ps1 在线下载。'
            exit 1
        }
        Write-Host "SHA256 校验通过: $h"
    } else {
        # 仓库里没有分片 → 在线下载兜底
        $urls = @{
            'xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip' =
                'https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip'
            'xpack-windows-build-tools-4.4.1-3-win32-x64.zip' =
                'https://github.com/xpack-dev-tools/windows-build-tools-xpack/releases/download/v4.4.1-3/xpack-windows-build-tools-4.4.1-3-win32-x64.zip'
        }
        Write-Host "未找到 $name 分片, 尝试在线下载 (备用通道) ..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $urls[$name] -OutFile $zip -UseBasicParsing -TimeoutSec 600
    }

    Write-Host "==> 解压 $name ..."
    Expand-Archive -Path $zip -DestinationPath $outDir -Force
    Remove-Item $zip -Force
}

# ---------- 最终校验 ----------
$gccExe  = Join-Path $TcDir 'xpack-arm-none-eabi-gcc-15.2.1-1.1\bin\arm-none-eabi-gcc.exe'
$makeExe = Join-Path $TcDir 'xpack-windows-build-tools-4.4.1-3\bin\make.exe'
if ((Test-Path $gccExe) -and (Test-Path $makeExe)) {
    Write-Host ''
    Write-Host '工具链就绪!' -ForegroundColor Green
    Write-Host "  gcc : $gccExe"
    Write-Host "  make: $makeExe"
    Write-Host ''
    Write-Host '下一步编译:  powershell -ExecutionPolicy Bypass -File tools\build.ps1' -ForegroundColor Green
} else {
    Write-Host '工具链恢复失败, 请检查后重试。' -ForegroundColor Red
    exit 1
}
