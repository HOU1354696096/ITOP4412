/**
 * @file    panel.c
 * @brief   屏幕分块状态面板模块实现
 *
 * 布局 (800x1280 竖屏):
 *   - 顶栏 0..63: "Exynos4412 MODULE STATUS"
 *   - 左半边 (x 10..394): UART 串口状态块, 贯通整个内容区 (y 74..1224)
 *   - 右半边 (x 400..789): 从上到下依次 LED / BUZZER / SYSTEM 三个块
 *   - 每个块: 白色 1px 边框 + 黄色标题(scale2) + 内容行(scale2)
 *
 * 刷新策略: 每 300ms 整体重绘 (全屏仅 1M 像素, DDR 带宽充足),
 * 保证任何模块状态变化都能及时显示且不会残留旧文本。
 */
#include "panel.h"
#include "exynos4412_lcd.h"
#include "exynos4412_clock.h"
#include "exynos4412_buzzer.h"
#include "exynos4412_key.h"
#include "exynos4412_uart.h"
#include "led.h"

#define PANEL_REFRESH_MS   200

/*------------------- 分块几何 -------------------*/
#define PANEL_BLOCK_UART2   0
#define PANEL_BLOCK_UART3   1
#define PANEL_BLOCK_KEY     2
#define PANEL_BLOCK_LED     3
#define PANEL_BLOCK_BUZZER  4
#define PANEL_BLOCK_SYS     5
#define PANEL_BLOCK_NUM     6

#define BLOCK_LEFT_X0    10
#define BLOCK_LEFT_X1    394
#define BLOCK_RIGHT_X0   400
#define BLOCK_RIGHT_X1   789
#define BLOCK_TOP        74
#define BLOCK_BOTTOM     1224

typedef struct {
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
    uint32_t bg;
    const char *title;
} PanelBlockDef;

static const PanelBlockDef Blocks[PANEL_BLOCK_NUM] = {
    /* 左半边: 上 UART2 / 下 UART3 */
    { BLOCK_LEFT_X0,  BLOCK_TOP,    BLOCK_LEFT_X1, 640,           0x000020, "UART2 STATUS"  },
    { BLOCK_LEFT_X0,  650,          BLOCK_LEFT_X1, BLOCK_BOTTOM,  0x000030, "UART3 STATUS"  },
    /* 右半边: 从上到下 KEY / LED / BUZZER / SYSTEM */
    { BLOCK_RIGHT_X0, BLOCK_TOP,    BLOCK_RIGHT_X1, 400,           0x101000, "KEY STATUS"    },
    { BLOCK_RIGHT_X0, 410,          BLOCK_RIGHT_X1, 600,           0x002000, "LED STATUS"    },
    { BLOCK_RIGHT_X0, 610,          BLOCK_RIGHT_X1, 860,           0x200000, "BUZZER STATUS" },
    { BLOCK_RIGHT_X0, 870,          BLOCK_RIGHT_X1, BLOCK_BOTTOM,  0x202020, "SYSTEM STATUS" },
};

/*------------------- UART 统计与日志 -------------------*/
#define LOG_LEN     66          /* 每个串口的环形日志长度 (3 行 x 22 字符) */
#define LOG_ROW     22
#define LOG_ROWS    3
static uint32_t Uart2RxCount = 0;   /* UART2 (调试口) 接收计数 */
static uint32_t Uart3RxCount = 0;   /* UART3 (CON2 串口1) 接收计数 */
static uint32_t Uart2TxCount = 0;   /* UART2 心跳打印次数 */
static uint32_t Uart3TxCount = 0;   /* UART3 发送字节数 */
static char     Uart2LastChar = '-';
static char     Uart3LastChar = '-';
static char     Uart2RxLog[LOG_LEN];
static uint8_t  Uart2RxLogIdx = 0;
static uint8_t  Uart2RxLogCnt = 0;
static char     Uart2TxLog[LOG_LEN];
static uint8_t  Uart2TxLogIdx = 0;
static uint8_t  Uart2TxLogCnt = 0;
static char     Uart3RxLog[LOG_LEN];
static uint8_t  Uart3RxLogIdx = 0;
static uint8_t  Uart3RxLogCnt = 0;
static char     Uart3TxLog[LOG_LEN];
static uint8_t  Uart3TxLogIdx = 0;
static uint8_t  Uart3TxLogCnt = 0;

/*------------------- 字符串工具 (无标准库) -------------------*/

