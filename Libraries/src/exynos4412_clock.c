/**
 * @file    exynos4412_clock.c
 * @brief   Exynos4412 时钟库（最小化配置，与已实测可用的汇编例程一致）
 *
 * 只配置串口/外设必需的三组时钟:
 *   - MPLL = 800MHz (0x80640300: MDIV=100, PDIV=3, SDIV=0, 24MHz 晶振)
 *   - CLK_SRC_DMC / CLK_SRC_TOP0 / CLK_SRC_TOP1 / CLK_SRC_PERIL0: 选择时钟源
 *   - CLK_DIV_TOP / CLK_DIV_LEFTBUS / CLK_DIV_RIGHTBUS: 总线时钟
 *     (ACLK_100=100MHz 是 PWM/串口等 PERIL 外设 PCLK 的来源)
 *   - CLK_DIV_PERIL0 = 0x777777: UART 分频 7 -> SCLK_UART = MPLL/8 = 100MHz
 *
 * 注意: 此配置逐条对应迅为 iTOP-4412 上已验证可用的 UART 汇编例程
 * (main.S) 的寄存器写入，避免激进重配时钟导致板子卡死。
 * 总线时钟部分对应 POP 板官方 u-boot lowlevel_init_POP.S。
 *
 * 2026-08-07 重要修正:
 *   4412 的 ACLK_100 和 FIMD 时钟源都来自 MOUTMPLL_USER,
 *   而 MOUTMPLL_USER 由 CLK_SRC_CPU[24] 选择 (0=FINPLL 24MHz, 1=FOUTMPLL 800MHz)。
 *   此前未配置 CLK_SRC_CPU, 整条链一直跑在 24MHz: ACLK_100=3MHz,
 *   FIMD VCLK=1.5MHz (刷新率约 1Hz), PWM 背光不工作。
 *   现按 u-boot 置 bit24=1, 与内核 clock-exynos4212.c 的
 *   exynos4_clkset_aclk_top_list[0]=mout_mpll_user 一致。
 */
#include "exynos4412_clock.h"

/*------------------- CMU 寄存器地址（基址 0x10030000）-------------------*/
#define CLK_MPLL_CON0       (EXYNOS4412_CLOCK_BASE + 0x10108)  /* MPLL 配置 */
#define CLK_APLL_LOCK       (EXYNOS4412_CLOCK_BASE + 0x14000)  /* APLL 锁存时间 */
#define CLK_APLL_CON0       (EXYNOS4412_CLOCK_BASE + 0x14100)  /* APLL 配置 */
#define CLK_APLL_CON1       (EXYNOS4412_CLOCK_BASE + 0x14104)  /* APLL 辅助配置 */
#define CLK_SRC_CPU         (EXYNOS4412_CLOCK_BASE + 0x14200)  /* CPU 时钟源(0x10044200) */
#define CLK_DIV_CPU0        (EXYNOS4412_CLOCK_BASE + 0x14500)  /* CPU 分频 0 */
#define CLK_DIV_CPU1        (EXYNOS4412_CLOCK_BASE + 0x14504)  /* CPU 分频 1 */
#define CLK_DIV_DMC0        (EXYNOS4412_CLOCK_BASE + 0x10500)  /* DMC 分频 0 */
#define CLK_DIV_DMC1        (EXYNOS4412_CLOCK_BASE + 0x10504)  /* DMC 分频 1 */
#define CLK_SRC_DMC         (EXYNOS4412_CLOCK_BASE + 0x10200)  /* DMC 时钟源 */
#define CLK_SRC_TOP0        (EXYNOS4412_CLOCK_BASE + 0x0C210)  /* 顶层时钟源0(ACLK 总线 mux) */
#define CLK_SRC_TOP1        (EXYNOS4412_CLOCK_BASE + 0x0C214)  /* 顶层时钟源1 */
#define CLK_DIV_TOP         (EXYNOS4412_CLOCK_BASE + 0x0C510)  /* 顶层分频(ACLK_100/160/200/133) */
#define CLK_SRC_LEFTBUS     (EXYNOS4412_CLOCK_BASE + 0x04200)  /* 左总线时钟源 */
#define CLK_DIV_LEFTBUS     (EXYNOS4412_CLOCK_BASE + 0x04500)  /* 左总线分频 */
#define CLK_SRC_RIGHTBUS    (EXYNOS4412_CLOCK_BASE + 0x08200)  /* 右总线时钟源 */
#define CLK_DIV_RIGHTBUS    (EXYNOS4412_CLOCK_BASE + 0x08500)  /* 右总线分频 */
#define CLK_SRC_PERIL0      (EXYNOS4412_CLOCK_BASE + 0x0C250)  /* 外设时钟源 */
#define CLK_DIV_PERIL0      (EXYNOS4412_CLOCK_BASE + 0x0C550)  /* 外设分频 */

