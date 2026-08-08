/**
 * @file    exynos4412_uart.c
 * @brief   Exynos4412 UART 库实现（STM32 库风格）
 *
 * UCON 采用三星/迅为已验证的标准值 0x3C5:
 *   [1:0]=01 RX 模式, [3:2]=01 TX 模式, [6]=1 RX 超时使能,
 *   [9:8]=11, [11:10]=11 时钟源 = SCLK_UART (100MHz)
 */
#include "exynos4412_uart.h"
#include "exynos4412_clock.h"
#include "exynos4412_gpio.h"

/* UART 时钟门控 (CMU, 与内核 clock-exynos4.c 一致):
 *   CLK_GATE_IP_PERIL[0:3]  = UART0~UART3 IP 时钟
 *   CLK_SRC_MASK_PERIL0     = UART0~3 源时钟: bit0/bit4/bit8/bit12 */
#define CLK_GATE_IP_PERIL     0x1003C950UL
#define CLK_SRC_MASK_PERIL0   0x1003C350UL

/**
 * @brief 发送通知弱回调 (STM32 风格钩子)
 * @note  应用层(如 User/App/panel.c)可定义同名强符号覆盖, 用于 TX LOG 显示;
 *        默认空实现, 不影响任何现有调用。
 */
__attribute__((weak)) void UART_TxNotify(UART_TypeDef *UARTx, uint8_t ch)
{
    (void)UARTx;
    (void)ch;
}

/**
 * @brief 发送通知屏蔽标志
 * @note  置 1 后 UART_SendData 不触发 UART_TxNotify (用于"回显"等不希望
 *        计入 TX LOG 的场景, 例如把上位机发来的字符原样弹回时)
 */
uint8_t UART_TxNotifyMask = 0;

/**
 * @brief  打开/关闭 UART 外设时钟 (STM32 库风格 RCC 使能)
 * @param  UARTx   UART0~UART3
 * @param  Enable  1 = 开时钟, 0 = 关时钟
 */
void UART_ClockCmd(UART_TypeDef *UARTx, uint8_t Enable)
{
    uint32_t gate_bit = 0;
    uint32_t src_bit  = 0;

    if (UARTx == UART0) {
        gate_bit = (1u << 0);
        src_bit  = (1u << 0);
    } else if (UARTx == UART1) {
        gate_bit = (1u << 1);
        src_bit  = (1u << 4);
    } else if (UARTx == UART2) {
        gate_bit = (1u << 2);
        src_bit  = (1u << 8);
    } else if (UARTx == UART3) {
        gate_bit = (1u << 3);
        src_bit  = (1u << 12);
    } else {
        return;
    }

    if (Enable) {
        *(volatile uint32_t *)CLK_GATE_IP_PERIL   |= gate_bit;
        *(volatile uint32_t *)CLK_SRC_MASK_PERIL0 |= src_bit;
    } else {
        *(volatile uint32_t *)CLK_GATE_IP_PERIL   &= ~gate_bit;
        *(volatile uint32_t *)CLK_SRC_MASK_PERIL0 &= ~src_bit;
    }
}

/**
 * @brief  配置 UART 对应引脚为复用功能 (AF2)
 * @param  UARTx  UART0~UART3
 * @note   引脚表 (Exynos4412 用户手册 4.3.2):
 *           UART0: GPA0_0(RXD)/GPA0_1(TXD)
 *           UART1: GPA0_4(RXD)/GPA0_5(TXD)
 *           UART2: GPA1_0(RXD)/GPA1_1(TXD)
 *           UART3: GPA1_4(RXD)/GPA1_5(TXD)
 */
void UART_GPIO_Init(UART_TypeDef *UARTx)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_AF    = 2;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;

    if (UARTx == UART0) {
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
        GPIO_Init(GPA0, &GPIO_InitStructure);
    } else if (UARTx == UART1) {
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
        GPIO_Init(GPA0, &GPIO_InitStructure);
    } else if (UARTx == UART2) {
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
        GPIO_Init(GPA1, &GPIO_InitStructure);
    } else if (UARTx == UART3) {
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
        GPIO_Init(GPA1, &GPIO_InitStructure);
    }
}

/**
 * @brief  初始化 UART: 帧格式 + 时钟源 + 波特率
 * @param  UARTx              UART0~UART3
 * @param  UART_InitStruct    初始化参数
 */
