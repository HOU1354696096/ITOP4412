# -*- coding: utf-8 -*-
"""
gen_tutorial.py - 生成完整教学文档 docs/裸机教程.md

结构: 每一章 = "详细讲解(含逐句注释) + 该模块的完整源码文件"。
完整代码从工程实时读取, 保证与代码一致; 讲解文字为手工编写。

用法:
    python tools/gen_tutorial.py
"""
import os
import io
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "裸机教程.md")
OUT_CSDN = os.path.join(ROOT, "docs", "裸机教程-CSDN导入版.md")


def read(p):
    with io.open(os.path.join(ROOT, p), "r", encoding="utf-8") as f:
        return f.read()


def lang_of(p):
    return {
        ".c": "c", ".h": "c", ".s": "asm", ".lds": "ld", ".ps1": "powershell",
    }.get(os.path.splitext(p)[1].lower(), "")


def full(p, title=None):
    """返回: '#### 完整文件 xxx' + 代码块。

    注意: 嵌入内容里可能含有三重反引号(如 gen_tutorial.py 自身):
    - 用普通围栏会被内容里的反引号提前闭合, 后续内容变成真实标题;
    - 用 4 反引号长围栏在 GitHub/Gitee 可用, 但 CSDN 等编辑器不识别;
    因此这种情况统一改用 4 空格缩进代码块(全平台兼容)。
    层级说明: 完整文件是各模块章节(##)下的从属内容, 用 #### 与
    编号小节(### 9.1~9.4)区分开, 避免在目录/大纲里平级并列。
    CSDN 兼容: 嵌入内容里行首 "# 注释" 在 CSDN 导入时会被误判为标题
    (ATX 标题要求 # 后跟空格), 因此统一去掉空格写成 "#注释"。
    对 PowerShell/Makefile 语义完全不变, 复制出来仍可编译。
    """
    t = title or p
    body = read(p).rstrip("\n")
    # CSDN 会把行首 "# + 空格" 当成标题, 去掉 # 后所有空格即可避免
    # (PowerShell/Makefile 中 "#  注释" 与 "#注释" 语义完全相同)
    body_safe = re.sub(r"(?m)^# +", "#", body)
    lang = lang_of(p)
    if "```" in body_safe:
        indented = "\n".join("    " + line for line in body_safe.split("\n"))
        return "#### 完整文件：%s\n\n%s\n\n" % (t, indented)
    return "#### 完整文件：%s\n\n```%s\n%s\n```\n" % (t, lang, body_safe)


def csdn_variant(text):
    """把标准 Markdown 转成 CSDN 导入专用版。

    CSDN 导入对 ``` 围栏支持不稳定(围栏一旦丢失, 行首 # 注释还会被
    误判成标题), 因此本版本把全部围栏代码块改写成 4 空格缩进式代码块,
    不依赖任何围栏; 行首 "#注释" 保留无空格写法, 双保险。
    同时处理引用块内的小段代码(如 23.4 的示例)去掉围栏标记。
    """
    banner = (
        "> **CSDN 导入专用版**\n"
        "> 本文件由 `tools/gen_tutorial.py` 自动生成, 专为 CSDN 导入优化:\n"
        "> 1. 所有代码块都是 4 空格缩进式, 不含三反引号围栏(CSDN 对围栏支持不稳定);\n"
        "> 2. 代码内行首注释统一写成 `#注释`(无空格), 不会被误判成标题;\n"
        "> 3. 导入方法: 打开 CSDN 编辑器并切换到 **Markdown 模式**, 先全选删除\n"
        ">    旧文章内容(CSDN 是追加式导入, 不清空会和新内容叠加导致重复), 再把\n"
        ">    本文件的**源文件内容**整篇粘贴进去(不要复制渲染后的网页/预览);\n"
        ">    图片/视频需自行上传 CSDN 图床; 完整可编译版本见 docs/裸机教程.md。\n"
        "\n"
        "---\n"
        "\n"
    )
    lines = text.split("\n")
    out = []
    fence = False      # 顶层 ``` 围栏
    bq_fence = False   # 引用块内的 ``` 围栏
    for line in lines:
        if bq_fence:
            if line.startswith("> ```"):
                bq_fence = False
                continue
            if line.startswith(">"):
                out.append("> " + line[2:] if len(line) > 2 else ">")
            else:
                out.append("> " + line)
            continue
        if line.startswith("> ```"):
            bq_fence = True
            continue
        if fence:
            if line.strip() == "```":
                fence = False
                continue
            out.append("    " + line)
            continue
        if line.strip().startswith("```"):
            fence = True
            continue
        out.append(line)
    return banner + "\n".join(out)


parts = []
add = parts.append

add("""# Exynos4412 裸机开发完整教程（STM32 库风格）

> 目标板：**iTOP-4412 精英版（POP 封装，LPDDR2 1G）**
> 工程：本仓库 `exynos4412`，仿 STM32 标准外设库编写
>
> 本文每一章 = **详细讲解（含每一句关键代码的作用） + 该模块完整源码**。
> 完整源码由 `tools/gen_tutorial.py` 从工程实时读取，保证与代码 100% 一致；
> 照着本文即可复刻整个工程。

## 本文框架

全文分 **8 个部分**，按“先看懂 → 再动手 → 后排错”的顺序组织：

| 部分 | 讲什么 |
| ---- | ---- |
| **第一部分 预备知识** | 启动流程、工具下载、目录结构——先建立整体概念 |
| **第二部分 启动与链接** | start.S / main_start.S / 链接脚本——程序怎么跑起来 |
| **第三部分 外设库详解** | GPIO/UART/DDR/LCD/蜂鸣器/按键/时钟——每个模块逐句讲解 + 完整源码 |
| **第四部分 应用层** | main.c 逐段、User/App 各模块——业务逻辑怎么组织 |
| **第五部分 显示与构建** | 8x16 字模、Windows 编译（build.ps1 / Makefile） |
| **第六部分 烧录与上板** | 确认 SD 磁盘号、烧录 SD 卡、上电验证 |
| **第七部分 速查与排错** | 关键项速查表、完整五步流程、常见问题 |
| **第八部分 附录** | 构建与烧录脚本完整源码 |

---
""")

