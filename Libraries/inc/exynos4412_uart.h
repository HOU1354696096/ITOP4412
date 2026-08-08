/**
 * @file    exynos4412_uart.h
 * @brief   Exynos4412 UART 库头文件（STM32 库风格）
 *
 * 重要: Exynos4412 的 UART 寄存器偏移与老款 s3c2410 不同!
 *   4412 实际偏移(已用迅为 iTOP-4412 实测汇编例程核对):
 *     ULCON 0x00  UCON 0x04  UFCON 0x08  UMCON 0x0C
 *     UTRSTAT 0x10  UERSTAT 0x14  UFSTAT 0x18  UMSTAT 0x1C
 *     UTXH 0x20  URXH 0x24  UBRDIV 0x28  UDIVSLOT 0x2C
 *
 * 常用引脚复用（功能号 2, 已按 Exynos4412 用户手册 4.3.2 GPA0CON/GPA1CON 核对）:
 *   UART0: GPA0_0=RXD, GPA0_1=TXD
 *   UART1: GPA0_4=RXD, GPA0_5=TXD   <-- 底板 GPS/核心板连接器 (XuTXD1/XuRXD1)
 *   UART2: GPA1_0=RXD, GPA1_1=TXD   <-- iTOP-4412 底板 DB9 调试串口
 *   UART3: GPA1_4=RXD, GPA1_5=TXD   <-- iTOP-4412 底板 CON2 DB9 "串口1"
 *
 * 波特率: UBRDIV = UART时钟/(波特率*16) - 1
 *          UDIVSLOT 由分频余数确定(16 位 slot 表)
 * 本工程时钟配置下 SCLK_UART = 100MHz, 115200 波特率对应 UBRDIV=53。
 */
#ifndef __EXYNOS4412_UART_H
#define __EXYNOS4412_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/*------------------- UART 寄存器组 -------------------*/
typedef struct {
    __IO uint32_t ULCON;     /* 0x00 线控制: 帧格式 */
    __IO uint32_t UCON;      /* 0x04 控制(时钟源/收发模式) */
    __IO uint32_t UFCON;     /* 0x08 FIFO 控制 */
    __IO uint32_t UMCON;     /* 0x0C 调制解调控制 */
    __I  uint32_t UTRSTAT;   /* 0x10 收发状态 */
    __I  uint32_t UERSTAT;   /* 0x14 错误状态 */
    __I  uint32_t UFSTAT;    /* 0x18 FIFO 状态 */
    __I  uint32_t UMSTAT;    /* 0x1C 调制解调状态 */
    __O  uint32_t UTXH;      /* 0x20 发送缓冲(写) */
    __I  uint32_t URXH;      /* 0x24 接收缓冲(读) */
    __IO uint32_t UBRDIV;    /* 0x28 波特率分频 */
    __IO uint32_t UDIVSLOT;  /* 0x2C 分频余数 slot 表 */
    __IO uint32_t UINTP;     /* 0x30 中断请求 */
    __IO uint32_t UINTSP;    /* 0x34 中断源 */
    __IO uint32_t UINTM;     /* 0x38 中断屏蔽 */
} UART_TypeDef;

/*------------------- UART 外设地址 -------------------*/
#define UART0   ((UART_TypeDef *)EXYNOS4412_UART0_BASE)
#define UART1   ((UART_TypeDef *)EXYNOS4412_UART1_BASE)
#define UART2   ((UART_TypeDef *)EXYNOS4412_UART2_BASE)
#define UART3   ((UART_TypeDef *)EXYNOS4412_UART3_BASE)

/*------------------- 状态标志 (UTRSTAT) -------------------*/
#define UART_FLAG_RXNE   ((uint16_t)0x0001)  /* UTRSTAT[0]: 接收缓冲数据就绪 */
#define UART_FLAG_TXE    ((uint16_t)0x0002)  /* UTRSTAT[1]: 发送缓冲空 */

/*------------------- 帧格式定义 -------------------*/
typedef enum {
    UART_WordLength_5b = 0x0,  /* ULCON[1:0] */
    UART_WordLength_6b = 0x1,
    UART_WordLength_7b = 0x2,
    UART_WordLength_8b = 0x3
} UARTWordLength_TypeDef;

typedef enum {
    UART_StopBits_1 = 0x0,     /* ULCON[3:2] */
    UART_StopBits_2 = 0x1
} UARTStopBits_TypeDef;

typedef enum {
    UART_Parity_No   = 0x00,   /* ULCON[6:5] */
    UART_Parity_Odd  = 0x20,
    UART_Parity_Even = 0x60
} UARTParity_TypeDef;

typedef enum {
    UART_Mode_Rx    = 0x1,
    UART_Mode_Tx    = 0x2,
    UART_Mode_Tx_Rx = 0x3
} UARTMode_TypeDef;

/* UCON[11:10] 时钟源 */
#define UART_ClockSource_PCLK   ((uint32_t)0x0)   /* PCLK */
#define UART_ClockSource_SCLK   ((uint32_t)0x3)   /* SCLK_UART (推荐, 100MHz) */

/*------------------- 初始化结构体 (STM32 库风格) -------------------*/
typedef struct {
    uint32_t UART_BaudRate;      /* 波特率, 如 115200 */
    uint32_t UART_WordLength;    /* 数据位 */
    uint32_t UART_StopBits;      /* 停止位 */
    uint32_t UART_Parity;        /* 校验 */
    uint32_t UART_Mode;          /* 收发模式 */
    uint32_t UART_Clock;         /* UART 输入时钟 (Hz), 本工程 100000000 */
    uint32_t UART_ClockSource;   /* UART_ClockSource_PCLK / UART_ClockSource_SCLK */
} UART_InitTypeDef;

/*------------------- 函数声明 -------------------*/
void     UART_ClockCmd(UART_TypeDef *UARTx, uint8_t Enable);
void     UART_GPIO_Init(UART_TypeDef *UARTx);
void     UART_Init(UART_TypeDef *UARTx, UART_InitTypeDef *UART_InitStruct);
void     UART_SendData(UART_TypeDef *UARTx, uint16_t Data);
uint16_t UART_ReceiveData(UART_TypeDef *UARTx);
void     UART_SendString(UART_TypeDef *UARTx, const char *str);
FlagStatus UART_GetFlagStatus(UART_TypeDef *UARTx, uint16_t UART_FLAG);

/** @brief 发送通知弱回调 (应用层可定义强符号覆盖, 用于 TX LOG) */
void UART_TxNotify(UART_TypeDef *UARTx, uint8_t ch);

/** @brief 发送通知屏蔽: 置 1 时 UART_SendData 不触发 UART_TxNotify (用于回显) */
extern uint8_t UART_TxNotifyMask;

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_UART_H */