/** @brief 无符号整数转十进制字符串, 返回串尾指针 (volatile: 强制字节写) */
static volatile char *U32ToStr(volatile char *dst, uint32_t v)
{
    char tmp[12];
    int i = 0;
    int j;

    do {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v);
    for (j = i - 1; j >= 0; j--) {
        *dst++ = tmp[j];
    }
    *dst = '\0';
    return dst;
}

/** @brief 拼接字符串, 返回串尾指针 (volatile: 强制字节写) */
static volatile char *StrCat(volatile char *dst, const char *src)
{
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
    return dst;
}

/*------------------- 绘制辅助 -------------------*/

/** @brief 重绘一个分块: 背景 + 边框 + 标题 */
static void Panel_BlockRedraw(const PanelBlockDef *b)
{
    LCD_FillRect(b->x0, b->y0, b->x1, b->y1, b->bg);

    /* 1px 白色边框 */
    LCD_FillRect(b->x0, b->y0, b->x1, b->y0, LCD_COLOR_WHITE);
    LCD_FillRect(b->x0, b->y1, b->x1, b->y1, LCD_COLOR_WHITE);
    LCD_FillRect(b->x0, b->y0, b->x0, b->y1, LCD_COLOR_WHITE);
    LCD_FillRect(b->x1, b->y0, b->x1, b->y1, LCD_COLOR_WHITE);

    /* 标题 (scale2: 16x32px) */
    LCD_ShowString_Scaled(b->x0 + 14, b->y0 + 6, b->title, 2,
                          LCD_COLOR_YELLOW, b->bg);

    /* 标题分隔线 */
    LCD_FillRect(b->x0 + 4, b->y0 + 42, b->x1 - 4, b->y0 + 42, 0x606060);
}

/*------------------- 行缓存 (只重画变化行, 避免整块清屏闪烁) -------------------*/
#define PANEL_BLOCK_LINES   22
#define PANEL_LINE_MAX      24
static char    PanelLineCache[PANEL_BLOCK_NUM][PANEL_BLOCK_LINES][PANEL_LINE_MAX];
static uint8_t PanelLineDirty[PANEL_BLOCK_NUM][PANEL_BLOCK_LINES];

/** @brief 字符串比较 (无标准库) */
static uint8_t Panel_StrEq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == *b);
}

/** @brief 字符串拷贝 (限长) */
static void Panel_StrCpy(char *dst, const char *src)
{
    uint8_t n = 0;

    while (*src != '\0' && n < (PANEL_LINE_MAX - 1)) {
        *dst++ = *src++;
        n++;
    }
    *dst = '\0';
}

/**
 * @brief 画一行正文 (scale2), 内容与上次相同则跳过, 不同则只清本行再重画
 */
static void Panel_LineEx(const PanelBlockDef *b, uint8_t block, uint8_t line,
                         const char *s, uint32_t color)
{
    uint16_t y;

    if (line >= PANEL_BLOCK_LINES) {
        return;
    }
    if (PanelLineDirty[block][line] == 0 &&
        Panel_StrEq(s, PanelLineCache[block][line])) {
        return;
    }

    y = b->y0 + 52 + (uint16_t)line * 38;
    /* 只清掉本行区域 (整行背景), 不整块清屏, 消除闪烁 */
    LCD_FillRect(b->x0 + 14, y, b->x1 - 14, y + 31, b->bg);
    LCD_ShowString_Scaled(b->x0 + 14, y, s, 2, color, b->bg);

    Panel_StrCpy(PanelLineCache[block][line], s);
    PanelLineDirty[block][line] = 0;
}

/* 日志行提取函数 (定义在文件底部, 此处前置声明) */
static volatile char *Panel_LogRow(volatile char *dst, const char *log,
                                   uint8_t idx, uint8_t cnt, uint8_t row);

/*------------------- 对外接口 -------------------*/

/**
 * @brief 初始化面板
 */
void Panel_Init(void)
{
    uint8_t i;
    uint8_t j;

    LCD_Clear(LCD_COLOR_BLACK);

    /* 顶栏 */
    LCD_FillRect(0, 0, 799, 63, 0x101010);
    LCD_FillRect(0, 62, 799, 63, LCD_COLOR_WHITE);
    LCD_ShowString_Scaled(112, 8, "Exynos4412 MODULE STATUS", 3,
                          LCD_COLOR_GREEN, 0x101010);

    /* 4 个分块的边框/标题/分隔线, 只画一次 (内容行由 Panel_Update 增量刷新) */
    for (i = 0; i < PANEL_BLOCK_NUM; i++) {
        Panel_BlockRedraw(&Blocks[i]);
    }

    /* 所有内容行标记为"需重画" */
    for (i = 0; i < PANEL_BLOCK_NUM; i++) {
        for (j = 0; j < PANEL_BLOCK_LINES; j++) {
            PanelLineDirty[i][j] = 1;
            PanelLineCache[i][j][0] = '\0';
        }
    }
}