add("""## 运行效果演示（视频 + 截图）

以下为本工程在 **iTOP-4412（POP 封装）** 开发板上的实际运行效果。
视频约 4MB，GitHub / Gitee 页面可直接播放（无法播放时点击下载）。

![运行效果截图（屏幕分块状态面板）](media/运行效果.jpg)

<video src="media/运行效果.mp4" controls width="720"></video>

> 视频内容：上电 → BL2 初始化时钟与 DDR → 主程序搬运到 DDR `0x43E00000` →
> LCD 显示分块状态面板（UART2/UART3 收发计数与 LOG、LED、蜂鸣器、按键、
> 系统状态）→ 蜂鸣器播放内置歌曲，VOL± 切歌、SLEEP 停止、BACK/HOME 控制 LED。

---
""")

add("""## 目录

**第一部分 预备知识**
- 1. [总体架构与启动流程](#1-总体架构与启动流程)
- 2. [需要的工具与下载地址](#2-需要的工具与下载地址)
- 3. [工程目录结构](#3-工程目录结构)

**第二部分 启动与链接**
- 4. [启动文件 start.S 逐句讲解 + 完整源码](#4-启动文件-starts-逐句讲解--完整源码)
- 5. [主程序启动文件 main_start.S + 完整源码](#5-主程序启动文件-main_starts--完整源码)
- 6. [链接脚本 .lds 讲解 + 完整源码](#6-链接脚本-lds-讲解--完整源码)

**第三部分 外设库详解**
- 7. [GPIO 驱动讲解 + 完整源码](#7-gpio-驱动讲解--完整源码)
- 8. [UART 驱动讲解 + 完整源码](#8-uart-驱动讲解--完整源码)
- 9. [DDR 驱动详解 + 完整源码（重点）](#9-ddr-驱动详解--完整源码重点)
- 10. [LCD 驱动详解 + 完整源码（重点）](#10-lcd-驱动详解--完整源码重点)
- 11. [蜂鸣器与按键驱动 + 完整源码（含音乐数组）](#11-蜂鸣器与按键驱动--完整源码含音乐数组)
- 12. [时钟与毫秒节拍 + 完整源码](#12-时钟与毫秒节拍--完整源码)

**第四部分 应用层**
- 13. [主程序 main.c 逐段讲解 + 完整源码](#13-主程序-mainc-逐段讲解--完整源码)
- 14. [应用模块 User/App + 完整源码](#14-应用模块-userapp--完整源码)

**第五部分 显示与构建**
- 15. [8x16 字模表 font8x16.h](#15-8x16-字模表-font8x16h)
- 16. [Windows 下编译（build.ps1 / Makefile）](#16-windows-下编译buildps1--makefile)

**第六部分 烧录与上板**
- 17. [如何确认 SD 卡磁盘号（详细步骤）](#17-如何确认-sd-卡磁盘号详细步骤)
- 18. [Windows 下烧录 SD 卡](#18-windows-下烧录-sd-卡)
- 19. [上电验证](#19-上电验证)

**第七部分 速查与排错**
- 20. [关键项速查表](#20-关键项速查表)
- 21. [完整操作流程（编译→烧录→上电）](#21-完整操作流程编译烧录上电)
- 22. [常见问题](#22-常见问题)

**第八部分 附录**
- 23. [构建与烧录脚本完整源码](#23-附录构建与烧录脚本完整源码)

---
""")

# ============ 1 架构 ============
add("""# 第一部分：预备知识

> **本部分讲什么**：先建立整体概念——芯片怎么启动、需要哪些工具、工程文件怎么
> 组织。看完这部分，你对“BL1/BL2/main 三个镜像”和“哪些文件干什么”就有数了。

---

""")
add("""## 1. 总体架构与启动流程

Exynos4412 上电后的启动链路：**iROM(BL1) → BL2(IRAM) → 主程序(DDR)**。

```text
上电
 │
 ├─ 芯片内部 iROM 固化代码（三星出厂写好，用户不可改）
 │    ├─ 初始化时钟/DDR 的"最小部分"
 │    └─ 从 SD 卡扇区 1 读 8KB BL1 到 IRAM 0x02020000 并运行
 │
 ├─ BL1（tools/bl1/E4412.S.BL1.SSCR.EVT1.1.bin，迅为/三星提供）
 │    └─ 从 SD 卡扇区 17 读 14KB BL2 到 IRAM 0x02023400 并运行
 │
 ├─ BL2（本工程编译，运行在 IRAM，受 14KB 限制）
 │    ├─ 关闭看门狗、配置异常模式
 │    ├─ SystemInit()：DDR_Init() → System_ClockInit() → DDR_DllStartPost()
 │    └─ 用 iROM 拷贝函数把 SD 扇区 49 起的 512KB 主程序搬到 DDR 0x43E00000
 │
 └─ 主程序 main.bin（运行在 DDR）
      ├─ UART2/UART3 初始化
      ├─ LVDS-LCD 初始化（800x1280）
      ├─ 分栏状态面板（左半 UART2/UART3，右半 KEY/LED/BUZZER/SYSTEM）
      ├─ 蜂鸣器循环播放 3 首歌（生日快乐/小星星/两只老虎）
      ├─ 5 个按键：VOL± 切歌、SLEEP 停止、BACK/HOME 控制 LED
      └─ UART3 打印歌曲名等系统信息 + 收到字符回显
```

**为什么要分 BL2 和 main 两个镜像？**

- BL2 只能放在 IRAM（BL1 加载到 `0x02023400`，实际限制 **14KB**），
  放不下 LCD 驱动、字库、状态面板这些大代码；
- 所以 BL2 只做“初始化时钟 + DDR + 搬运”，把真正的大程序搬到 1GB 的 DDR
  （`0x43E00000`）里运行，空间随便用。

> **关键项 1**：BL2 必须小于 14332 字节（14KB − 4 字节校验和），否则 BL1 加载失败。
> **关键项 2**：DDR 初始化必须在切换高速时钟之前完成（先 `DDR_Init()` 再
> `System_ClockInit()`），与官方 U-Boot 流程一致。

---
""")

