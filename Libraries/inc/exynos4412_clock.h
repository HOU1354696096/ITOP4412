/**
 * @file    exynos4412_clock.h
 * @brief   Exynos4412 时钟库头文件
 *
 * System_ClockInit 配置后的频率:
 *   APLL = 800MHz  (ARMCLK 800MHz)
 *   MPLL = 800MHz  (SCLK_MPLL_USER_T = 800MHz)
 *   EPLL =  96MHz
 *   VPLL = 108MHz
 *   SCLK_UART = MPLL / 8 = 100MHz
 */
#ifndef __EXYNOS4412_CLOCK_H
#define __EXYNOS4412_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/* 4412 典型外部晶振 */
#define EXYNOS4412_XTAL_FREQ       24000000UL
/* System_ClockInit 后的 UART 时钟 */
#define EXYNOS4412_SCLK_UART_HZ    100000000UL

void System_ClockInit(void);
void System_Delay(uint32_t loops);

/**
 * @brief 初始化毫秒节拍 (借用 PWM 定时器2 做 1MHz 自由计数)
 * @note  只使用 TCFG0[15:8]/TCFG1[11:8]/TCON[15:12] 与 TCNTB2,
 *        不影响 LCD 背光 PWM1 与蜂鸣器 PWM0; 主程序初始化时调用
 */
void System_TickInit(void);

/**
 * @brief 获取上电后经过的毫秒数 (需先 System_TickInit)
 */
uint32_t System_GetMs(void);

/**
 * @brief 获取毫秒节拍的原始计数器值 (TCNTO2, 1MHz 递减计数)
 * @note  用于诊断: 连续两次读数相同说明节拍硬件已停止
 */
uint32_t System_GetTickRaw(void);

/* start.S 调用的系统初始化入口 (在 system_4412.c 中实现) */
void SystemInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_CLOCK_H */