void UART_Init(UART_TypeDef *UARTx, UART_InitTypeDef *UART_InitStruct)
{
    uint32_t ucon      = 0;
    uint32_t ubrdiv    = 0;
    uint32_t slot      = 0;
    uint32_t divisor   = 0;

    /* 0. 打开 UART 外设时钟 (IP 门控 + 源时钟) */
    UART_ClockCmd(UARTx, 1);

    /* 1. 帧格式: 数据位 + 停止位 + 校验 */
    UARTx->ULCON = (UART_InitStruct->UART_WordLength & 0x03)
                 | ((UART_InitStruct->UART_StopBits & 0x03) << 2)
                 | (UART_InitStruct->UART_Parity & 0x60);

    /* 2. 使能 FIFO (0x111), 与迅为已验证例程逐字节一致; 关闭流控 */
    UARTx->UFCON = 0x111;
    UARTx->UMCON = 0x00;

    /* 3. 控制寄存器: 标准三星配置 0x3C5 (SCLK_UART 时钟源) */
    ucon = 0x3C5;
    if (UART_InitStruct->UART_ClockSource == UART_ClockSource_PCLK) {
        ucon &= ~(0x3 << 10);   /* 时钟源改为 PCLK */
    }
    UARTx->UCON = ucon;

    /* 4. 波特率:
     *    UBRDIV  = UART时钟 / (波特率 * 16) - 1
     *    UDIVSLOT = 分频余数折算值(三星公式), 与已验证例程 UFRACVAL=0x4 一致 */
    divisor = UART_InitStruct->UART_Clock / (UART_InitStruct->UART_BaudRate * 16);
    ubrdiv = (divisor > 1) ? (divisor - 1) : 1;

    slot = ((UART_InitStruct->UART_Clock % (UART_InitStruct->UART_BaudRate * 16)) * 16)
         / (UART_InitStruct->UART_BaudRate * 16);

    UARTx->UBRDIV   = ubrdiv;
    UARTx->UDIVSLOT = slot;
}

/**
 * @brief 发送一个字节 (盲写, 与已验证汇编例程一致)
 *
 * 注意: 之前尝试等待 UTRSTAT/UFSTAT 状态标志都会导致发送卡死, 采用盲写。
 * 每个字节后加一个节拍延时, 让数据按线路速率(115200, 每字节约 87us)
 * 均匀发送, 避免突发导致 PL2303 仿冒串口线丢字节。
 * 2026-08-08: CPU 提到 800MHz 后 0x4000(约 82us) 已略快于线路速率,
 * 会造成仿冒线丢字节(启动横幅乱码)。改为 0x9000(约 185us) 留足余量。
 */
void UART_SendData(UART_TypeDef *UARTx, uint16_t Data)
{
    /* 通知应用层: 本字节已发送 (用于 TX LOG) */
    if (UART_TxNotifyMask == 0) {
        UART_TxNotify(UARTx, (uint8_t)(Data & 0xFF));
    }

    /* 盲写发送 */
    UARTx->UTXH = Data & 0xFF;
    System_Delay(0x9000);   /* 约 185us (800MHz 主频), 慢于 115200 字节间隔, 防丢字节 */
}

/**
 * @brief 接收一个字节 (查询 RXNE 标志, 阻塞)
 * @retval 接收到的数据
 */
uint16_t UART_ReceiveData(UART_TypeDef *UARTx)
{
    /* 等待接收数据就绪 */
    while (UART_GetFlagStatus(UARTx, UART_FLAG_RXNE) == RESET) {
    }
    return (uint16_t)(UARTx->URXH & 0xFF);
}

/**
 * @brief 发送字符串 (以 '\0' 结束)
 */
void UART_SendString(UART_TypeDef *UARTx, const char *str)
{
    while (*str != '\0') {
        UART_SendData(UARTx, (uint16_t)(*str));
        str++;
    }
}

/**
 * @brief 查询 UART 状态标志
 * @retval SET / RESET
 */
FlagStatus UART_GetFlagStatus(UART_TypeDef *UARTx, uint16_t UART_FLAG)
{
    return (UARTx->UTRSTAT & UART_FLAG) ? SET : RESET;
}