/* PWM 定时器 (毫秒节拍使用通道2; 蜂鸣器通道0/背光通道1 见各外设库) */
#define PWM_TCFG0           0x139D0000UL
#define PWM_TCFG1           0x139D0004UL
#define PWM_TCON            0x139D0008UL
#define PWM_TCNTB2          0x139D0024UL
#define PWM_TCNTO2          0x139D002CUL
#define CLK_GATE_IP_PERIL   0x1003C950UL   /* PWM 定时器时钟门 [24] */

/* 寄存器读写辅助 */
static inline void CLK_WR(uint32_t reg, uint32_t val)
{
    *(volatile uint32_t *)reg = val;
}

/**
 * @brief 简单忙等待延时（仅用于上电初期，时间由主频粗略决定）
 */
void System_Delay(uint32_t loops)
{
    volatile uint32_t i;
    for (i = 0; i < loops; i++) {
    }
}

/**
 * @brief 初始化毫秒节拍: PWM 定时器2 做 1MHz 自由递减计数
 *
 * 配置:
 *   - TCFG0[15:8] 预分频1 = 99 -> PCLK 100MHz/100 = 1MHz (不影响 [7:0] 背光预分频)
 *   - TCFG1[11:8] MUX2    = 0  -> 1/1 分频
 *   - TCNTB2 = 1000000 自动重载 -> 每秒回绕一次, 与背光 PWM1 相同的
 *     自动重载机制 (此前用 0xFFFFFFFF 大周期, 怀疑回绕/重载异常导致
 *     节拍运行一段时间后停止, 歌曲停在某个音符上长鸣, 面板不再刷新)
 *   - TCON 通道2: manual=bit13, start=bit12, auto=bit15
 * 读数方式: TCNTO2 递减, 回绕到 TCNTB2 时按 (last + TCNTB2 - now) 计算,
 * 保证任何 1 秒周期内读数都正确。
 */
void System_TickInit(void)
{
    uint32_t reg;
    volatile uint32_t i;

    /* 1. 打开 PWM 定时器时钟门 */
    CLK_WR(CLK_GATE_IP_PERIL, *(volatile uint32_t *)CLK_GATE_IP_PERIL | (1u << 24));

    /* 2. 预分频1 = 99 (100 分频 -> 1MHz), 只改 [15:8] */
    reg = *(volatile uint32_t *)PWM_TCFG0;
    reg = (reg & ~(0xFFu << 8)) | (99u << 8);
    CLK_WR(PWM_TCFG0, reg);

    /* 3. MUX2 = 0 (1/1), 只改 [11:8] */
    reg = *(volatile uint32_t *)PWM_TCFG1;
    reg = (reg & ~(0xFu << 8)) | (0x0u << 8);
    CLK_WR(PWM_TCFG1, reg);

    /* 4. 装载周期 1,000,000 (1s @1MHz) + 自动重载 */
    CLK_WR(PWM_TCNTB2, 1000000UL);
    reg = *(volatile uint32_t *)PWM_TCON;
    reg = (reg & ~0xF000u) | (1u << 13) | (1u << 15);   /* manual + auto */
    CLK_WR(PWM_TCON, reg);
    for (i = 0; i < 0x1000; i++) {   /* 等装载完成 */
    }

    /* 5. 清手动装载, 启动 + 保持自动重载 */
    reg = *(volatile uint32_t *)PWM_TCON;
    reg = (reg & ~(1u << 13)) | (1u << 12) | (1u << 15);
    CLK_WR(PWM_TCON, reg);
}