# ============ 2 工具 ============
add("""## 2. 需要的工具与下载地址

| 工具 | 用途 | 下载地址 |
| ---- | ---- | ---- |
| **arm-none-eabi 交叉编译器** | 把 C/汇编编译成 ARM 裸机程序 | xPack：<https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases>（选 `win-x64.zip`）；或 Arm 官方：<https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads> |
| **串口助手** | 查看 UART2 输出、给 UART3 发字符 | SSCOM：<https://www.daxia.com/download/sscom.rar>；XCOM；PuTTY：<https://www.putty.org> |
| **USB 转串口驱动** | PL2303/CH340 驱动 | 搜"PL2303 官方驱动"/"CH340 驱动" |
| **SD 卡 + 读卡器** | 存放启动镜像 | 普通 SD/microSD 卡 |
| **管理员权限** | 烧录脚本直接写物理磁盘 | Windows UAC |

**工具链安装步骤（仓库已内置分片，离线恢复，链接永不过期）：**

1. 在工程根目录打开 PowerShell，执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\\toolchain\\join_toolchain.ps1
```

2. 脚本把 `tools\\toolchain\\parts\\` 里的安装包分片（320MB，随 git 仓库
   提交）合并、校验 SHA256 并解压到 `tools\\toolchain\\`（约 1.4GB，全程离线）；
3. 之后直接编译即可，`tools\\build.ps1` 会自动在 `tools\\toolchain\\` 下
   找到工具链（找不到再回退到系统 PATH），**无需修改任何脚本路径**。

> 备用：如果 `parts\\` 分片缺失（旧版本克隆），才需要在线下载：
> `powershell -ExecutionPolicy Bypass -File tools\\toolchain\\download_toolchain.ps1`

> **关键项 3**：工具链请放在 `tools\\toolchain\\` 下（或加入 PATH）。
> ⚠ 绝对路径警告：**不要**像旧教程那样把工具链写死成
> `C:\\Users\\xxx\\...\\bin`——工程拷贝到其他目录/其他电脑后，绝对路径必然失效。
> 本工程所有脚本均以工程目录为基准的相对路径定位工具。

---
""")

# ============ 3 目录 ============
add("""## 3. 工程目录结构

```text
exynos4412/
├── Libraries/
│   ├── inc/                     # 外设库头文件
│   └── src/                     # 外设库源码
│       ├── exynos4412_gpio.c    # GPIO
│       ├── exynos4412_uart.c    # UART
│       ├── exynos4412_ddr.c     # LPDDR2
│       ├── exynos4412_clock.c   # 时钟 + 毫秒节拍
│       ├── exynos4412_lcd.c     # FIMD + LVDS
│       ├── exynos4412_buzzer.c  # PWM0 蜂鸣器 + 3 首歌
│       └── exynos4412_key.c     # 5 按键
├── User/
│   ├── main.c                   # 主程序
│   └── App/                     # 功能模块
│       ├── led.c/h              # LED
│       ├── panel.c/h            # 状态面板
│       ├── debug.c/h            # 调试打印
│       └── system_4412.c        # SystemInit
├── startup/
│   ├── start.S                  # BL2 启动
│   ├── main_start.S             # 主程序启动
│   ├── aeabi_div.S              # 软件除法
│   ├── exynos4412.lds           # BL2 链接脚本
│   └── main.lds                 # 主程序链接脚本
├── tools/
│   ├── build.ps1                # Windows 一键编译
│   ├── burn_sd.ps1              # Windows 烧录 SD 卡
│   ├── list_disks.ps1           # 查看磁盘号
│   ├── verify_sd.ps1            # 读回校验 SD
│   ├── gen_tutorial.py          # 生成本文档的脚本
│   ├── toolchain/               # 交叉编译器
│   │   ├── parts/               # 安装包分片（已入库，链接不过期）
│   │   ├── join_toolchain.ps1   # 首选：分片合并解压（离线）
│   │   └── download_toolchain.ps1 # 备用：在线下载
│   ├── teraterm/                # 串口终端
│   ├── pl2303_v150/             # PL2303 串口驱动
│   ├── bl1/                     # 三星 BL1
│   │   └── E4412.S.BL1.SSCR.EVT1.1.bin
│   └── other/                   # 参考资料（体积大，不提交 git）
├── Makefile
├── build/                       # 编译输出（自动生成）
└── README.md
```

---
""")

# ============ 4 start.S ============
add("""# 第二部分：启动与链接

> **本部分讲什么**：程序是怎么跑起来的——BL2 启动文件（start.S）、主程序入口
> （main_start.S）、链接脚本（.lds）和软件除法（aeabi_div.S）。这是整个工程的
> “地基”，先看懂它，后面外设库才有运行环境。

---

""")
add("""## 4. 启动文件 start.S 逐句讲解 + 完整源码

BL2 被 BL1 加载到 IRAM `0x02023400` 后从 `_start` 执行。核心步骤：

1. **关看门狗**：不关的话系统每隔几秒自动复位，串口只看到零星字节；
2. **进 SVC 模式、关中断**：裸机不需要中断打扰；
3. **关 MMU/D-Cache/I-Cache**：没配页表前不能开 MMU；
4. **设栈、清 BSS**：C 语言运行环境；
5. **SystemInit()**：`DDR_Init()` → `System_ClockInit()` → `DDR_DllStartPost()`；
6. **iROM 拷贝**：把 SD 扇区 49 起的 512KB 主程序搬到 DDR `0x43E00000`；
7. **跳转 DDR** 运行 main。

关键点说明：

- `0x02020030` 存着 iROM 提供的 `copy_sd_mmc_to_mem` 函数指针（BL1 启动后有效），
  参数是（起始扇区，块数，目标地址），每块 512 字节；
- BL2 头部 4 个字 `0x2000 0 0 0` 与迅为已验证程序一致，不能省；
- 搬运后打印主程序首 4 字节，用于校验 SD 读取是否正确。

""" + full("startup/start.S"))