/**
 * @brief 刷新面板 (每 200ms 只重画有变化的内容行)
 */
void Panel_Update(uint32_t nowMs)
{
    static uint32_t LastRefresh = 0;
    static uint8_t  First = 1;
    const PanelBlockDef *b;
    volatile char buf[48];   /* volatile: 防止编译器合并成未对齐的 str/strh */
    volatile char *p;
    uint8_t r;

    if (First == 0 && (nowMs - LastRefresh) < PANEL_REFRESH_MS) {
        return;
    }
    First = 0;
    LastRefresh = nowMs;

    /* ---------- 块 1: UART2 (左上半块) ---------- */
    b = &Blocks[PANEL_BLOCK_UART2];
    Panel_LineEx(b, PANEL_BLOCK_UART2, 0, "UART2: 115200 8N1 OK", LCD_COLOR_GREEN);

    p = StrCat(buf, "TX alive: ");
    p = U32ToStr(p, Uart2TxCount);
    Panel_LineEx(b, PANEL_BLOCK_UART2, 1, (const char *)buf, LCD_COLOR_CYAN);

    p = StrCat(buf, "RX: ");
    p = U32ToStr(p, Uart2RxCount);
    Panel_LineEx(b, PANEL_BLOCK_UART2, 2, (const char *)buf, LCD_COLOR_CYAN);

    p = StrCat(buf, "Last: '");
    *p++ = Uart2LastChar;
    *p++ = '\'';
    *p = '\0';
    Panel_LineEx(b, PANEL_BLOCK_UART2, 3, (const char *)buf, LCD_COLOR_WHITE);

    Panel_LineEx(b, PANEL_BLOCK_UART2, 4, "RX LOG:", LCD_COLOR_GREEN);
    for (r = 0; r < LOG_ROWS; r++) {
        Panel_LogRow(buf, Uart2RxLog, Uart2RxLogIdx, Uart2RxLogCnt, r);
        Panel_LineEx(b, PANEL_BLOCK_UART2, (uint8_t)(5 + r),
                     (const char *)buf, LCD_COLOR_WHITE);
    }

    Panel_LineEx(b, PANEL_BLOCK_UART2, 8, "TX LOG:", LCD_COLOR_CYAN);
    for (r = 0; r < LOG_ROWS; r++) {
        Panel_LogRow(buf, Uart2TxLog, Uart2TxLogIdx, Uart2TxLogCnt, r);
        Panel_LineEx(b, PANEL_BLOCK_UART2, (uint8_t)(9 + r),
                     (const char *)buf, LCD_COLOR_CYAN);
    }

    /* ---------- 块 2: UART3 (左下半块) ---------- */
    b = &Blocks[PANEL_BLOCK_UART3];
    Panel_LineEx(b, PANEL_BLOCK_UART3, 0, "UART3: 115200 8N1 OK", LCD_COLOR_GREEN);

    p = StrCat(buf, "TX bytes: ");
    p = U32ToStr(p, Uart3TxCount);
    Panel_LineEx(b, PANEL_BLOCK_UART3, 1, (const char *)buf, LCD_COLOR_CYAN);

    p = StrCat(buf, "RX: ");
    p = U32ToStr(p, Uart3RxCount);
    Panel_LineEx(b, PANEL_BLOCK_UART3, 2, (const char *)buf, LCD_COLOR_CYAN);

    p = StrCat(buf, "Last: '");
    *p++ = Uart3LastChar;
    *p++ = '\'';
    *p = '\0';
    Panel_LineEx(b, PANEL_BLOCK_UART3, 3, (const char *)buf, LCD_COLOR_WHITE);

    Panel_LineEx(b, PANEL_BLOCK_UART3, 4, "RX LOG:", LCD_COLOR_GREEN);
    for (r = 0; r < LOG_ROWS; r++) {
        Panel_LogRow(buf, Uart3RxLog, Uart3RxLogIdx, Uart3RxLogCnt, r);
        Panel_LineEx(b, PANEL_BLOCK_UART3, (uint8_t)(5 + r),
                     (const char *)buf, LCD_COLOR_WHITE);
    }

    Panel_LineEx(b, PANEL_BLOCK_UART3, 8, "TX LOG:", LCD_COLOR_CYAN);
    for (r = 0; r < LOG_ROWS; r++) {
        Panel_LogRow(buf, Uart3TxLog, Uart3TxLogIdx, Uart3TxLogCnt, r);
        Panel_LineEx(b, PANEL_BLOCK_UART3, (uint8_t)(9 + r),
                     (const char *)buf, LCD_COLOR_CYAN);
    }

    /* ---------- 块 3: KEY (右半上部) ---------- */
    b = &Blocks[PANEL_BLOCK_KEY];
    {
        uint8_t k;
        for (k = 0; k < KEY_NUM; k++) {
            uint8_t down = Key_IsPressed((Key_TypeDef)k);
            p = StrCat(buf, Key_GetName((Key_TypeDef)k));
            *p++ = ':';
            *p++ = ' ';
            p = StrCat(p, down ? "DOWN" : "UP");
            Panel_LineEx(b, PANEL_BLOCK_KEY, k, (const char *)buf,
                         down ? LCD_COLOR_YELLOW : LCD_COLOR_WHITE);
        }
    }

    p = StrCat(buf, "Song: ");
    if (Buzzer_IsPlaying() != SET) {
        p = StrCat(p, "STOP");
    } else {
        p = StrCat(p, Buzzer_GetSongName());
    }
    Panel_LineEx(b, PANEL_BLOCK_KEY, 5, (const char *)buf,
                 Buzzer_IsPlaying() ? LCD_COLOR_GREEN : LCD_COLOR_WHITE);

    p = StrCat(buf, "LED : ");
    p = StrCat(p, LED_IsBlinking() ? "BLINK" : "STOP");
    Panel_LineEx(b, PANEL_BLOCK_KEY, 6, (const char *)buf,
                 LED_IsBlinking() ? LCD_COLOR_GREEN : LCD_COLOR_WHITE);

    /* ---------- 块 4: LED ---------- */
    b = &Blocks[PANEL_BLOCK_LED];
    p = StrCat(buf, "LED1 GPL2_0 : ");
    p = StrCat(p, LED_GetStateString(LED1));
    Panel_LineEx(b, PANEL_BLOCK_LED, 0, (const char *)buf,
                 LED_GetState(LED1) ? LCD_COLOR_RED : LCD_COLOR_WHITE);

    p = StrCat(buf, "LED2 GPK1_1 : ");
    p = StrCat(p, LED_GetStateString(LED2));
    Panel_LineEx(b, PANEL_BLOCK_LED, 1, (const char *)buf,
                 LED_GetState(LED2) ? LCD_COLOR_RED : LCD_COLOR_WHITE);

    p = StrCat(buf, "Blink: ");
    p = StrCat(p, LED_IsBlinking() ? "ON" : "OFF");
    Panel_LineEx(b, PANEL_BLOCK_LED, 2, (const char *)buf, LCD_COLOR_GREEN);

    /* ---------- 块 5: 蜂鸣器 ---------- */
    b = &Blocks[PANEL_BLOCK_BUZZER];
    p = StrCat(buf, "State: ");
    p = StrCat(p, Buzzer_GetStateString());
    Panel_LineEx(b, PANEL_BLOCK_BUZZER, 0, (const char *)buf,
                 Buzzer_IsPlaying() ? LCD_COLOR_GREEN : LCD_COLOR_WHITE);

    p = StrCat(buf, "Song: ");
    p = StrCat(p, Buzzer_GetSongName());
    Panel_LineEx(b, PANEL_BLOCK_BUZZER, 1, (const char *)buf, LCD_COLOR_CYAN);

    p = StrCat(buf, "Note: ");
    p = U32ToStr(p, (uint32_t)(Buzzer_GetNoteIndex() + 1));
    *p++ = '/';
    p = U32ToStr(p, (uint32_t)Buzzer_GetSongLength());
    Panel_LineEx(b, PANEL_BLOCK_BUZZER, 2, (const char *)buf, LCD_COLOR_WHITE);

    p = StrCat(buf, "Freq: ");
    p = U32ToStr(p, (uint32_t)Buzzer_GetNoteFreq());
    p = StrCat(p, " Hz");
    Panel_LineEx(b, PANEL_BLOCK_BUZZER, 3, (const char *)buf, LCD_COLOR_CYAN);

    p = StrCat(buf, "Beep: ");
    p = U32ToStr(p, (uint32_t)Buzzer_GetNoteDuration());
    p = StrCat(p, " ms (PWM0)");
    Panel_LineEx(b, PANEL_BLOCK_BUZZER, 4, (const char *)buf, LCD_COLOR_WHITE);

    /* ---------- 块 6: 系统 ---------- */
    b = &Blocks[PANEL_BLOCK_SYS];
    p = StrCat(buf, "Uptime: ");
    p = U32ToStr(p, nowMs / 1000);
    p = StrCat(p, " s");
    Panel_LineEx(b, PANEL_BLOCK_SYS, 0, (const char *)buf, LCD_COLOR_GREEN);

    Panel_LineEx(b, PANEL_BLOCK_SYS, 1, "Main: DDR 0x43E00000", LCD_COLOR_WHITE);
    Panel_LineEx(b, PANEL_BLOCK_SYS, 2, "LCD : 800x1280 LVDS", LCD_COLOR_WHITE);
    Panel_LineEx(b, PANEL_BLOCK_SYS, 3, "Keys: 5 active-low", LCD_COLOR_CYAN);
}

