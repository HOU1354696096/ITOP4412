/**
 * @file    system_4412.c
 * @brief   系统初始化入口 (对应 STM32 工程的 system_stm32f10x.c)
 *
 * start.S 复位后调用 SystemInit():
 *   - SD 卡裸机模式 (make RUN_MODE=sd): 初始化时钟 (与已验证例程一致的
 *     最小配置: MPLL 800MHz, SCLK_UART = 100MHz)
 *   - U-Boot 下载模式 (默认): 时钟已由 U-Boot 初始化, 直接跳过
 *
 * 说明: DDR_Init() 不在 SystemInit 中调用, 而是放在 main 里串口打印
 * 测试信息之后再调用, 这样即使 DDR 初始化有问题, 串口也能先输出。
 */
#include "exynos4412_clock.h"
#include "exynos4412_ddr.h"

void SystemInit(void)
{
#ifdef EXYNOS4412_BOOT_SD
    DDR_Init();
    System_ClockInit();
    DDR_DllStartPost();
#else
    /* U-Boot 下载模式: 时钟已由 U-Boot 初始化 */
#endif
}
