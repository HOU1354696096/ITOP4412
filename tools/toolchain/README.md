# tools/toolchain - 交叉编译工具链

本目录放工程编译用的 **arm-none-eabi 交叉编译器**（绿色版，不装系统）。

## 目录内容

| 目录/文件 | 说明 |
| ---- | ---- |
| `download_toolchain.ps1` | 一键下载并解压工具链（已提交到 git） |
| `xpack-arm-none-eabi-gcc-15.2.1-1.1/` | xPack GCC 15.2.1 交叉编译器（体积大，**不提交 git**） |
| `xpack-windows-build-tools-4.4.1-3/` | Windows 版 make / rm 等（体积小，**不提交 git**） |

## 使用方法

第一次使用（或新克隆仓库后）执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\toolchain\download_toolchain.ps1
```

脚本从 xPack 官方 GitHub Release 下载约 1.4GB 并解压到本目录。
之后 `tools\build.ps1` 会自动找到工具链，**无需修改任何路径**。

## 为什么不同时提交到 git

解压后的工具链约 1.4GB，GitHub 等平台限制单文件/仓库大小，无法入库。
所以 git 仓库里只保存下载脚本，任何人克隆后运行一次脚本即可得到完全相同的版本。