/**
 * @brief 把字符写入环形日志缓冲 (可打印字符原样保存, 其余用 '.' 代替)
 */
static void Panel_LogChar(char ch, char *log, uint8_t *idx, uint8_t *cnt)
{
    char c = ((ch >= 0x20) && (ch < 0x7F)) ? ch : '.';

    log[*idx] = c;
    *idx = (uint8_t)((*idx + 1) % LOG_LEN);
    if (*cnt < LOG_LEN) {
        (*cnt)++;
    }
}

/**
 * @brief 把环形日志的第 row 行写入 dst (每行 LOG_ROW 字符, 不足补空格贴满右边界)
 * @param row 行号 0..LOG_ROWS-1 (0=最早)
 */
static volatile char *Panel_LogRow(volatile char *dst, const char *log,
                                   uint8_t idx, uint8_t cnt, uint8_t row)
{
    uint8_t base;
    uint8_t start;
    uint8_t n;
    uint8_t k;

    /* 未写满时数据从 0 线性排列; 写满后 idx 指向最旧字符 */
    base = (cnt >= LOG_LEN) ? idx : 0;
    n = (cnt > (uint8_t)(row * LOG_ROW)) ? (uint8_t)(cnt - row * LOG_ROW) : 0;
    if (n > LOG_ROW) {
        n = LOG_ROW;
    }
    start = (uint8_t)((base + (uint8_t)(row * LOG_ROW)) % LOG_LEN);
    for (k = 0; k < n; k++) {
        *dst++ = log[(uint8_t)((start + k) % LOG_LEN)];
    }
    /* 补齐空格, 让每一行都顶到右边界, 视觉上互不串扰 */
    for (; n < LOG_ROW; n++) {
        *dst++ = ' ';
    }
    *dst = '\0';
    return dst;
}

