/**
 * @file    main.c
 * @brief   Exynos4412 主程序 (从 DDR 0x43E00000 运行, STM32 库风格)
 *
 * 功能:
 *   1. 初始化 UART2 (GPA1_0/GPA1_1 调试串口) 与 UART3 (GPA1_4/GPA1_5,
 *      底板 CON2 "串口1"), 115200 8N1
 *   2. 初始化 LVDS-LCD (7.0 寸 IPS 800x1280 电容屏)
 *   3. 分栏状态面板: 左半 UART2/UART3, 右半 KEY/LED/BUZZER/SYSTEM
 *   4. 蜂鸣器内置 3 首歌循环播放, 5 个按键切换/停止歌曲、控制 LED
 *   5. UART3 收到字符立即回显, 并打印歌曲名等系统信息
 *
 * 模块划分:
 *   - Libraries/      : 芯片外设库 (GPIO/UART/LCD/时钟/蜂鸣器/按键)
 *   - User/App/led.c  : LED 应用模块
 *   - User/App/panel.c: 屏幕状态面板模块
 *   - User/App/debug.c: 寄存器调试打印模块
 *
 * 本文由 BL2 从 SD 卡搬运到 DDR 后跳转执行:
 *   - 代码/字模/显存全部使用 DDR, 不受 IRAM 14KB 限制
 *   - 显存固定 0x50000000 (800*1280*4 ≈ 4MB)
 */
#include "exynos4412.h"
#include "exynos4412_gpio.h"
#include "exynos4412_uart.h"
#include "exynos4412_lcd.h"
#include "exynos4412_clock.h"
#include "exynos4412_buzzer.h"
#include "exynos4412_key.h"
#include "led.h"
#include "panel.h"
#include "debug.h"

