/**
 * @file    debug.h
 * @brief   调试打印工具模块头文件 (从 main.c 归档拆分)
 */
#ifndef __DEBUG_H
#define __DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/** @brief 以十六进制向 UART2 打印一个 32 位寄存器值 */
void PrintHex32(uint32_t data);

/** @brief 以十六进制向 UART2 打印一个 8 位值 (2 位十六进制) */
void PrintHex8(uint8_t data);

/** @brief 以十六进制向 UART2 打印一个 16 位值 (4 位十六进制) */
void PrintHex16(uint16_t data);

/** @brief 打印 LCD/背光相关寄存器, 用于排查背光不亮/显示异常 */
void LCD_PrintDebugRegs(void);

/** @brief 打印 UART3 (CON2 串口1) 配置寄存器 */
void UART3_PrintDebugRegs(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H */
