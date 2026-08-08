# Exynos4412 裸机库工程（STM32 库风格）

面向 **Exynos4412（iTOP-4412 精英版 / POP 封装，LPDDR2）** 开发板的裸机外设库，
仿照 STM32 标准外设库的组织方式编写：BL2 先初始化时钟 + DDR，把主程序从 SD 卡
搬运到 DDR（`0x43E00000`）运行，屏幕、串口、蜂鸣器、按键全部在 DDR 中执行。

> 📖 **新手教学文档**：见 [docs/裸机教程.md](docs/裸机教程.md) —— 含每一句关键
> 代码的作用、Windows 编译/烧录步骤、工具下载地址、Makefile 与链接脚本讲解。
> 导入 CSDN 请用 [docs/裸机教程-CSDN导入版.md](docs/裸机教程-CSDN导入版.md)
> （所有代码块均为缩进式、不含三反引号围栏，专为 CSDN 兼容生成）。

## 外设库（Libraries/）

| 模块 | 文件 | 说明 |
| ---- | ---- | ---- |
| GPIO | `exynos4412_gpio.c/h` | 全 42 组 GPIO，输入/输出/复用功能、上下拉、驱动强度 |
| UART | `exynos4412_uart.c/h` | UART0~3，轮询收发、自动计算波特率分频、TX LOG 弱回调 |
| DDR | `exynos4412_ddr.c/h` | DMC0/DMC1 控制器，LPDDR2（POP）初始化 + 读写 API |
| 时钟 | `exynos4412_clock.c/h` | APLL/MPLL 800MHz、SCLK_UART=100MHz、毫秒节拍（PWM2） |
| LCD | `exynos4412_lcd.c/h` | FIMD + LVDS（7.0 寸 IPS 800x1280），绘图原语 + 8x16 字库 |
| 蜂鸣器 | `exynos4412_buzzer.c/h` | 无源压电蜂鸣器，PWM0(TOUT_0/GPD0_0) 变调，内置 3 首歌 |
| 按键 | `exynos4412_key.c/h` | 底板 5 按键（GPX1/GPX2/GPX3），20ms 消抖、边沿触发 |

## 应用模块（User/App/，功能模块文件夹）

| 模块 | 文件 | 说明 |
| ---- | ---- | ---- |
| 主程序 | `User/main.c` | 模块调度：串口/屏幕/蜂鸣器/按键/LED |
| LED | `User/App/led.c/h` | 底板 LED（GPL2_0/GPK1_1），非阻塞交替闪烁，可开关 |
| 状态面板 | `User/App/panel.c/h` | 屏幕分块状态面板（增量重绘，不闪烁） |
| 调试打印 | `User/App/debug.c/h` | 寄存器十六进制打印，排查背光/串口用 |
| 系统初始化 | `User/App/system_4412.c` | `SystemInit()`，对应 STM32 的 system 文件 |

## 主程序功能

1. UART2（GPA1_0/GPA1_1 调试口）+ UART3（GPA1_4/GPA1_5，底板 CON2 "串口1"），115200 8N1；
2. LVDS-LCD 初始化（7.0 寸 IPS 800x1280），分栏状态面板：
   - 左半屏：上半 **UART2**、下半 **UART3**（各自收发计数 + RX/TX LOG 各 3 行）；
   - 右半屏从上到下：**KEY / LED / BUZZER / SYSTEM** 状态块；
3. 蜂鸣器内置 3 首歌循环播放（生日快乐 / 小星星 / 两只老虎）；
4. 5 个按键控制：

| 按键 | 引脚 | 功能 |
| ---- | ---- | ---- |
| VOL- | GPX2(0) | 上一首蜂鸣器歌曲 |
| VOL+ | GPX2(1) | 下一首蜂鸣器歌曲 |
| SLEEP | GPX3(3) | 停止播放 |
| BACK | GPX1(2) | LED 开始闪烁 |
| HOME | GPX1(1) | LED 停止闪烁并熄灭 |

5. UART3（CON2）打印系统信息：按键事件 `KEY: VOL+`、切歌 `SONG: TWINKLE STAR`、
   停止 `SONG: STOP`、LED 开关 `LED: BLINK ON/OFF`；UART3 收到字符立即回显。

## 目录结构

