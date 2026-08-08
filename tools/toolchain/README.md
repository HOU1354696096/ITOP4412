# tools/toolchain - 交叉编译工具链

本目录放工程编译用的 **arm-none-eabi 交叉编译器**（绿色版，不装系统）。
工具链安装包（约 320MB）已**分片打包进 git 仓库**（`parts/` 子目录），
克隆仓库即可离线恢复，不依赖任何网络链接。

## 目录内容

| 目录/文件 | 说明 |
| ---- | ---- |
| `parts/` | 工具链安装包分片 + make 安装包（**已提交 git**，链接永不过期） |
| `join_toolchain.ps1` | **首选**：把分片合并、校验 SHA256、解压到本目录（离线） |
| `download_toolchain.ps1` | 备用：官方链接在线下载（分片缺失/损坏时用） |
| `xpack-arm-none-eabi-gcc-15.2.1-1.1/` | 解压产物：xPack GCC 15.2.1 交叉编译器（体积大，**不提交 git**） |
| `xpack-windows-build-tools-4.4.1-3/` | 解压产物：Windows 版 make / rm 等（**不提交 git**） |

## parts/ 分片说明

GitHub 等平台限制单文件不超过 100MB，而 gcc 安装包有 320MB，
因此切成 5 个分片（每个 ≤76.3MB）随仓库提交：

| 文件 | 大小 | 说明 |
| ---- | ---- | ---- |
| `xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip.part01~05` | 320MB 合计 | gcc 交叉编译器安装包 |
| `xpack-windows-build-tools-4.4.1-3-win32-x64.zip` | 2.6MB | make / rm 等（无需分片） |

`join_toolchain.ps1` 会按顺序合并并校验 SHA256，防止克隆损坏。

## 使用方法

第一次使用（或新克隆仓库后）执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\toolchain\join_toolchain.ps1
```

脚本从仓库内 `parts/` 分片合并解压（约 1.4GB），全程离线。
之后 `tools\build.ps1` 会自动找到工具链，**无需修改任何路径**。

如果 `parts/` 分片缺失（例如旧版本克隆），可改用在线下载：

```powershell
powershell -ExecutionPolicy Bypass -File tools\toolchain\download_toolchain.ps1
```

## 为什么不直接提交整个工具链

解压后的工具链约 1.4GB（10 万+ 文件），远超任何 git 平台的合理仓库体积；
所以仓库保存的是**官方安装包的分片**（320MB），合并解压后与在线下载
得到的版本完全一致（SHA256 校验保证），既满足"链接永不过期"，
又不让仓库膨胀到无法克隆。