add("""---
""")

# ============ 5 main_start.S ============
add("""## 5. 主程序启动文件 main_start.S + 完整源码

主程序被搬到 DDR 后从这里进入：

1. **关对齐检查（A=0）**：面板字符串拼接会产生未对齐的半字存储，
   开着对齐检查会触发数据访问异常（`FAULT D, DFSR=0x801`）；
2. **设异常向量表**（VBAR）：诊断用，异常时打印 DFSR/DFAR/PC；
3. **设栈（DDR 顶部 0x7FFFE000）、清 BSS**；
4. **跳 main()**。

""" + full("startup/main_start.S"))

add("""---
""")

# ============ 6 lds ============
add("""## 6. 链接脚本 .lds 讲解 + 完整源码

链接脚本决定代码/数据放在内存的哪个地址。本工程有两个：

### 6.1 exynos4412.lds（BL2）

- 基址 `0x02023400`（IRAM），由编译脚本通过 `--defsym __LINK_BASE` 传入；
- `KEEP(*(.text.entry))` 保证 `_start` 在镜像最前；
- 导出 `__bss_start/__bss_end` 供 start.S 清 BSS；
- `__stack_top = 0x02060000`（IRAM 顶部）。

### 6.2 main.lds（主程序）

- 基址固定 `0x43E00000`（DDR），必须与 start.S 的搬运目标一致；
- `__stack_top = 0x7FFFE000`（1GB DDR 顶端留 8KB 给栈）；
- 帧缓冲在 `0x50000000`，与代码/栈互不冲突。

""" + full("startup/exynos4412.lds") + "\n" + full("startup/main.lds"))

add("""
### 6.3 startup/aeabi_div.S —— 软件除法

裸机没有 libgcc，用汇编实现 64/32 位除法，供 C 代码里的 `/` `%` 运算符使用。

""" + full("startup/aeabi_div.S"))

add("""
> **关键项 5**：链接基址（0x43E00000）、搬运目标（start.S 扇区49）、
> 烧录布局（burn_sd.ps1）三处必须一致。

---
""")

# ============ 7 GPIO ============
add("""# 第三部分：外设库详解

> **本部分讲什么**：芯片外设库（Libraries/）每个模块的完整源码和逐句讲解——
> GPIO、UART、DDR、LCD、蜂鸣器、按键、时钟。每个模块 = 讲解 + 完整源码。
> 这一部分是全文最长的部分，也是“照着复刻”的核心。

---

""")
add("""## 7. GPIO 驱动讲解 + 完整源码

`GPIO_Init()` 是核心：每组 GPIO 有 `CON`（每引脚 4 位：0=输入 1=输出 2~15=复用）、
`PUD`（每引脚 2 位：00=无 01=下拉 11=上拉）、`DRV`（驱动强度）三个配置寄存器。
函数先读回再按位修改，**只影响指定的引脚**，不会破坏同组其他引脚（比如背光 GPD0_1）。

4412 GPIO 基址速查（`exynos4412_gpio.h`）：

| 控制器 | 基地址 | 组 |
| ------ | ------ | -- |
| 0 | `0x11400000` | GPA0、GPA1、GPB、GPC0/1、GPD0/1、GPF0~3、GPJ0/1 |
| 1 | `0x11000000` | GPK0~3、GPL0~2、GPM0~4、GPY0~6、GPX0~3 |
| 2 | `0x03860000` | GPZ |
| 3 | `0x106E0000` | GPV0~4 |

本工程用到的引脚：UART2=GPA1_0/1、UART3=GPA1_4/5、LCD=GPF0~3/GPL0/GPL1、
蜂鸣器=GPD0_0、LED=GPL2_0/GPK1_1、按键=GPX1/GPX2/GPX3。

""" + full("Libraries/src/exynos4412_gpio.c") + "\n" + full("Libraries/inc/exynos4412_gpio.h"))

add("""---
""")

# ============ 8 UART ============
add("""## 8. UART 驱动讲解 + 完整源码

`UART_Init()` 做四件事：开外设时钟 → 配帧格式（ULCON）→ 开 FIFO → 算波特率
（`UBRDIV = UART时钟/(波特率x16) − 1`，100MHz/115200/16−1=53）。

`UART_SendData()` 是**盲写 + 固定延时**：写完 `UTXH` 后 `System_Delay(0x9000)`
约 185us，比 115200 线路速率（每字节 87us）慢一倍多，防止 PL2303 仿冒串口线丢字节。

> **关键项 6**：改 CPU 主频后，`System_Delay(0x9000)` 的实际时长会变，
> 若发现串口丢字节，需要同步调整这个延时。

""" + full("Libraries/src/exynos4412_uart.c") + "\n" + full("Libraries/inc/exynos4412_uart.h"))

add("""---
""")

