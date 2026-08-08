/**
 * @file    exynos4412_ddr.c
 * @brief   Exynos4412 DDR(LPDDR2 POP) 库实现
 *
 * 初始化流程逐寄存器移植自迅为官方 POP 1G u-boot
 * (iTop4412_uboot_public 仓库 uboot_pop_1GDDR 分支 cpu_init.S
 *  mem_ctrl_asm_init, 与 ITOP4412-POP-1G-uboot-2017 dmc_init_exynos4.c
 *  CONFIG_ITOP4412 分支完全一致, 该分支已在 TF 卡上验证可运行)。
 *
 * 注意:
 *   1) 本序列只适用于 LPDDR2(POP 封装) 1G 核心板;
 *   2) DDR_Init 必须在高速时钟切换之前调用(SD 卡裸机启动时),
 *      即先 DDR_Init() 再 System_ClockInit(), 与 u-boot 流程一致;
 *   3) 不要在 DDR 初始化时等待 PHYSTATUS, 官方序列为固定延时。
 */
#include "exynos4412_ddr.h"
#include "exynos4412_clock.h"

/*------------------- mem_ctrl_init 前置寄存器 -------------------*/
#define DDR_ASYNC_CONFIG        0x10010350UL   /* CPU_core 异步桥配置: 1=half_sync */
#define DDR_CLK_DIV_DMC0        0x10040500UL   /* CMU_DMC 分频 0 */
#define DDR_CLK_DIV_DMC0_INIT   0x13113113UL   /* iROM 低速 DMC 时钟下的初始分频 */
#define DDR_CLK_DIV_DMC0_FAST   0x00117713UL   /* 第二次写入(官方原样保留) */
#define DDR_REG_10020A00        0x10020A00UL   /* 官方序列保留寄存器 */
#define DDR_REG_10040A00        0x10040A00UL   /* 官方序列保留寄存器 */

/*------------------- TZASC -------------------*/
#define DMC_TZASC_STRIDE      0x10000UL
#define DMC_TZASC_ATTR0_OFF   0x108UL
#define DMC_TZASC_ALLOW_NSEC  0xf0000000UL   /* 允许安全/非安全访问全部内存 */

/**
 * @brief 配置 TrustZone 地址空间控制器, 允许所有主设备访问 DDR
 */
static void DDR_TzascInit(void)
{
    uint32_t base = EXYNOS4412_DMC_TZASC_BASE;
    uint32_t i;

    for (i = 0; i < 4; i++) {
        *(volatile uint32_t *)(base + i * DMC_TZASC_STRIDE + DMC_TZASC_ATTR0_OFF)
            = DMC_TZASC_ALLOW_NSEC;
    }
}

/**
 * @brief 初始化单个 DMC 控制器(官方 POP 1G LPDDR2 序列)
 * @param dmc DMC0(0x10600000) 或 DMC1(0x10610000)
 *
 * 与官方 cpu_init.S mem_ctrl_asm_init 逐条一致:
 *   PHYZQCONTROL=0xE3855403
 *   PHYCONTROL0/1 抖动序列(不开 SHGATE)
 *   CONCONTROL=0x0FFF30CA -> MEMCONTROL -> MEMCONFIG0 -> IVCONTROL
 *   PRECHCONFIG=0xFF000000 -> 时序 -> 6 条 DIRECTCMD -> CONCONTROL=0x0FFF303A
 */
static void DDR_DmcInit(DMC_TypeDef *dmc)
{
    /* ZQ 校准: 终止关闭, 自动校准启动 */
    dmc->PHYZQCONTROL = 0xE3855403UL;

    /* PHY DLL 启动序列(与官方逐条一致, 不写 SHGATE) */
    dmc->PHYCONTROL0 = 0x71101008UL;
    dmc->PHYCONTROL0 = 0x7110100AUL;
    dmc->PHYCONTROL1 = 0x00000084UL;
    dmc->PHYCONTROL0 = 0x71101008UL;
    dmc->PHYCONTROL1 = 0x0000008CUL;
    dmc->PHYCONTROL1 = 0x00000084UL;
    dmc->PHYCONTROL1 = 0x0000008CUL;
    dmc->PHYCONTROL1 = 0x00000084UL;

    /* 控制器配置(先关闭自动刷新) */
    dmc->CONCONTROL  = 0x0FFF30CAUL;

    /* 存储器控制 / 配置 */
    dmc->MEMCONTROL  = 0x00202500UL;
    dmc->MEMCONFIG0  = 0x40C01323UL;   /* 片选基址 0x40000000 */

    /* DMC 间 128B 交织 */
    dmc->IVCONTROL   = 0x8000001DUL;

    /* 预充电配置 */
    dmc->PRECHCONFIG = 0xFF000000UL;

    /* AC 时序(DMC 400MHz, 与 DRAM_CLK_400 一致) */
    dmc->TIMINGREF   = 0x0000005DUL;
    dmc->TIMINGROW   = 0x34498691UL;
    dmc->TIMINGDATA  = 0x36330306UL;
    dmc->TIMINGPOWER = 0x50380365UL;

    System_Delay(0x100000);

    /* 直接命令序列: NOP -> MRS -> MRS -> MRS -> MRS -> MRS (LPDDR2 MRW) */
    dmc->DIRECTCMD   = 0x07000000UL;   /* NOP: 拉高 CKE */
    System_Delay(0x100000);

    dmc->DIRECTCMD   = 0x00071C00UL;
    System_Delay(0x100000);

    dmc->DIRECTCMD   = 0x00010BFCUL;
    System_Delay(0x100000);

    dmc->DIRECTCMD   = 0x00000488UL;
    dmc->DIRECTCMD   = 0x00000810UL;
    dmc->DIRECTCMD   = 0x00000C08UL;

    /* 打开 DREX(自动刷新使能) */
    dmc->CONCONTROL  = 0x0FFF303AUL;
}

