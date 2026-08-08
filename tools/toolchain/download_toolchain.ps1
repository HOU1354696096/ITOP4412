<#
    download_toolchain.ps1 - 一键下载并解压 arm-none-eabi 交叉编译器

    这个脚本会把本工程编译所需的全部工具链下载到工程内的
    tools\toolchain\ 目录 (绿色版, 不污染系统 PATH), 内容包括:

      1. xPack arm-none-eabi-gcc 15.2.1-1.1   (交叉编译器 gcc/ld/objcopy/...)
      2. xPack Windows Build Tools 4.4.1-3    (make / rm 等, 供 Makefile 使用)

    下载完成后, tools\build.ps1 会自动在 tools\toolchain\ 下找到工具链,
    不需要手动修改任何脚本路径。

    用法:
        powershell -ExecutionPolicy Bypass -File tools\toolchain\download_toolchain.ps1

    下载源 (xPack 官方 GitHub Release, 与工程测试版本完全一致):
        https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip
        https://github.com/xpack-dev-tools/windows-build-tools-xpack/releases/download/v4.4.1-3/xpack-windows-build-tools-4.4.1-3-win32-x64.zip

    ⚠ 注意: 下载解压后 tools\toolchain\ 下会有约 1.4GB 文件,
    它们体积太大不会提交到 git, 换电脑/新克隆仓库后请重新运行本脚本。
#>

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$TcDir  = $PSScriptRoot
$DlDir  = Join-Path $TcDir 'downloads'
New-Item -ItemType Directory -Force -Path $DlDir | Out-Null

$Jobs = @(
    @{
        Name = 'xpack-arm-none-eabi-gcc-15.2.1-1.1'
        Url  = 'https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip'
    },
    @{
        Name = 'xpack-windows-build-tools-4.4.1-3'
        Url  = 'https://github.com/xpack-dev-tools/windows-build-tools-xpack/releases/download/v4.4.1-3/xpack-windows-build-tools-4.4.1-3-win32-x64.zip'
    }
)

foreach ($j in $Jobs) {
    $zip = Join-Path $DlDir ($j.Name + '.zip')
    $out = Join-Path $TcDir $j.Name
    if (Test-Path (Join-Path $out 'bin\arm-none-eabi-gcc.exe') -or
        Test-Path (Join-Path $out 'bin\make.exe')) {
        Write-Host "已存在: $out , 跳过下载。" -ForegroundColor Green
        continue
    }

    Write-Host "==> 下载 $($j.Name) ..." -ForegroundColor Cyan
    Write-Host "    $($j.Url)"
    if (-not (Test-Path $zip)) {
        Invoke-WebRequest -Uri $j.Url -OutFile $zip -UseBasicParsing -TimeoutSec 600
    } else {
        Write-Host "    安装包已存在, 直接解压: $zip"
    }

    Write-Host "==> 解压到 $TcDir ..."
    Expand-Archive -Path $zip -DestinationPath $TcDir -Force
    Remove-Item $zip -Force
}

# ---------- 最终校验 ----------
$gccExe = Join-Path $TcDir 'xpack-arm-none-eabi-gcc-15.2.1-1.1\bin\arm-none-eabi-gcc.exe'
$makeExe = Join-Path $TcDir 'xpack-windows-build-tools-4.4.1-3\bin\make.exe'
if ((Test-Path $gccExe) -and (Test-Path $makeExe)) {
    Write-Host ''
    Write-Host '工具链就绪!' -ForegroundColor Green
    Write-Host "  gcc : $gccExe"
    Write-Host "  make: $makeExe"
    Write-Host ''
    Write-Host '下一步编译:  powershell -ExecutionPolicy Bypass -File tools\build.ps1' -ForegroundColor Green
} else {
    Write-Host '工具链校验失败, 请检查网络后重试。' -ForegroundColor Red
    exit 1
}