# ============ 9 DDR ============
add("""## 9. DDR 驱动详解 + 完整源码（重点）

### 9.1 为什么 DDR 初始化最难

Exynos4412（POP 封装）内封装了 LPDDR2 颗粒，上电后 DMC（内存控制器）完全是"裸"的：
没有时序、没有 ZQ 校准、DLL 没启动，直接读写就是死机。必须按官方序列一步步配好。

> **关键项 7**：本序列只适用 **POP 1G LPDDR2**；DDR3 板卡时序完全不同。

### 9.2 初始化顺序

```c
void SystemInit(void)
{
#ifdef EXYNOS4412_BOOT_SD
    DDR_Init();            /* 1. 先初始化 DDR (低速时钟下) */
    System_ClockInit();    /* 2. 再切高速时钟 APLL/MPLL=800MHz */
    DDR_DllStartPost();    /* 3. 时钟稳定后 DLL 收尾 */
#endif
}
```

### 9.3 核心步骤解读

`DDR_DmcInit()` 逐条解读：

- `PHYZQCONTROL=0xE3855403`：ZQ 校准，LPDDR2 上电必需；
- `PHYCONTROL0/1` 抖动序列：启动 PHY DLL（**不能写 SHGATE**）；
- `CONCONTROL=0x0FFF30CA`：先**关闭自动刷新**（初始化期间不能刷新）；
- `MEMCONFIG0=0x40C01323`：片选基址 0x40000000、容量 1G；
- `IVCONTROL=0x8000001D`：DMC0/DMC1 之间 128 字节交织，拼成连续 1G；
- `TIMINGREF/ROW/DATA/POWER`：AC 时序（DMC 400MHz）；
- `DIRECTCMD` 6 条：NOP（拉高 CKE）→ 5 条 MRS/MRW 写模式寄存器，**一条不能少**；
- `CONCONTROL=0x0FFF303A`：打开 DREX（自动刷新使能）。

> **关键项 8**：初始化期间**不要等待 PHYSTATUS**，官方序列就是固定延时；
> 等待会卡死。

### 9.4 DDR 读写 API

```c
DDR_WriteByte(0x40000000, 0x31);              /* 写 1 字节 */
DDR_WriteWord(0x40001000, 0x12345678);        /* 写 1 个字 */
DDR_WriteBuffer(0x40000000, buf, 6);          /* 写一段 */
DDR_ReadBuffer(0x40000000, buf, 6);           /* 读一段 */
DDR_MemSet(0x40000000, 0xFF, 1024);           /* 填充 */
```

这些 API 就是 volatile 指针的内存读写，DDR 初始化成功后直接可用。

""" + full("Libraries/src/exynos4412_ddr.c") + "\n" + full("Libraries/inc/exynos4412_ddr.h"))

add("""---
""")

# ============ 10 LCD ============
add("""## 10. LCD 驱动详解 + 完整源码（重点）

### 10.1 屏幕与引脚

7.0 寸 IPS 电容屏，LVDS 接口，有效扫描 800x1280，通过 GM8285C 桥接芯片把
FIMD 的 RGB 信号转成 LVDS。

| 功能 | 引脚 | 说明 |
| ---- | ---- | ---- |
| 背光电源使能 | GPL0_4 | 输出高电平 |
| LVDS 桥使能 | GPL1_0 | 输出高电平（SHTDN=高才工作） |
| 触摸电源使能 | GPL0_2 | 输出高电平 |
| 背光 PWM | GPD0_1 = TOUT_1 | PWM1，约 10kHz，88% 占空比 |
| RGB 数据 | GPF0~GPF3 | 复用功能 2 |
| 帧缓冲 | DDR 0x50000000 | 800x1280x4 字节 ≈ 4MB |

### 10.2 初始化流程解读

1. `LCD_PowerOn()`：背光电源 + LVDS 桥使能 + 背光 PWM1（TCON bit8~11）；
2. `LCD_ClockInit()`：FIMD0 时钟源=SCLK_MPLL(800MHz)，分频 2 → SCLK_FIMD=400MHz；
3. `LCD_GpioInit()`：GPF0~GPF3 复用功能 2，驱动强度 LV4；
4. FIMD 寄存器：
   - `VIDCON0` CLKVAL=5 → **VCLK = 400/(5+1) = 66.7MHz**；
   - `VIDTCON0/1/2`：垂直/水平时序 + 分辨率（800x1280）；
   - `WINCON0`：24bpp；`VIDOSD0B`：窗口右下角；
   - `VIDW00ADD0B0`：帧缓冲地址 0x50000000；
   - `WINSHMAP`：使能窗口通道；最后 `ENVID|ENVID_F` 开显示。

刷新率验证：行总=800+24+72+4=900，帧总行=1280+10+12+2=1304，
刷新率=66.7MHz/(900x1304)≈**56.8Hz**（接近 60Hz，正常不闪）。

> **关键项 9**：背光 PWM1 用 TCON **bit8~11**，蜂鸣器 PWM0 用 **bit0~3**，
> 毫秒节拍 PWM2 用 **bit12~15**，三个通道共享 TCFG0/TCFG1/TCON，
> 但各自只改自己的位域，**绝不能整寄存器覆盖**。
>
> **关键项 10**：屏幕不亮先查三件事：① GPL0_4/GPL1_0 是否输出高；
> ② 背光 PWM1 是否工作（TCON bit8=1）；③ `CLK_SRC_CPU` bit24 是否置 1
> （否则 MOUTMPLL_USER=24MHz，整条 LCD 时钟链只有 1.5MHz，屏闪一下即黑）。

### 10.3 绘图函数

- `LCD_DrawPixel(x,y,color)`：写显存 `fb[y*800+x]`；
- `LCD_FillRect`：两层循环写实心矩形；
- `LCD_DrawChar_Scaled`：按 8x16 字模逐点画，每点放大 scale 倍；
- `LCD_ShowString_Scaled`：逐字符显示，字符宽 8*scale。

""" + full("Libraries/src/exynos4412_lcd.c") + "\n" + full("Libraries/inc/exynos4412_lcd.h"))

add("""---
""")

# ============ 11 蜂鸣器+按键 ============
add("""## 11. 蜂鸣器与按键驱动 + 完整源码（含音乐数组）

### 11.1 蜂鸣器（PWM0）

无源压电蜂鸣器由 PWM0（GPD0_0=TOUT_0）驱动，**改变 PWM 频率 = 改变音高**。

- PWM 计数时钟：ACLK_100=100MHz，预分频 50 → 2MHz；
- `TCNTB0 = 2000000/频率`，`TCMPB0 = TCNTB0/2`（50% 占空比）；
- TCON 通道 0：bit0=启动、bit1=手动装载、bit3=自动重载；
- **休止时把引脚拉成 GPIO 低电平**（驱动管 Q5 截止），保证绝对静音；
- 切歌：先静音 300ms 再开始新歌（`Buzzer_PlaySongByIndex` 的延时切换机制）；
- 内置 3 首歌：**生日快乐 / 小星星 / 两只老虎**，完整音符数组（频率+时长）
  见下方 `exynos4412_buzzer.c` 全文，VOL± 上下曲循环切换。

### 11.2 按键（5 键，消抖）

引脚（与内核 mach-itop4412.c 一致，全部 active_low）：

| 按键 | 引脚 | 功能 |
| ---- | ---- | ---- |
| VOL- | GPX2(0) | 上一首 |
| VOL+ | GPX2(1) | 下一首 |
| SLEEP | GPX3(3) | 停止 |
| BACK | GPX1(2) | LED 闪 |
| HOME | GPX1(1) | LED 停 |

消抖：每 10ms 扫描一次，连续 2 次一致才认可状态变化；只在“松开→按下”边沿
返回按键号，避免长按连发和机械抖动误触发。

> **关键项 12**：按键初始化开**上拉**（GPIO_PuPd_UP），按下为低电平。

""" + full("Libraries/src/exynos4412_buzzer.c") + "\n" + full("Libraries/inc/exynos4412_buzzer.h") + "\n" + full("Libraries/src/exynos4412_key.c") + "\n" + full("Libraries/inc/exynos4412_key.h"))

