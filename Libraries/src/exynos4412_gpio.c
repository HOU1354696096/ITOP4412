/**
 * @file    exynos4412_gpio.c
 * @brief   Exynos4412 GPIO 库实现 (STM32 库风格)
 */
#include "exynos4412_gpio.h"

/**
 * @brief  初始化 GPIO 引脚: 功能、上下拉、驱动强度
 * @param  GPIOx            组指针, 如 GPA0
 * @param  GPIO_InitStruct  初始化参数
 */
void GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_InitStruct)
{
    uint32_t pinpos = 0;
    uint32_t pos = 0;
    uint32_t con = GPIOx->CON;
    uint32_t pud = GPIOx->PUD;
    uint32_t drv = GPIOx->DRV;

    for (pinpos = 0; pinpos < 8; pinpos++) {
        pos = ((uint32_t)0x0001) << pinpos;

        /* 只配置结构体中指定的引脚 */
        if ((GPIO_InitStruct->GPIO_Pin & pos) != pos) {
            continue;
        }

        /* CON: 每引脚 4 位, 0=输入 1=输出 2~15=复用功能 */
        {
            uint32_t shift = pinpos * 4;
            uint32_t mode  = GPIO_InitStruct->GPIO_Mode;

            if (mode == GPIO_Mode_AF) {
                mode = GPIO_InitStruct->GPIO_AF & 0x0F;
            }
            con &= ~(((uint32_t)0x0F) << shift);
            con |= (mode & 0x0F) << shift;
        }

        /* PUD: 每引脚 2 位, 00=无 01=下拉 11=上拉 */
        {
            uint32_t shift = pinpos * 2;
            pud &= ~(((uint32_t)0x03) << shift);
            pud |= (GPIO_InitStruct->GPIO_PuPd & 0x03) << shift;
        }

        /* DRV: 每引脚 2 位, 00=LV1 01=LV2 10=LV3 11=LV4 */
        {
            uint32_t shift = pinpos * 2;
            drv &= ~(((uint32_t)0x03) << shift);
            drv |= (GPIO_InitStruct->GPIO_Drv & 0x03) << shift;
        }
    }

    GPIOx->CON = con;
    GPIOx->PUD = pud;
    GPIOx->DRV = drv;
}

/**
 * @brief 将指定引脚输出置 1
 */
void GPIO_SetBits(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin)
{
    GPIOx->DAT |= GPIO_Pin;
}

/**
 * @brief 将指定引脚输出清零
 */
void GPIO_ResetBits(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin)
{
    GPIOx->DAT &= ~GPIO_Pin;
}

/**
 * @brief 按 BitVal 写指定引脚
 */
void GPIO_WriteBit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin, BitAction BitVal)
{
    if (BitVal != Bit_RESET) {
        GPIOx->DAT |= GPIO_Pin;
    } else {
        GPIOx->DAT &= ~GPIO_Pin;
    }
}

/**
 * @brief 翻转指定引脚输出
 */
void GPIO_ToggleBits(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin)
{
    GPIOx->DAT ^= GPIO_Pin;
}

/**
 * @brief 读取单个引脚输入电平
 * @retval 0 或 1
 */
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin)
{
    return (GPIOx->DAT & GPIO_Pin) ? 1 : 0;
}

/**
 * @brief 读取整组引脚输入
 */
uint32_t GPIO_ReadInputData(GPIO_TypeDef *GPIOx)
{
    return GPIOx->DAT;
}

/**
 * @brief 读取整组引脚输出
 */
uint32_t GPIO_ReadOutputData(GPIO_TypeDef *GPIOx)
{
    return GPIOx->DAT;
}