/**
 * @brief  DDR 初始化入口(官方 POP 1G 流程)
 * @param  ram_size_mb 保留参数(官方序列固定为 1G POP, 可传 0/1024)
 *
 * 必须在 System_ClockInit() 之前调用!
 */
void DDR_InitEx(uint32_t ram_size_mb)
{
    (void)ram_size_mb;

    /* 1. CPU_core 异步桥配置: half_sync */
    *(volatile uint32_t *)DDR_ASYNC_CONFIG = 1UL;

    /* 2. 低速时钟下的 DMC 分频(官方原样两次写入) */
    *(volatile uint32_t *)DDR_CLK_DIV_DMC0 = DDR_CLK_DIV_DMC0_INIT;
    *(volatile uint32_t *)DDR_CLK_DIV_DMC0 = DDR_CLK_DIV_DMC0_FAST;

    /* 3. 官方保留寄存器写入 */
    *(volatile uint32_t *)DDR_REG_10020A00 = 0x00000000UL;
    *(volatile uint32_t *)DDR_REG_10040A00 = 0x00010905UL;

    /* 4. 分别初始化 DMC0 和 DMC1 */
    DDR_DmcInit(DMC0);
    DDR_DmcInit(DMC1);

    /* 5. 允许非安全访问 */
    DDR_TzascInit();
}

/**
 * @brief 自动初始化 DDR(POP 1G)
 * @note  仅用于 DDR 尚未初始化的场景(SD 卡裸机启动), 须先于时钟切换
 */
void DDR_Init(void)
{
    DDR_InitEx(1024);
}

/**
 * @brief 时钟切换后的 DLL 启动与控制器收尾(官方 lowlevel_init_POP.S
 *        system_clock_init 末尾 MEM_DLLl_ON 块)
 *
 * 顺序: DDR_Init() -> System_ClockInit() -> DDR_DllStartPost()
 * 对应官方: mem_ctrl_asm_init -> system_clock_init
 */
void DDR_DllStartPost(void)
{
    /* DMC0 */
    DMC0->PHYCONTROL0 = 0x7F10100AUL;
    DMC0->PHYCONTROL1 = 0xE0000084UL;
    DMC0->PHYCONTROL0 = 0x7F10100BUL;
    System_Delay(0x20000);
    DMC0->PHYCONTROL1 = 0x0000008CUL;
    DMC0->PHYCONTROL1 = 0x00000084UL;
    System_Delay(0x20000);

    /* DMC1 */
    DMC1->PHYCONTROL0 = 0x7F10100AUL;
    DMC1->PHYCONTROL1 = 0xE0000084UL;
    DMC1->PHYCONTROL0 = 0x7F10100BUL;
    System_Delay(0x20000);
    DMC1->PHYCONTROL1 = 0x0000008CUL;
    DMC1->PHYCONTROL1 = 0x00000084UL;
    System_Delay(0x20000);

    /* 打开自动刷新并配置 MEMCONTROL(与官方一致) */
    DMC0->CONCONTROL  = 0x0FFF30FAUL;
    DMC1->CONCONTROL  = 0x0FFF30FAUL;
    DMC0->MEMCONTROL  = 0x00202533UL;
    DMC1->MEMCONTROL  = 0x00202533UL;
}

/**
 * @brief 向 DDR 写入一个字节
 */
void DDR_WriteByte(uint32_t addr, uint8_t data)
{
    *(volatile uint8_t *)addr = data;
}

/**
 * @brief 从 DDR 读取一个字节
 */
uint8_t DDR_ReadByte(uint32_t addr)
{
    return *(volatile uint8_t *)addr;
}

/**
 * @brief 向 DDR 写入一个字 (32 位)
 */
void DDR_WriteWord(uint32_t addr, uint32_t data)
{
    *(volatile uint32_t *)addr = data;
}

/**
 * @brief 从 DDR 读取一个字 (32 位)
 */
uint32_t DDR_ReadWord(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

/**
 * @brief 向 DDR 写入一段数据
 */
void DDR_WriteBuffer(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        *(volatile uint8_t *)(addr + i) = buf[i];
    }
}

/**
 * @brief 从 DDR 读出一段数据
 */
void DDR_ReadBuffer(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        buf[i] = *(volatile uint8_t *)(addr + i);
    }
}

/**
 * @brief 向 DDR 一段区域填充指定值
 */
void DDR_MemSet(uint32_t addr, uint8_t val, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        *(volatile uint8_t *)(addr + i) = val;
    }
}