add("""---
""")

# ============ 12 时钟 ============
add("""## 12. 时钟与毫秒节拍 + 完整源码

### 12.1 时钟链要点

- APLL=800MHz（ARM 主频；比迅为 U-Boot 的 1000MHz 更稳，裸机不依赖 PMIC 1.3V）；
- MPLL=800MHz，**CLK_SRC_CPU bit24 必须置 1**（MOUTMPLL_USER 选 FOUTMPLL），
  否则 ACLK_100=3MHz，串口/背光/屏幕全部异常；
- ACLK_100=100MHz（PWM 定时器 PCLK，DIV_TOP=0x01315474）；
- SCLK_UART = MPLL/8 = 100MHz（DIV_PERIL0=0x777777）。

### 12.2 毫秒节拍（PWM2）

- TCFG0 预分频1=99 → 1MHz，TCNTB2=1000000 → 每秒回绕一次；
- `System_GetMs()` 用回绕安全减法累计毫秒，供蜂鸣器/LED/面板计时；
- 主循环里有“节拍看门狗”：原始计数器长时间不变就重新初始化节拍。

""" + full("Libraries/src/exynos4412_clock.c") + "\n" + full("Libraries/inc/exynos4412_clock.h") + "\n" + full("Libraries/inc/exynos4412.h"))

add("""---
""")

# ============ 13 main.c ============
add("""# 第四部分：应用层

> **本部分讲什么**：业务逻辑怎么组织——主程序 main.c 逐段讲解，以及 User/App
> 下的功能模块（LED、状态面板、调试打印、系统初始化）。外设库是“积木”，
> 这一部分是“怎么搭积木”。

---

""")
add("""## 13. 主程序 main.c 逐段讲解 + 完整源码

1. **UART 参数结构体**：115200 8N1，SCLK_UART=100MHz；
2. **UART2/UART3 初始化**：GPIO 复用 + UART_Init；
3. **LCD_Init()**：屏幕点亮；
4. **System_TickInit()**：毫秒节拍；
5. **外设初始化**：Buzzer_Init / LED_Init / Key_Init / Panel_Init；
6. **播放第 1 首**（切歌 300ms 延时，非阻塞）；
7. **主循环**：
   - 节拍看门狗；
   - UART2 回显（屏蔽 TX 通知，回显不进 TX LOG）+ 记 RX LOG；
   - UART3 回显（计入 UART3 TX LOG）；
   - 按键扫描：VOL- 上一首 / VOL+ 下一首 / SLEEP 停止 / BACK LED 闪 / HOME LED 停；
   - LED_BlinkTick / Buzzer_Tick / Panel_Update 非阻塞调度；
   - UART2 每秒心跳（含 U2/U3 RX 计数）。

""" + full("User/main.c"))

add("""---
""")

# ============ 14 App ============
add("""## 14. 应用模块 User/App + 完整源码

| 文件 | 作用 |
| ---- | ---- |
| `led.c/h` | LED1=GPL2_0、LED2=GPK1_1，非阻塞 500ms 交替闪烁；`LED_BlinkEnable/Disable` 由 BACK/HOME 控制 |
| `panel.c/h` | 800x1280 分栏状态面板；**只重画变化行**（防闪烁）；左半 UART2/UART3、右半 KEY/LED/BUZZER/SYSTEM |
| `debug.c/h` | `PrintHex32/8/16`、`LCD_PrintDebugRegs` 等调试打印 |
| `system_4412.c` | `SystemInit()`：`DDR_Init()` → `System_ClockInit()` → `DDR_DllStartPost()` |

""" + full("User/App/system_4412.c") + "\n" + full("User/App/led.h") + "\n" + full("User/App/led.c") + "\n" + full("User/App/panel.h") + "\n" + full("User/App/panel.c") + "\n" + full("User/App/debug.h") + "\n" + full("User/App/debug.c"))

add("""---
""")

# ============ 15 字模 ============
add("""# 第五部分：显示与构建

> **本部分讲什么**：屏幕字模数据（font8x16.h）和 Windows 下怎么编译
> （build.ps1 / Makefile）。编译产物 bl2_14k.bin / main.bin 是烧录用文件。

---

""")
add("""## 15. 8x16 字模表 font8x16.h

8x16 点阵 ASCII 字模，覆盖 0x20~0x7E 共 95 个字符，每字符 16 字节
（每字节 bit7 为最左列，1 表示前景色）。数据来自 U-Boot 的 `include/video_font.h`。
字模范围之外的字符在显示时会被替换成 `?`。

""" + full("Libraries/inc/exynos4412_font8x16.h"))

add("""---
""")