/**
 * @brief 返回上电后经过的毫秒数
 */
uint32_t System_GetMs(void)
{
    static uint32_t last = 0;
    static uint32_t ms   = 0;
    static uint32_t sub  = 0;   /* 不足 1ms 的微秒余数 */
    static uint8_t  first = 1;
    uint32_t now;
    uint32_t delta;

    now = System_GetTickRaw();
    if (first) {
        last  = now;
        first = 0;
        return 0;
    }
    if (now <= last) {
        /* 正常递减: 差值即流逝的 1us 数 */
        delta = last - now;
    } else {
        /* 递减计数回绕到 TCNTB2 (每秒一次) */
        delta = last + (1000000UL - now);
    }
    last  = now;

    sub += delta;
    while (sub >= 1000) {
        sub -= 1000;
        ms++;
    }
    return ms;
}

/**
 * @brief 返回毫秒节拍原始计数器 (TCNTO2)
 */
uint32_t System_GetTickRaw(void)
{
    return *(volatile uint32_t *)PWM_TCNTO2;
}

/**
 * @brief 初始化 4412 时钟（与已验证汇编例程一致的最小配置）
 *
 * 配置完成后:
 *   MPLL = 800MHz
 *   ACLK_100 = 100MHz (PERIL 外设 PCLK, PWM 定时器时钟来源)
 *   SCLK_UART = MPLL / 8 = 100MHz
 */