/**
 * @brief 发送通知回调 (覆盖 UART 库中的弱符号): 按端口记录发出的字符
 * @note  UART2 只记板子自己发出的内容 (回显已被主程序屏蔽);
 *        UART3 记全部发送 (含 CON2 回显, 因为回显就是它的主要发送)
 */
void UART_TxNotify(UART_TypeDef *UARTx, uint8_t ch)
{
    if (UARTx == UART2) {
        Panel_LogChar((char)ch, Uart2TxLog, &Uart2TxLogIdx, &Uart2TxLogCnt);
    } else if (UARTx == UART3) {
        Uart3TxCount++;
        Panel_LogChar((char)ch, Uart3TxLog, &Uart3TxLogIdx, &Uart3TxLogCnt);
    }
}

/**
 * @brief 记录 UART3 (CON2 串口1) 收到的字符, 并累加 UART3 接收计数
 */
void Panel_NotifyUartRx(char ch)
{
    Uart3RxCount++;
    Uart3LastChar = ((ch >= 0x20) && (ch < 0x7F)) ? ch : '.';
    Panel_LogChar(ch, Uart3RxLog, &Uart3RxLogIdx, &Uart3RxLogCnt);
}

/**
 * @brief 记录 UART2 (调试口) 收到的字符, 并累加 UART2 接收计数
 */
void Panel_NotifyUart2Rx(char ch)
{
    Uart2RxCount++;
    Uart2LastChar = ((ch >= 0x20) && (ch < 0x7F)) ? ch : '.';
    Panel_LogChar(ch, Uart2RxLog, &Uart2RxLogIdx, &Uart2RxLogCnt);
}

/**
 * @brief 记录 UART2 心跳打印次数
 */
void Panel_NotifyUartTx(void)
{
    Uart2TxCount++;
}

/**
 * @brief 获取 UART3 (CON2 串口1) 已接收字符计数 (供主程序串口心跳打印)
 */
uint32_t Panel_GetUartRxCount(void)
{
    return Uart3RxCount;
}

/**
 * @brief 获取 UART2 调试口已接收字符计数 (供主程序串口心跳打印)
 */
uint32_t Panel_GetUart2RxCount(void)
{
    return Uart2RxCount;
}