# ============ 16 编译 ============
add("""## 16. Windows 下编译（build.ps1 / Makefile）

### 方式一：build.ps1（推荐 Windows）

1. 确保工具链就绪：运行 `tools\\toolchain\\join_toolchain.ps1`
   （或把 arm-none-eabi-gcc 所在目录加入 PATH）；
2. 在工程根目录打开 PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File tools\\build.ps1
```

3. 产物在 `build/`：

| 文件 | 说明 |
| ---- | ---- |
| `bl2.bin` | BL2（IRAM 0x02023400，时钟+DDR+搬运） |
| `bl2_14k.bin` | BL2 + 4 字节校验和（14336 字节，烧录用） |
| `main.bin` | 主程序（DDR 0x43E00000） |

`bl2_14k.bin` 的生成规则：前 14332 字节固件（不足补 0xFF）+ 末 4 字节累加校验和
（小端）。BL1 会校验这个和，不对就不启动。

### 方式二：Makefile（Linux/WSL 或 Windows + make）

```bash
make            # 编译 BL2 + 主程序
make clean      # 清空 build/
```

> **关键项 13**：BL2 链接基址通过 `--defsym __LINK_BASE=0x02023400` 传入
> `exynos4412.lds`；主程序基址直接写在 `main.lds`（0x43E00000）。

---
""")

# ============ 17 磁盘号 ============
add("""# 第六部分：烧录与上板

> **本部分讲什么**：把编译好的镜像烧到 SD 卡并上板验证——确认 SD 磁盘号、
> 烧录命令、上电后的预期输出。这部分照着做就能跑起来。

---

""")
add("""## 17. 如何确认 SD 卡磁盘号（详细步骤）

烧录脚本 `burn_sd.ps1 -Disk N` 的 N 必须是 SD 卡在电脑里的磁盘号，选错会写坏其他盘。

**方法一：磁盘管理（图形界面，最简单）**

1. 把 SD 卡插进读卡器，插到电脑 USB；
2. 按 `Win + R`，输入 `diskmgmt.msc` 回车；
3. 看磁盘列表，找“**可移动**”且容量与 SD 卡一致的那块；
4. 磁盘号就是“磁盘 N”里的 N（如“磁盘 1”）。

**方法二：diskpart（命令行）**

```text
diskpart
list disk
```

容量小、类型为可移动的那块就是 SD 卡。

**方法三：PowerShell（管理员）**

```powershell
Get-Disk | Format-Table Number, FriendlyName, Size, BusType
```

烧录前脚本会先读目标盘 MBR 并打印，请再核对一次磁盘号，**别选成系统盘（磁盘 0）**。

---
""")

# ============ 18 烧录 ============
add("""## 18. Windows 下烧录 SD 卡

```powershell
#在工程根目录执行 (会弹 UAC, 点"是"; -Disk 换成你的磁盘号)
powershell -ExecutionPolicy Bypass -File tools\\burn_sd.ps1 -Disk 1
```

SD 卡布局：

| 扇区 | 内容 |
| ---- | ---- |
| 0 | 保留（MBR，不动） |
| 1~16 | 三星 BL1（`tools/bl1/E4412.S.BL1.SSCR.EVT1.1.bin`） |
| 17~48 | BL2（`build/bl2_14k.bin`） |
| 49~1072 | 主程序（`build/main.bin`，填充到 512KB） |

烧录脚本做的事：

1. 校验管理员权限；
2. 直接打开物理盘 `\\\\.\\PhysicalDriveN`；
3. 依次写入 BL1 / BL2 / main；
4. 读回全部数据逐字节比对，全部一致才输出 `BURN_OK`。

结果写在 `%TEMP%\\burn_sd_result.log`：成功为 `BURN_OK`，并显示
`BL1_match=True BL2_match=True MAIN_match=True`。

> ⚠ **绝对路径提醒**：`%TEMP%` 是 Windows 系统变量，指向你电脑的
> **用户临时目录**（形如 `C:\\Users\\你的用户名\\AppData\\Local\\Temp\\`），
> 不是工程目录。不同电脑这个路径不同，属正常现象，用资源管理器地址栏输入
> `%TEMP%` 即可直接打开。

---
""")

# ============ 19 上电 ============
add("""## 19. 上电验证

1. SD 卡插回开发板；拨码开关拨到 **SD 启动**（丝印一般标 SD/eMMC，详见使用手册）；
2. 串口接 UART2（DB9 调试口），115200 8N1；
3. 上电应看到：

```text
BL2: clocks+DDR ok
BL2: main copied to 0x43E00000, head=...
======== UART2 DEBUG PORT ========
Main running from DDR @ 0x43E00000
LCD: OK
...
Buzzer: playing HAPPY BIRTHDAY (GPD0_0, PWM tone)
alive | U2 rx=00000000 U3 rx=00000000 | BZ 01/1D f=0310
```

4. 屏幕显示分栏面板；按 5 个按键验证切歌/LED；UART3 打印系统信息。

---
""")

# ============ 20 关键项 ============
add("""# 第七部分：速查与排错

> **本部分讲什么**：15 条关键项速查表、完整的五步操作流程、常见问题排查。
> 遇到问题时先查这一部分。

---

""")
add("""## 20. 关键项速查表

| # | 关键项 | 说明 |
| -- | ---- | ---- |
| 1 | BL2 < 14332 字节 | 超过则 BL1 加载失败 |
| 2 | 先 DDR 后时钟 | `DDR_Init()` 必须在 `System_ClockInit()` 之前 |
| 3 | 工具链路径 | 放 `tools/toolchain/`（或加入 PATH），`build.ps1` 自动查找；**别写绝对路径** |
| 4 | 三处一致 | start.S 扇区49/目标0x43E00000 ↔ burn_sd.ps1 布局 ↔ main.lds 基址 |
| 5 | DDR 布局 | 代码 0x43E00000 / 显存 0x50000000 / 栈 0x7FFFE000 互不冲突 |
| 6 | 串口延时 | `UART_SendData` 约 185us/字节，防仿冒线丢字节 |
| 7 | DDR 序列 | 只适用 POP 1G LPDDR2；DDR3 板卡需换时序 |
| 8 | 不等 PHYSTATUS | DDR 初始化用固定延时，DIRECTCMD 6 条不能少 |
| 9 | PWM 位域 | 背光 bit8~11、蜂鸣器 bit0~3、节拍 bit12~15，各改各的 |
| 10 | 屏不亮三查 | GPL0_4/GPL1_0 高电平、背光 PWM1、CLK_SRC_CPU bit24 |
| 11 | 面板防闪 | 只重画变化行，不整屏清空 |
| 12 | 按键上拉 | 按下为低，消抖 20ms |
| 13 | 链接基址 | BL2=0x02023400（--defsym），main=0x43E00000（lds 内） |
| 14 | 磁盘号 | `-Disk N` 的 N 必须是 SD 卡磁盘号，别选系统盘 |
| 15 | 校验和 | bl2_14k.bin 末 4 字节 = 前 14332 字节累加和 |

---
""")

