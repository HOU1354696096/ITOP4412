/**
 * @file    debug.c
 * @brief   调试打印工具模块实现 (从 main.c 归档拆分, 内容不变)
 */
#include "debug.h"
#include "exynos4412_uart.h"
#include "exynos4412_clock.h"

/**
 * @brief 以十六进制打印 32 位寄存器值 (UART2)
 */
void PrintHex32(uint32_t data)
{
    static const char hex[] = "0123456789ABCDEF";
    int i;
    for (i = 7; i >= 0; i--) {
        UART_SendData(UART2, (uint16_t)hex[(data >> (i * 4)) & 0x0F]);
    }
}

/**
 * @brief 以十六进制打印 8 位值 (2 位, UART2)
 */
void PrintHex8(uint8_t data)
{
    static const char hex[] = "0123456789ABCDEF";
    UART_SendData(UART2, (uint16_t)hex[(data >> 4) & 0x0F]);
    UART_SendData(UART2, (uint16_t)hex[data & 0x0F]);
}

/**
 * @brief 以十六进制打印 16 位值 (4 位, UART2)
 */
void PrintHex16(uint16_t data)
{
    static const char hex[] = "0123456789ABCDEF";
    UART_SendData(UART2, (uint16_t)hex[(data >> 12) & 0x0F]);
    UART_SendData(UART2, (uint16_t)hex[(data >> 8) & 0x0F]);
    UART_SendData(UART2, (uint16_t)hex[(data >> 4) & 0x0F]);
    UART_SendData(UART2, (uint16_t)hex[data & 0x0F]);
}

/**
 * @brief 打印 LCD/背光相关寄存器 (排查背光不亮用)
 */
void LCD_PrintDebugRegs(void)
{
    UART_SendString(UART2, "GPL0CON="); PrintHex32(*(volatile uint32_t *)0x110000C0); UART_SendString(UART2, " DAT="); PrintHex32(*(volatile uint32_t *)0x110000C4); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "GPL1CON="); PrintHex32(*(volatile uint32_t *)0x110000E0); UART_SendString(UART2, " DAT="); PrintHex32(*(volatile uint32_t *)0x110000E4); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "GPD0CON="); PrintHex32(*(volatile uint32_t *)0x114000A0); UART_SendString(UART2, " DAT="); PrintHex32(*(volatile uint32_t *)0x114000A4); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "TCFG0=");   PrintHex32(*(volatile uint32_t *)0x139D0000);
    UART_SendString(UART2, " TCFG1=");  PrintHex32(*(volatile uint32_t *)0x139D0004);
    UART_SendString(UART2, " TCON=");   PrintHex32(*(volatile uint32_t *)0x139D0008); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "TCNTB1=");  PrintHex32(*(volatile uint32_t *)0x139D0018);
    UART_SendString(UART2, " TCMPB1="); PrintHex32(*(volatile uint32_t *)0x139D001C); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "TCNTO1a="); PrintHex32(*(volatile uint32_t *)0x139D0020);
    System_Delay(0x2000);  /* 延时后再读一次, 确认计数器在跑 */
    UART_SendString(UART2, " TCNTO1b="); PrintHex32(*(volatile uint32_t *)0x139D0020); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "VIDCON0="); PrintHex32(*(volatile uint32_t *)0x11C00000);
    UART_SendString(UART2, " VIDCON1="); PrintHex32(*(volatile uint32_t *)0x11C00004);
    UART_SendString(UART2, " VIDCON2="); PrintHex32(*(volatile uint32_t *)0x11C00008); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "VIDTCON0="); PrintHex32(*(volatile uint32_t *)0x11C00010);
    UART_SendString(UART2, " VIDTCON1="); PrintHex32(*(volatile uint32_t *)0x11C00014);
    UART_SendString(UART2, " VIDTCON2="); PrintHex32(*(volatile uint32_t *)0x11C00018); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "WINCON0="); PrintHex32(*(volatile uint32_t *)0x11C00020);
    UART_SendString(UART2, " WINSHMAP="); PrintHex32(*(volatile uint32_t *)0x11C00034);
    UART_SendString(UART2, " ADD2="); PrintHex32(*(volatile uint32_t *)0x11C00100);
    UART_SendString(UART2, " FB0="); PrintHex32(*(volatile uint32_t *)0x11C000A0); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "GPF0DRV="); PrintHex32(*(volatile uint32_t *)0x1140018C);
    UART_SendString(UART2, " GPF3DRV="); PrintHex32(*(volatile uint32_t *)0x114001EC); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "SRC_LCD0="); PrintHex32(*(volatile uint32_t *)0x1003C234);
    UART_SendString(UART2, " DIV_LCD0="); PrintHex32(*(volatile uint32_t *)0x1003C534); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "GATE_PERIL="); PrintHex32(*(volatile uint32_t *)0x1003C950); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "SRC_CPU="); PrintHex32(*(volatile uint32_t *)0x10044200); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "SRC_TOP0="); PrintHex32(*(volatile uint32_t *)0x1003C210);
    UART_SendString(UART2, " DIV_TOP="); PrintHex32(*(volatile uint32_t *)0x1003C510);
    UART_SendString(UART2, " DIV_LBUS="); PrintHex32(*(volatile uint32_t *)0x10034500);
    UART_SendString(UART2, " DIV_RBUS="); PrintHex32(*(volatile uint32_t *)0x10038500); UART_SendString(UART2, "\r\n");
}

/**
 * @brief 打印 UART3 配置寄存器 (CON2 串口1)
 */
void UART3_PrintDebugRegs(void)
{
    UART_SendString(UART2, "GPA1CON="); PrintHex32(*(volatile uint32_t *)0x11400020);
    UART_SendString(UART2, " GPA1DAT="); PrintHex32(*(volatile uint32_t *)0x11400024); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "U3ULCON="); PrintHex32(*(volatile uint32_t *)0x13830000);
    UART_SendString(UART2, " U3UCON="); PrintHex32(*(volatile uint32_t *)0x13830004);
    UART_SendString(UART2, " U3UBRDIV="); PrintHex32(*(volatile uint32_t *)0x13830028);
    UART_SendString(UART2, " U3SLOT="); PrintHex32(*(volatile uint32_t *)0x1383002C); UART_SendString(UART2, "\r\n");
    UART_SendString(UART2, "GATE_PERIL="); PrintHex32(*(volatile uint32_t *)0x1003C950);
    UART_SendString(UART2, " MASK_PERIL0="); PrintHex32(*(volatile uint32_t *)0x1003C350); UART_SendString(UART2, "\r\n");
}