```text
├── Libraries/
│   ├── inc/            # 外设库头文件
│   └── src/            # 外设库源码
├── User/
│   ├── main.c          # 主程序（模块调度）
│   └── App/            # 功能模块文件夹
│       ├── led.c/h     # LED 应用模块
│       ├── panel.c/h   # 屏幕状态面板模块
│       ├── debug.c/h   # 寄存器调试打印模块
│       └── system_4412.c # 系统初始化
├── startup/
│   ├── start.S         # BL2 启动文件（关对齐检查、异常向量）
│   ├── main_start.S    # 主程序启动文件（异常向量表、故障打印）
│   ├── aeabi_div.S     # 软件除法（替代 libgcc）
│   ├── exynos4412.lds  # BL2 链接脚本（IRAM 0x02023400）
│   └── main.lds        # 主程序链接脚本（DDR 0x43E00000）
├── tools/
│   ├── build.ps1       # 一键编译（BL2 + 主程序）
│   ├── burn_sd.ps1     # 烧录 SD 卡（需要管理员权限）
│   ├── list_disks.ps1  # 查看电脑磁盘号
│   ├── verify_sd.ps1   # 读回校验 SD 卡
│   ├── gen_tutorial.py # 生成 docs/裸机教程.md 的脚本
│   ├── check_tutorial.py # 校验教程文档结构 + 嵌入源码一致性
│   ├── toolchain/      # 交叉编译器（parts/ 分片已入库，join_toolchain.ps1 恢复）
│   ├── teraterm/       # 串口终端
│   ├── pl2303_v150/    # PL2303 串口驱动
│   ├── bl1/            # 三星 BL1 启动镜像
│   └── other/          # 参考资料（体积大，不提交 git）
└── README.md
```

## 编译

需要 `arm-none-eabi` 交叉编译器（本工程使用 xPack arm-none-eabi-gcc 15.2.1）。
工具链安装包（约 320MB）已**分片打包进 git 仓库**，克隆后离线恢复即可：

```powershell
powershell -ExecutionPolicy Bypass -File tools\toolchain\join_toolchain.ps1
```

`join_toolchain.ps1` 会把 `tools/toolchain/parts/` 里的分片合并、校验 SHA256
并解压到 `tools/toolchain/`（约 1.4GB，全程离线，不依赖任何网络链接）。
之后 `tools/build.ps1` 会自动找到工具链，
**不需要手动配置任何路径**（找不到时回退到系统 PATH）。

> 备用：如果 `parts/` 分片缺失（旧版本克隆），才需要在线下载
> `tools\toolchain\download_toolchain.ps1`。

> ⚠ 路径规范：本工程所有脚本都以**工程目录为基准的相对路径**定位文件，
> 换目录、换电脑都不会失效。**请不要把工具链或文件路径写死成
> `C:\Users\xxx\...` 这类绝对路径**——那是本工程最早版本踩过的坑。

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

产物在 `build/` 下：

| 文件 | 说明 |
| ---- | ---- |
| `bl2.bin` | BL2 镜像，运行在 IRAM 0x02023400（时钟+DDR+搬运主程序） |
| `bl2_14k.bin` | BL2 + 4 字节校验和（14336 字节，烧录用） |
| `main.bin` | 主程序镜像，运行在 DDR 0x43E00000 |

## 烧录 SD 卡

把 SD 卡插到电脑读卡器，确认磁盘号（一般 USB 读卡器为 1），然后以管理员方式执行
（会弹出 UAC 确认框）：

```powershell
powershell -ExecutionPolicy Bypass -File tools\burn_sd.ps1 -Disk 1
```

SD 卡布局（与三星 E4412 / 迅为 iTOP-4412 标准一致）：

| 扇区 | 内容 |
| ---- | ---- |
| 0 | 保留（MBR，不动） |
| 1~16 | 三星 BL1（`tools/bl1/E4412.S.BL1.SSCR.EVT1.1.bin`） |
| 17~48 | BL2（14KB 代码 + 4 字节校验和） |
| 49~1072 | 主程序（填充到 512KB），由 BL2 搬运到 DDR 0x43E00000 |

烧录成功日志会写入 `%TEMP%\burn_sd_result.log`（`BURN_OK`），并把 SD 卡插回开发板、
拨码开关设为 SD 启动后上电。

> ⚠ **唯一必须了解的"绝对路径"**：`%TEMP%` 是 Windows 系统变量，指向
> **你自己的用户临时目录**（每台电脑不同，形如
> `C:\Users\你的用户名\AppData\Local\Temp\`）。在资源管理器地址栏输入 `%TEMP%`
> 即可直达，这不是工程目录，属正常现象。

## 路径规范（重要，读一遍避免踩坑）

本项目遵循 **相对路径优先** 原则，任何人都可以把整个文件夹放到任意位置使用：

1. 编译/烧录/校验脚本（`tools\*.ps1`）全部通过 `$PSScriptRoot` / `Split-Path`
   相对定位，不依赖任何 `C:\Users\...` 路径；
2. 工具链固定在工程内 `tools\toolchain\`，由
   `tools\toolchain\download_toolchain.ps1` 下载解压，移动工程后依然可用；
3. 文档里的路径示例一律写成 `tools\...`、`build\...` 这类相对路径；
4. 必须使用系统路径的地方只有两处，请牢记：
   - `%TEMP%\burn_sd_result.log` —— 烧录结果日志（系统临时目录）；
   - `\\.\PhysicalDriveN` —— 烧录目标物理盘（N 是 SD 卡磁盘号，选错会写坏别的盘）。

## git 归档与下载

本仓库已按上述结构归档到 git。**提交进仓库的内容**：全部源码、教程、
`tools/` 下的脚本与必需工具（串口终端、驱动、BL1），体积可控、人人可克隆。
**不提交的内容**（体积太大或仅本地有用）：

| 内容 | 位置 | 原因 |
| ---- | ---- | ---- |
| 交叉工具链安装包 | `tools/toolchain/parts/`（约 320MB，**已入库**） | 超 100MB 单文件限制，切成 ≤76.3MB 分片提交 |
| 工具链解压产物（约 1.4GB） | `tools/toolchain/xpack-*/` | 体积过大不入库，由 `join_toolchain.ps1` 恢复 |
| 迅为 U-Boot/内核/原理图/手册（约 4.2GB） | `tools/other/` | 参考用，见 `tools/other/README.md` |
| 开发过程日志/脚本 | `tools/other/dev/` | 仅作者排错用 |

克隆仓库后执行两条命令即可编译：

```powershell
powershell -ExecutionPolicy Bypass -File tools\toolchain\join_toolchain.ps1
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

