/**
 * @file    exynos4412_gpio.h
 * @brief   Exynos4412 GPIO 库头文件 (STM32 库风格)
 *
 * Exynos4412 的 GPIO 每个引脚由 4 位 CON 寄存器控制:
 *   0 = 输入, 1 = 输出, 2~15 = 各种复用功能(具体功能号查芯片手册)
 * 每组寄存器: CON(0x00) DAT(0x04) PUD(0x08) DRV(0x0C)
 */
#ifndef __EXYNOS4412_GPIO_H
#define __EXYNOS4412_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/*------------------- GPIO 寄存器组 -------------------*/
typedef struct {
    __IO uint32_t CON;      /* 0x00 引脚功能配置(每引脚 4 位) */
    __IO uint32_t DAT;      /* 0x04 数据寄存器 */
    __IO uint32_t PUD;      /* 0x08 上/下拉电阻(每引脚 2 位) */
    __IO uint32_t DRV;      /* 0x0C 驱动强度(每引脚 2 位) */
    __IO uint32_t CONPDN;   /* 0x10 掉电模式配置 */
    __IO uint32_t PUDPDN;   /* 0x14 掉电模式上下拉 */
} GPIO_TypeDef;

/*------------------- GPIO 外设地址 -------------------*/
/* 控制器1: 0x11400000 */
#define GPA0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x000UL))
#define GPA1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x020UL))
#define GPB     ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x040UL))
#define GPC0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x060UL))
#define GPC1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x080UL))
#define GPD0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x0A0UL))
#define GPD1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x0C0UL))
#define GPF0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x180UL))
#define GPF1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x1A0UL))
#define GPF2    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x1C0UL))
#define GPF3    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x1E0UL))
#define GPJ0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x240UL))
#define GPJ1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO1_BASE + 0x260UL))

/* 控制器2: 0x11000000 */
#define GPK0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x040UL))
#define GPK1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x060UL))
#define GPK2    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x080UL))
#define GPK3    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x0A0UL))
#define GPL0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x0C0UL))
#define GPL1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x0E0UL))
#define GPL2    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x100UL))
#define GPM0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x260UL))
#define GPM1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x280UL))
#define GPM2    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x2A0UL))
#define GPM3    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x2C0UL))
#define GPM4    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x2E0UL))
#define GPY0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x120UL))
#define GPY1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x140UL))
#define GPY2    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x160UL))
#define GPY3    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x180UL))
#define GPY4    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x1A0UL))
#define GPY5    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x1C0UL))
#define GPY6    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0x1E0UL))
#define GPX0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0xC00UL))
#define GPX1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0xC20UL))
#define GPX2    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0xC40UL))
#define GPX3    ((GPIO_TypeDef *)(EXYNOS4412_GPIO2_BASE + 0xC60UL))

/* 控制器3: 0x03860000 */
#define GPZ     ((GPIO_TypeDef *)(EXYNOS4412_GPIO3_BASE + 0x000UL))

/* 控制器4: 0x106E0000 */
#define GPV0    ((GPIO_TypeDef *)(EXYNOS4412_GPIO4_BASE + 0x000UL))
#define GPV1    ((GPIO_TypeDef *)(EXYNOS4412_GPIO4_BASE + 0x020UL))
#define GPV2    ((GPIO_TypeDef *)(EXYNOS4412_GPIO4_BASE + 0x040UL))
#define GPV3    ((GPIO_TypeDef *)(EXYNOS4412_GPIO4_BASE + 0x060UL))
#define GPV4    ((GPIO_TypeDef *)(EXYNOS4412_GPIO4_BASE + 0x080UL))

/*------------------- 引脚定义 -------------------*/
#define GPIO_Pin_0      ((uint32_t)0x0001)
#define GPIO_Pin_1      ((uint32_t)0x0002)
#define GPIO_Pin_2      ((uint32_t)0x0004)
#define GPIO_Pin_3      ((uint32_t)0x0008)
#define GPIO_Pin_4      ((uint32_t)0x0010)
#define GPIO_Pin_5      ((uint32_t)0x0020)
#define GPIO_Pin_6      ((uint32_t)0x0040)
#define GPIO_Pin_7      ((uint32_t)0x0080)
#define GPIO_Pin_All    ((uint32_t)0x00FF)

/*------------------- 模式定义 -------------------*/
typedef enum {
    GPIO_Mode_IN  = 0x0,   /* 输入 */
    GPIO_Mode_OUT = 0x1,   /* 输出 */
    GPIO_Mode_AF  = 0x2    /* 复用功能(具体功能号由 GPIO_AF 指定, 2~15) */
} GPIOMode_TypeDef;

/* 上下拉 */
typedef enum {
    GPIO_PuPd_NOPULL = 0x0,  /* 无上下拉 */
    GPIO_PuPd_DOWN   = 0x1,  /* 下拉 */
    GPIO_PuPd_UP     = 0x3   /* 上拉 */
} GPIOPuPd_TypeDef;

/* 驱动强度 */
typedef enum {
    GPIO_Drv_LV1 = 0x0,  /* 1x */
    GPIO_Drv_LV2 = 0x1,  /* 2x */
    GPIO_Drv_LV3 = 0x2,  /* 3x */
    GPIO_Drv_LV4 = 0x3   /* 4x */
} GPIODrv_TypeDef;

/*------------------- 初始化结构体 (STM32 风格) -------------------*/
typedef struct {
    uint32_t GPIO_Pin;      /* 要配置的引脚, 可多个 "|" 组合 */
    uint32_t GPIO_Mode;     /* GPIO_Mode_IN / GPIO_Mode_OUT / GPIO_Mode_AF */
    uint32_t GPIO_AF;       /* 复用功能号 0~15, GPIO_Mode_AF 时有效 */
    uint32_t GPIO_PuPd;     /* 上下拉 */
    uint32_t GPIO_Drv;      /* 驱动强度 */
} GPIO_InitTypeDef;

/*------------------- 函数声明 -------------------*/
void    GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_InitStruct);
void    GPIO_SetBits(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin);
void    GPIO_ResetBits(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin);
void    GPIO_WriteBit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin, BitAction BitVal);
void    GPIO_ToggleBits(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin);
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin);
uint32_t GPIO_ReadInputData(GPIO_TypeDef *GPIOx);
uint32_t GPIO_ReadOutputData(GPIO_TypeDef *GPIOx);

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_GPIO_H */