void System_ClockInit(void)
{
    volatile uint32_t src_cpu;

    /* 1. MPLL = 800MHz (0x80640300) */
    CLK_WR(CLK_MPLL_CON0, 0x80640300UL);

    /* 2. MUX_MPLL_USER_SEL_C[24]=1: MOUTMPLL_USER 选 FOUTMPLL(800MHz)
     *    而非复位默认的 FINPLL(24MHz)。
     *    4412 的 ACLK_100(→PWM 定时器) 与 FIMD sclk 都以此时钟为源,
     *    不置位时 ACLK_100=24/8=3MHz, 背光 PWM 几乎不走;
     *    FIMD VCLK=12MHz/(7+1)=1.5MHz, 刷新率仅约 1Hz(表现为屏闪一下即黑)。
     *    只改 bit24, 其余位保持不动 (与 u-boot CLK_SRC_CPU_VAL_MOUTMPLLFOUT 一致) */
    src_cpu = *(volatile uint32_t *)CLK_SRC_CPU;
    CLK_WR(CLK_SRC_CPU, (src_cpu & ~(1u << 24)) | (1u << 24));

    /* 2. 时钟源选择（与实测例程一致） */
    CLK_WR(CLK_SRC_DMC,    0x00001000UL);
    CLK_WR(CLK_SRC_TOP1,   0x01111000UL);
    CLK_WR(CLK_SRC_PERIL0, 0x00066666UL);

    /* DMC 分频: DDR 初始化时序参数依赖正确的 DMC 时钟 */
    CLK_WR(CLK_DIV_DMC0,   0x00111113UL);
    CLK_WR(CLK_DIV_DMC1,   0x01010013UL);

    /* 3. 顶层总线时钟 (与 POP 板官方 u-boot lowlevel_init_POP.S 一致)
     *    ACLK_100 = MPLL/(7+1) = 100MHz  -> PWM 定时器 PCLK
     *    ACLK_200 = MPLL/(4+1) = 160MHz, ACLK_160 = 160MHz, ACLK_133 = 133MHz
     *    CLK_SRC_TOP0 各 ACLK mux 选 SCLKMPLL(0x0), VPLL/EPLL mux 保持 FINPLL
     *    (我们的配置没有开 VPLL/EPLL, 因此不能选 FOUTVPLL/FOUTEPLL) */
    CLK_WR(CLK_SRC_TOP0,    0x00000000UL);
    CLK_WR(CLK_DIV_TOP,     0x01315474UL);

    /* 4. 左/右总线时钟: 分频后 GDL/GDR=200MHz, GPL/GPR=100MHz */
    CLK_WR(CLK_SRC_LEFTBUS, 0x00000010UL);
    CLK_WR(CLK_DIV_LEFTBUS, 0x00000013UL);
    CLK_WR(CLK_SRC_RIGHTBUS, 0x00000010UL);
    CLK_WR(CLK_DIV_RIGHTBUS, 0x00000013UL);

    /* 5. 外设分频: UART 分频 7 -> SCLK_UART = MPLL/8 = 100MHz */
    CLK_WR(CLK_DIV_PERIL0, 0x00077777UL);

    /* 6. CPU 时钟: APLL = 800MHz (对应官方 exynos4412_setup.h 参考配置)
     *    重要修正 (2026-08-08): 此前工程只配置了 MPLL, 从未配置 APLL,
     *    CPU 一直跑在复位频率 (约 24MHz), 导致主循环/串口打印/面板重绘
     *    慢了几十倍: 蜂鸣器旋律推进被拖死(卡在一个音符长鸣), 屏幕状态
     *    面板无法按时刷新。
     *    注意: 迅为 POP 板 uboot 用 APLL=1000MHz, 但要求 PMIC 给 ARM 1.3V
     *    (CONFIG_PM_13V_12V); 裸机未配 PMIC 电压, 1000MHz 会锁不定导致
     *    CPU 有时快有时慢(实测 DELAY16M 在 14ms 与 950ms 之间跳)。
     *    改用官方 4412 参考配置 800MHz (0x64/3/0), 对电压要求低, 锁定可靠。
     *    流程与 u-boot lowlevel_init_POP.S 一致:
     *    - 先把 ARMCLK mux 切到 FINPLL(24MHz), 外设时钟(MOUTMPLL_USER
     *      bit24)保持不变, 避免外设时钟闪断
     *    - 配置 APLL 锁存时间/CPU 分频/APLL_CON1, 再写 APLL_CON0
     *    - 等 PLL 锁定后把 ARMCLK mux 切回 FOUTAPLL (800MHz);
     *      若锁不定则保持 FINPLL, 保证系统仍可运行 */
    CLK_WR(CLK_SRC_CPU, 0x01000000UL);              /* bit0=0: ARMCLK=FINPLL; bit24=1: 外设仍用 MPLL */
    CLK_WR(CLK_APLL_LOCK, 0x000003E8UL);
    CLK_WR(CLK_DIV_CPU0,  0x01137520UL);            /* CORE2=1 APLL=2 PCLKDBG=2 ATB=4 PERIPH=8 COREM1=6 COREM0=3 CORE=1 */
    CLK_WR(CLK_DIV_CPU1,  0x00000303UL);            /* CORES=4 HPM=1 COPY=4 */
    CLK_WR(CLK_APLL_CON1, 0x00803800UL);
    CLK_WR(CLK_APLL_CON0, 0x80640300UL);            /* MDIV=100 PDIV=3 SDIV=0 -> 800MHz */
    {
        volatile uint32_t i;
        uint32_t locked = 0;
        for (i = 0; i < 0x100000; i++) {
            if (*(volatile uint32_t *)CLK_APLL_CON0 & (1u << 29)) {
                locked = 1;
                break;   /* PLL 已锁定 (CON0 bit29) */
            }
        }
        if (locked) {
            CLK_WR(CLK_SRC_CPU, 0x01000001UL);      /* bit0=1: ARMCLK=FOUTAPLL 800MHz */
        }
    }

    /* 7. 等待时钟稳定 */
    System_Delay(0x10000);
}