## 关键寄存器资料（已按 4412 手册核实）

### GPIO

4412 的 GPIO 与 4210 不同，**没有 GPE/GPG/GPH 组**：

| 控制器 | 基地址 | 组 |
| ------ | ------ | -- |
| 0 | `0x11400000` | GPA0、GPA1、GPB、GPC0、GPC1、GPD0、GPD1、GPF0~3、GPJ0、GPJ1 |
| 1 | `0x11000000` | GPK0~3、GPL0~2、GPM0~4、GPY0~6、GPX0~3（GPX0CON 在 `0x11000C00`） |
| 2 | `0x03860000` | GPZ |
| 3 | `0x106E0000` | GPV0~4 |

每组寄存器：`CON(+0x00)`、`DAT(+0x04)`、`PUD(+0x08)`、`DRV(+0x0C)`。

### UART

| 寄存器 | 偏移 | 寄存器 | 偏移 |
| ------ | ---- | ------ | ---- |
| ULCON | 0x00 | UTRSTAT | 0x10 |
| UCON | 0x04 | UTXH | 0x14 |
| UFCON | 0x08 | URXH | 0x18 |
| UMCON | 0x0C | UBRDIV / UDIVSLOT | 0x1C / 0x20 |

波特率：`UBRDIV = UART时钟/(波特率×16) − 1`。标准时钟配置下 **SCLK_UART = 100MHz**，
115200 波特率对应 `UBRDIV=53`。

### DDR（POP 封装 = LPDDR2）

- DMC0 = `0x10600000`，DMC1 = `0x10610000`，两控制器 128 字节交织；
- DDR 空间 = `0x40000000` 起，主程序运行在 `0x43E00000`；
- 初始化流程与寄存器值移植自 U-Boot Exynos4412 SPL，含 PHY DLL、ZQ 校准、AC 时序、MRS；
- 芯片版本（`PRO_ID[7:4]`）与封装自动检测，1GB/2GB 使用不同时序参数。

### 时钟链要点

- APLL = 800MHz（ARM 主频，比迅为 U-Boot 的 1000MHz 更稳，裸机不依赖 PMIC 1.3V）；
- MPLL = 800MHz，`MOUTMPLL_USER`（CLK_SRC_CPU bit24）必须置 1，否则 ACLK_100=3MHz，
  串口/背光/屏幕全部异常；
- ACLK_100 = 100MHz（PWM 定时器 PCLK 来源，`DIV_TOP=0x01315474`）；
- PWM0 蜂鸣器 = 100MHz/50 = 2MHz 计数；PWM2 毫秒节拍 = 100MHz/100 = 1MHz。

## 常见问题

**初始化阶段蜂鸣器一直响**

上电后 GPD0_0 先保持 GPIO 低电平，播放歌曲时才切到 TOUT_0；若仍误响，检查驱动管
Q5 基极网络（R22）是否悬空。

**歌曲衔接处有尖锐蜂鸣**

PWM 通道停止时 TOUT_0 电平不确定，休止/静音时库会把引脚强制拉低（GPIO 输出 0），
确保驱动管截止。

**屏幕闪烁**

面板采用"只重画变化行"的增量绘制，不再整块清屏；若仍闪烁，检查 FIMD VCLK
（VIDCON0 CLKVAL=5 -> 66.7MHz）与背光 PWM1 是否被误改。

**串口没输出 / 乱码**

确认波特率分频用的时钟（SCLK_UART=100MHz，UBRDIV=53），以及发送延时
（CPU 800MHz 下 `UART_SendData` 每字节约 185us，慢于 115200 线路速率防丢字节）。

**SD 模式跑不起来**

- 确认 `bl2_14k.bin` 烧在扇区 17、BL1 在扇区 1；
- 确认拨码开关为 SD 启动；
- 若 BL1 加载 BL2 地址与链接地址（`0x02023400`）不一致，改 `startup/exynos4412.lds`。