int main(void)
{
    UART_InitTypeDef UART_InitStructure;
    uint32_t lastAlive = 0;

    /* 1. 公共 UART 参数: 115200 8N1, SCLK_UART = 100MHz */
    UART_InitStructure.UART_BaudRate    = 115200;
    UART_InitStructure.UART_WordLength  = UART_WordLength_8b;
    UART_InitStructure.UART_StopBits    = UART_StopBits_1;
    UART_InitStructure.UART_Parity      = UART_Parity_No;
    UART_InitStructure.UART_Mode        = UART_Mode_Tx_Rx;
    UART_InitStructure.UART_Clock       = EXYNOS4412_SCLK_UART_HZ;
    UART_InitStructure.UART_ClockSource = UART_ClockSource_SCLK;

    /* BL2 阶段串口是盲写 FIFO, 先等尾巴发完再重新初始化 */
    System_Delay(0x800000);

    /* 2. UART2 (调试 DB9) + UART3 (CON2 串口1) */
    UART_GPIO_Init(UART2);
    UART_Init(UART2, &UART_InitStructure);

    UART_GPIO_Init(UART3);
    UART_Init(UART3, &UART_InitStructure);

    UART_SendString(UART2, "\r\n======== UART2 DEBUG PORT (GPA1_0/GPA1_1) ========\r\n");
    UART_SendString(UART2, "Main running from DDR @ 0x43E00000\r\n");

    UART_SendString(UART3, "\r\n======== UART3 PORT (GPA1_4/GPA1_5) = CON2 SERIAL1 ========\r\n");
    UART_SendString(UART3, "UART3 TX OK @115200 8N1, type chars to echo\r\n");

    /* 3. LVDS-LCD */
    UART_SendString(UART2, "Init LCD (LVDS 800x1280)...\r\n");
    LCD_Init();
    UART_SendString(UART2, "LCD: OK\r\n");

    /* 4. 系统毫秒节拍 (PWM 定时器2, 供蜂鸣器/LED/面板计时) */
    System_TickInit();

    /* 5. 外设与应用模块初始化 */
    Buzzer_Init();      /* GPD0_0 = TOUT_0, PWM0 */
    LED_Init();         /* GPL2_0 / GPK1_1 */
    Key_Init();         /* VOL-/VOL+/SLEEP/BACK/HOME */
    Panel_Init();       /* 分块状态面板 */

    /* 6. 寄存器调试信息 (排查用) */
    LCD_PrintDebugRegs();
    UART3_PrintDebugRegs();

    /* 8. 非阻塞循环播放第 1 首《祝你生日快乐》(PWM 变调, 无源蜂鸣器已确认) */
    Buzzer_PlaySongByIndex(0);

    UART_SendString(UART2, "UART3 echo ready: type chars on CON2 (SERIAL1)\r\n");
    UART_SendString(UART3, "System: Exynos4412 ready\r\n");
    UART_SendString(UART3, "SONG: HAPPY BIRTHDAY\r\n");

    /* 9. 主循环: 非阻塞调度 */
    while (1) {
        uint32_t now = System_GetMs();
        static uint32_t loopCnt = 0;
        static uint32_t lastRaw = 0;

        loopCnt++;

        /* 节拍看门狗: 原始计数器 500ms 未变化 -> 节拍硬件已停, 重新初始化 */
        {
            uint32_t raw = System_GetTickRaw();
            static uint32_t rawStall = 0;
            if (raw == lastRaw) {
                if (++rawStall >= 5000000) {   /* 500 万次循环未变化 (不依赖 tick) */
                    UART_SendString(UART2, "TICK FROZEN, reinit\r\n");
                    System_TickInit();
                    lastRaw = System_GetTickRaw();
                    rawStall = 0;
                }
            } else {
                lastRaw = raw;
                rawStall = 0;
            }
        }

        /* UART2 调试口: 收到字符后回显(便于确认链路)并写入屏幕 RX LOG
         * 回显不触发 TX LOG (上位机发来的数据应只出现在 RX LOG 中) */
        if (UART_GetFlagStatus(UART2, UART_FLAG_RXNE) == SET) {
            uint8_t ch = (uint8_t)UART_ReceiveData(UART2);
            UART_TxNotifyMask = 1;
            UART_SendData(UART2, ch);
            UART_TxNotifyMask = 0;
            Panel_NotifyUart2Rx((char)ch);
        }

        /* UART3 回显 (CON2 串口1): 回显是它的主要发送, 计入 UART3 TX LOG */
        if (UART_GetFlagStatus(UART3, UART_FLAG_RXNE) == SET) {
            uint8_t ch = (uint8_t)UART_ReceiveData(UART3);
            UART_SendData(UART3, ch);
            Panel_NotifyUartRx((char)ch);
        }

        /* 按键扫描与功能 (VOL-上一曲 / VOL+下一曲 / SLEEP停止 / BACK LED闪 / HOME停LED) */
        {
            Key_TypeDef key = Key_Scan(now);
            if (key != KEY_NUM) {
                UART_SendString(UART3, "KEY: ");
                UART_SendString(UART3, Key_GetName(key));
                UART_SendString(UART3, "\r\n");
            }
            switch (key) {
            case KEY_VOL_DOWN:
                Buzzer_PrevSong();
                UART_SendString(UART3, "SONG: ");
                UART_SendString(UART3, Buzzer_GetSongName());
                UART_SendString(UART3, "\r\n");
                break;
            case KEY_VOL_UP:
                Buzzer_NextSong();
                UART_SendString(UART3, "SONG: ");
                UART_SendString(UART3, Buzzer_GetSongName());
                UART_SendString(UART3, "\r\n");
                break;
            case KEY_SLEEP:
                Buzzer_StopSong();
                UART_SendString(UART3, "SONG: STOP\r\n");
                break;
            case KEY_BACK:
                LED_BlinkEnable();
                UART_SendString(UART3, "LED: BLINK ON\r\n");
                break;
            case KEY_HOME:
                LED_BlinkDisable();
                UART_SendString(UART3, "LED: BLINK OFF\r\n");
                break;
            default:
                break;
            }
        }

        /* 非阻塞模块调度 */
        LED_BlinkTick(now);     /* LED 交替闪烁 */
        Buzzer_Tick(now);       /* 歌曲推进 */
        Panel_Update(now);      /* 状态面板刷新 */

        /* UART2 心跳: 每秒一条 (含 U2/U3 接收计数, 便于核对屏幕 RX 显示) */
        if ((now - lastAlive) >= 1000) {
            lastAlive = now;
            UART_SendString(UART2, "alive | U2 rx=");
            PrintHex32(Panel_GetUart2RxCount());
            UART_SendString(UART2, " U3 rx=");
            PrintHex32(Panel_GetUartRxCount());
            UART_SendString(UART2, " | BZ ");
            PrintHex8((uint8_t)(Buzzer_GetNoteIndex() + 1));
            UART_SendString(UART2, "/");
            PrintHex8((uint8_t)Buzzer_GetSongLength());
            UART_SendString(UART2, " f=");
            PrintHex16(Buzzer_GetNoteFreq());
            UART_SendString(UART2, "\r\n");
            Panel_NotifyUartTx();
        }
    }

    return 0;
}