# ============ 21 流程 ============
add("""## 21. 完整操作流程（编译→烧录→上电）

**第一步 准备**：装工具链（运行 `tools\\toolchain\\download_toolchain.ps1`）、
装串口驱动、准备 SD 卡。

**第二步 编译**：

```powershell
powershell -ExecutionPolicy Bypass -File tools\\build.ps1
```

确认 `build/bl2_14k.bin` 和 `build/main.bin` 已生成，BL2 < 14332 字节。

**第三步 确认磁盘号**：`Win+R` → `diskmgmt.msc`，记下 SD 卡磁盘号 N。

**第四步 烧录**：

```powershell
powershell -ExecutionPolicy Bypass -File tools\\burn_sd.ps1 -Disk 1
```

查看 `%TEMP%\\burn_sd_result.log` 为 `BURN_OK`
（⚠ `%TEMP%` 是系统临时目录变量，每台电脑路径不同，见第 18 章说明）。

**第五步 上电**：SD 卡插回开发板 → 拨码 SD 启动 → 串口 UART2 115200 8N1 →
上电看 `BL2: clocks+DDR ok` → 屏幕面板 → 按键验证。

---
""")

# ============ 22 FAQ ============
add("""## 22. 常见问题

**串口没输出 / 乱码**：查波特率 115200 8N1、串口号、PL2303/CH340 驱动、
`UART_Clock=100MHz`、发送延时 185us/字节。

**SD 模式跑不起来**：查 BL2 < 14332 字节、BL1 在扇区 1、BL2 在扇区 17、
拨码 SD 启动、BL1 加载地址是否 0x02023400。

**初始化蜂鸣器一直响 / 切歌衔接尖锐**：初始化时 GPD0_0 保持 GPIO 低电平；
休止时引脚强制拉低；切歌有 300ms 延时。

**屏幕不亮 / 闪一下即黑**：查 GPL0_4/GPL1_0 高电平、背光 PWM1（TCON bit8）、
`CLK_SRC_CPU` bit24=1。

**编译找不到头文件**：`-I` 需包含 `Libraries/inc`、`User`、`User/App`。

**烧录报"找不到文件 '\\\\.\\PhysicalDriveN'"**：SD 卡没插好或磁盘号不对，
按第 17 章重新确认。

**导入 CSDN 后代码注释变成标题 / 代码围栏丢失**：教程文件是标准 Markdown，
请在 CSDN 编辑器 **Markdown 模式**下直接粘贴 `docs/裸机教程.md` 的**源文件内容**，
不要复制渲染后的网页/预览（HTML 粘贴会拆散代码块）；图片和视频需自行上传 CSDN 图床。
**导入前务必先清空旧文章内容**（全选删除）：CSDN 编辑器是追加式导入，不清空会把
旧内容和新内容叠在一起，表现为文末章节重复出现。
嵌入代码中注释统一写成 `#注释`（`#` 后无空格），是为避免 CSDN 把注释误判成标题；
复制出来仍是合法注释，不影响编译。**若围栏仍被 CSDN 破坏**，请改用
`docs/裸机教程-CSDN导入版.md`：该版本所有代码块均为 4 空格缩进式、不含
三反引号围栏，专为 CSDN 导入生成，复制到 GitHub/Gitee 也能正常阅读。

---
""")

# ============ 23 附录 ============
add("""# 第八部分：附录

> **本部分讲什么**：构建与烧录脚本的完整源码（Makefile、build.ps1、
> burn_sd.ps1）。生成这份文档的脚本 tools/gen_tutorial.py 因自身包含
> markdown 代码块与标题（内嵌会在 CSDN 等平台破坏文档结构），
> 不再全文粘贴，源码见仓库 tools/gen_tutorial.py。

---

""")
add("""## 23. 附录：构建与烧录脚本完整源码

### 23.1 Makefile

""" + full("Makefile"))

add("""### 23.2 tools/build.ps1

""" + full("tools/build.ps1"))

add("""### 23.3 tools/burn_sd.ps1

""" + full("tools/burn_sd.ps1"))

add("""### 23.4 tools/gen_tutorial.py（生成本文档的脚本）

> 本脚本从工程源码实时读取并生成这份教程，执行方式：
>
> ```powershell
> python tools\\gen_tutorial.py
> ```
>
> 脚本内容本身包含 markdown 代码块和标题，若全文内嵌，在 GitHub/Gitee
> 会被围栏截断、在 CSDN 会因缩进代码块不识别而泄漏成真实标题
> （早期版本曾因此出现文末内容错乱），因此不在本文重复粘贴。
> 完整源码见仓库：`tools/gen_tutorial.py`。
""")

add("""---

# 附：源码与更新（git 地址）

本工程源码归档在以下 git 仓库（内容同步，任选其一克隆）：

| 平台 | 克隆地址 |
| ---- | ---- |
| GitHub | `git clone https://github.com/HOU1354696096/ITOP4412.git` |
| Gitee | `git clone https://gitee.com/hou_banchao/ITOP4412_POP_7inLCD.git` |

克隆后第一步安装工具链（仓库自带分片，全程离线，不依赖网络链接）：

```powershell
powershell -ExecutionPolicy Bypass -File tools\\toolchain\\join_toolchain.ps1
```

然后编译、烧录、上板，详见本文第 16~21 章。
""")

with io.open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(parts))

with io.open(OUT_CSDN, "w", encoding="utf-8") as f:
    f.write(csdn_variant("\n".join(parts)))

print("tutorial written:", OUT)
print("csdn variant written:", OUT_CSDN)
