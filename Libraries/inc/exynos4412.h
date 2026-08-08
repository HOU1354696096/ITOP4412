/**
 * @file    exynos4412.h
 * @brief   Exynos4412 芯片级定义: 内存映射、寄存器组、通用类型
 * @note    采用 STM32 标准库的编程风格编写
 */
#ifndef __EXYNOS4412_H
#define __EXYNOS4412_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*------------------- STM32 风格通用类型定义 -------------------*/
#define __I     volatile const
#define __O     volatile
#define __IO    volatile

typedef enum { RESET = 0, SET = !RESET } FlagStatus;
typedef enum { DISABLE = 0, ENABLE = !DISABLE } FunctionalState;
typedef enum { Bit_RESET = 0, Bit_SET = !Bit_RESET } BitAction;

/*------------------- Exynos4412 内存映射 -------------------*/
/* 内部 SRAM(iRAM), 256KB, 0x02020000 ~ 0x0205FFFF */
#define EXYNOS4412_IRAM_BASE        0x02020000UL
#define EXYNOS4412_IRAM_SIZE        0x00040000UL

/* DDR 起始地址 */
#define EXYNOS4412_DRAM_BASE        0x40000000UL

/* 系统寄存器 */
#define EXYNOS4412_PRO_ID           0x10000000UL   /* 芯片 ID / 版本 */
#define EXYNOS4412_CLOCK_BASE       0x10030000UL   /* CMU 时钟 */

/* 动态内存控制器 (POP 封装为 LPDDR2) */
#define EXYNOS4412_DMC0_BASE        0x10600000UL
#define EXYNOS4412_DMC1_BASE        0x10610000UL
#define EXYNOS4412_DMC_TZASC_BASE   0x10700000UL  /* TrustZone 地址空间控制器 */

/* GPIO 控制器(注意: 4412 没有 GPE/GPG/GPH 组, 与 4210 不同) */
#define EXYNOS4412_GPIO1_BASE       0x11400000UL  /* GPA0~GPD1, GPF0~GPF3, GPJ0~GPJ1 */
#define EXYNOS4412_GPIO2_BASE       0x11000000UL  /* GPK0~GPK3, GPL0~GPL2, GPM0~GPM4, GPY0~GPY6, GPX0~GPX3 */
#define EXYNOS4412_GPIO3_BASE       0x03860000UL  /* GPZ */
#define EXYNOS4412_GPIO4_BASE       0x106E0000UL  /* GPV0~GPV4 */

/* UART */
#define EXYNOS4412_UART0_BASE       0x13800000UL
#define EXYNOS4412_UART1_BASE       0x13810000UL
#define EXYNOS4412_UART2_BASE       0x13820000UL
#define EXYNOS4412_UART3_BASE       0x13830000UL

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_H */
