/**
 * @file    exynos4412_lcd.c
 * @brief   Exynos4412 FIMD + LVDS-LCD 驱动实现 (STM32 库风格)
 *
 * 参考:
 *   - 迅为 iTOP-4412 精英版使用手册 71.6 节 (背光 GPL0_4 / LVDS 桥 GPL1_0)
 *   - 迅为 topeet_LVDS_7_0.dtsi (7.0 寸 800x1280 时序)
 *   - Exynos 4412 SCP 手册第 41 章 (FIMD 寄存器定义)
 *   - U-Boot exynos_fb.c (Exynos4 FIMD 初始化流程)
 *   - 内核 drivers/video/samsung/s3cfb_fimd6x.c (4412 实际使用的 FIMD 驱动:
 *     VIDCON2 写 WB_DISABLE, WINSHMAP 通道使能, WINCON0 显式配置
 *     DATAPATH_DMA + BURSTLEN_16WORD + BPPMODE_24BPP_888)
 *   - 内核 arch/arm/mach-exynos/setup-fb-s5p.c (iTop 精英版/POP 板:
 *     GPF0~GPF3 复用功能 2 且驱动强度 LV4, GPL0_4=BK_VDD_EN,
 *     GPL1_0=LCD_PWDN, FIMD bypass 寄存器 0x10010210 bit1)
 *   - 内核 drivers/pwm/pwm-samsung.c (Exynos 系列 TCON 位定义:
 *     Timer1 的 start=bit8 / manual=bit9 / invert=bit10 / auto=bit11)
 *
 * 时钟链路:
 *   MPLL(800MHz) --FIMD0_RATIO(CLK_DIV_LCD0[3:0]=1)--> SCLK_FIMD(400MHz)
 *   SCLK_FIMD --CLKVAL(VIDCON0[13:6]=5)--> VCLK = 400/(5+1) = 66.7MHz
 *   (SCP 手册 41 章: VCLK = SCLK_FIMD/(CLKVAL+1), CLKVAL>=1, VCLK 最大 80MHz;
 *    800x1280 采用 CLAA070WP03XG 规格 66.77MHz 附近, 取 CLKVAL=5)
 */
#include "exynos4412_lcd.h"
#include "exynos4412_gpio.h"
#include "exynos4412_clock.h"
#include "exynos4412_font8x16.h"

/*------------------- FIMD 寄存器 (基址 0x11C00000) -------------------*/
#define FIMD_VIDCON0        0x11C00000UL
#define FIMD_VIDCON1        0x11C00004UL
#define FIMD_VIDCON2        0x11C00008UL
#define FIMD_VIDTCON0       0x11C00010UL
#define FIMD_VIDTCON1       0x11C00014UL
#define FIMD_VIDTCON2       0x11C00018UL
#define FIMD_WINCON0        0x11C00020UL
#define FIMD_WINSHMAP       0x11C00034UL
#define FIMD_VIDOSD0A       0x11C00040UL
#define FIMD_VIDOSD0B       0x11C00044UL
#define FIMD_VIDOSD0C       0x11C00048UL
#define FIMD_VIDW00ADD0B0   0x11C000A0UL
#define FIMD_VIDW00ADD1B0   0x11C000D0UL
#define FIMD_VIDW00ADD2     0x11C00100UL

/*------------------- CMU 时钟 / 系统寄存器 -------------------*/
#define CLK_SRC_LCD0        0x1003C234UL   /* FIMD0_SEL [3:0] */
#define CLK_DIV_LCD0        0x1003C534UL   /* FIMD0_RATIO [3:0] */
#define CLK_GATE_IP_LCD0    0x1003C934UL   /* CLK_FIMD0 [0] */
#define CLK_GATE_IP_PERIL   0x1003C950UL   /* "timers"(PWM 定时器) 门控 [24] */
#define CLK_GATE_BLOCK      0x1003C970UL   /* CLK_LCD0 [4] */
#define LCDBLK_CFG          0x10010210UL   /* FIMDBYPASS_LBLK0 [1] */

/*------------------- PWM1 背光 (GPD0_1 = TOUT_1) -------------------*/
#define PWM_TCFG0           0x139D0000UL
#define PWM_TCFG1           0x139D0004UL
#define PWM_TCON            0x139D0008UL
#define PWM_TCNTB1          0x139D0018UL
#define PWM_TCMPB1          0x139D001CUL

/*------------------- 内部寄存器读写辅助 -------------------*/
static inline uint32_t LCD_ReadReg(uint32_t reg)
{
    return *(volatile uint32_t *)reg;
}

static inline void LCD_WriteReg(uint32_t reg, uint32_t val)
{
    *(volatile uint32_t *)reg = val;
}

static inline void LCD_SetBits(uint32_t reg, uint32_t mask)
{
    LCD_WriteReg(reg, LCD_ReadReg(reg) | mask);
}

/*------------------- 私有函数 -------------------*/

/**
 * @brief  背光与 LVDS 桥电源使能
 * @note   对应内核 arch/arm/mach-exynos/setup-fb-s5p.c 的 iTop 精英/POP 板配置:
 *         - GPL0_4 = BK_VDD_EN (背光电源, PMOS 控制, 高电平亮)
 *         - GPL1_0 = LCD_PWDN (GM8285C LVDS 发送器 SHTDN, 高电平工作)
 *         - GPL0_2 = TP1_EN (触摸电平转换使能, 高电平)
 *         - GPD0_1 = TOUT_1 (PWM1 背光亮度, 复用功能 2)
 *         背光 PWM 频率约 9kHz, 占空比 88% (对应内核默认亮度级别 6/7)
 */
static void LCD_PowerOn(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 1. 背光电源 (BK_VDD_EN) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_AF    = 0;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;
    GPIO_Init(GPL0, &GPIO_InitStructure);
    GPIO_SetBits(GPL0, GPIO_Pin_4);

    /* 2. LVDS 发送器使能 (LCD_PWDN) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_Init(GPL1, &GPIO_InitStructure);
    GPIO_SetBits(GPL1, GPIO_Pin_0);

    /* 3. 触摸电平转换使能 (TP1_EN) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_Init(GPL0, &GPIO_InitStructure);
    GPIO_SetBits(GPL0, GPIO_Pin_2);

    /* 4. PWM 背光输出: GPD0_1 = TOUT_1 (手册 71.6.2.1, pwm1 通道)
     *    时序: TCFG0 预分频 49(50分频), TCFG1 MUX1=1/8, TCNTB=25 -> 约 10kHz
     *       TCMPB=22 -> 88% 占空比 (高占空比 = 亮) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_AF    = 2;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;
    GPIO_Init(GPD0, &GPIO_InitStructure);

    /* 打开 PWM 定时器时钟门控 (内核 clock-exynos4.c 中 "timers" 时钟,
     * CLK_GATE_IP_PERIL[24], 复位默认开, 这里显式置 1 保险) */
    LCD_SetBits(CLK_GATE_IP_PERIL, 1u << 24);

    LCD_WriteReg(PWM_TCFG0, 49u);                       /* 预分频 50 */
    /* TCFG1 的 MUX1 在 [7:4] (SCP 手册 24.5.1.2), = 3 即 1/8 分频
     * 注意: 之前误写成 [11:8](MUX2), 会导致 PWM1 频率异常 */
    LCD_WriteReg(PWM_TCFG1, (LCD_ReadReg(PWM_TCFG1) & ~0x00F0u) | (0x3u << 4));

    /* 通道 1 (背光): Exynos4412 TCON: ch1 start=bit8, manual=bit9,
     * inv=bit10, auto=bit11 (SCP 手册 24.5.1.3)。
     * 装载顺序必须与 u-boot timer.c / 内核 pwm_config 一致:
     *   先写 TCNTB/TCMPB -> 再置手动更新位(bit9)把缓冲装进计数器
     *   -> 清手动更新 -> 置启动(bit8) + 自动重载(bit11)。
     * 之前先置手动更新位再写 TCNTB, 若装载发生在手动更新位上升沿,
     * 计数器只会装进复位值 0, 导致 TCNTO 一直为 0。 */
    LCD_WriteReg(PWM_TCNTB1, 25u);                      /* 周期 */
    LCD_WriteReg(PWM_TCMPB1, 22u);                      /* 占空比 88% */

    /* 自动重载 + 手动更新: 把 TCNTB1/TCMPB1 装入计数器 */
    LCD_WriteReg(PWM_TCON, (LCD_ReadReg(PWM_TCON) & ~0x0F00u)
                           | (1u << 9) | (1u << 11));
    System_Delay(0x1000);                               /* 等装载完成 */

    /* 启动 + 保持自动重载, 清除手动更新 */
    LCD_WriteReg(PWM_TCON, (LCD_ReadReg(PWM_TCON) & ~(1u << 9))
                           | (1u << 8) | (1u << 11));

    /* 5. 等电源/LVDS 桥稳定 (内核 setup-fb-s5p.c 中 BK_VDD 后延时 100ms) */
    System_Delay(0x80000);
}

/**
 * @brief  配置 FIMD/LCD 时钟与系统显示通路
 * @note   FIMD0_SEL=0x6(SCLK_MPLL=800MHz), FIMD0_RATIO=1 -> SCLK_FIMD=400MHz
 */
static void LCD_ClockInit(void)
{
    /* 打开 LCD0 块时钟门 (CLK_GATE_BLOCK[4]) 与 FIMD0 外设时钟 (CLK_GATE_IP_LCD0[0]) */
    LCD_SetBits(CLK_GATE_BLOCK,   1u << 4);
    LCD_SetBits(CLK_GATE_IP_LCD0, 1u << 0);

    /* FIMD0 时钟源 = SCLK_MPLL (0x6) */
    LCD_WriteReg(CLK_SRC_LCD0, (LCD_ReadReg(CLK_SRC_LCD0) & ~0x0F) | 0x06);

    /* FIMD0_RATIO = 1 -> SCLK_FIMD = MPLL / 2 = 400MHz */
    LCD_WriteReg(CLK_DIV_LCD0, (LCD_ReadReg(CLK_DIV_LCD0) & ~0x0F) | 0x01);

    /* 系统寄存器: FIMD 输出旁路 MIE/MDNIE, 直接到 RGB 引脚 (FIMDBYPASS_LBLK0[1]=1) */
    LCD_SetBits(LCDBLK_CFG, 1u << 1);
}

/**
 * @brief  配置 LCD 数据引脚 GPF0~GPF3 为 FIMD 复用功能
 * @note   与内核 setup-fb-s5p.c 一致:
 *         GPF0/1/2 全部 8 脚, GPF3 只用低 4 脚, 复用功能号均为 2,
 *         驱动强度全部 LV4 (内核 iTop 精英/POP 板用的 S5P_GPIO_DRVSTR_LV4,
 *         50MHz 像素时钟下信号完整性更好; 之前用 LV1 可能造成画面异常)
 */
static void LCD_GpioInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_AF    = 2;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV4;

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_All;
    GPIO_Init(GPF0, &GPIO_InitStructure);
    GPIO_Init(GPF1, &GPIO_InitStructure);
    GPIO_Init(GPF2, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
                                    GPIO_Pin_3;
    GPIO_Init(GPF3, &GPIO_InitStructure);
}

/*------------------- 对外接口 -------------------*/

/**
 * @brief  初始化 FIMD 并点亮 LVDS-LCD (7.0 寸 800x1280, 66.7MHz)
 */
void LCD_Init(void)
{
    uint32_t vidcon0;

    /* 1. 背光 + LVDS 桥电源 */
    LCD_PowerOn();

    /* 2. FIMD/LCD 时钟 */
    LCD_ClockInit();

    /* 3. GPIO 复用 */
    LCD_GpioInit();

    /* 4. 先关显示, 配置 FIMD (VIDCON0 = CLKVAL 5 << 6, VCLK=66.7MHz)
     *    SCP 手册 41-74: CLKVAL_F[13:6], VCLK = SCLK_FIMD/(CLKVAL+1),
     *    最大 80MHz; 4412 的 VIDCON0 无 CLKSEL/CLKDIR 位, [4:2] 保留置 0 */
    vidcon0 = (5u << 6);
    LCD_WriteReg(FIMD_VIDCON0, vidcon0);

    /* 输出路径: RGB 直通, 关闭写回/电视格式 (与内核 s3cfb_set_output 一致) */
    LCD_WriteReg(FIMD_VIDCON2, 0u);

    /* 极性 (与 Android 3.0 内核 s3cfb_wa101s.c 中 7.0 寸配置一致):
     *   FIXVCLK[10:9]=01  VCLK 正常运行 (数据欠载时不停钟, SCP 41-75)
     *   IVCLK[7]=1        数据在 VCLK 上升沿采样 (rise_vclk=1)
     *   IVSYNC[5]=1       VSYNC 反相 (inv_vsync=1)
     *   IVDEN[4]=0        DE 正常高有效 (inv_vden=0)  -> 0x2A0
     * 注意: 18bit LVDS 只传 DE, HSYNC/VSYNC 不到面板, 但保持与内核一致 */
    LCD_WriteReg(FIMD_VIDCON1, (1u << 9) | (1u << 7) | (1u << 5));

    /* 垂直时序 (Android wa101s 7.0): VBPD=10, VFPD=12, VSPW=2 (寄存器减 1) */
    LCD_WriteReg(FIMD_VIDTCON0, (9u  << 16) | (11u << 8) | 1u);

    /* 水平时序 (Android wa101s 7.0): HBPD=24, HFPD=72, HSPW=4 (寄存器减 1)
     * 行总数 = 800+72+24+4 = 900, 66.7MHz 下刷新率约 57Hz */
    LCD_WriteReg(FIMD_VIDTCON1, (23u << 16) | (71u << 8) | 3u);

    /* 分辨率: HOZVAL=800-1, LINEVAL=1280-1
     * SCP 手册 41-80: VIDTCON2 = LINEVAL[21:11] | HOZVAL[10:0] */
    LCD_WriteReg(FIMD_VIDTCON2, (1279u << 11) | 799u);

    /* 窗口 0: DATAPATH_DMA(bit22=0) + BURSTLEN_16WORD(bit10:9=0)
     * + INRGB_RGB(bit13=0) + BPPMODE_24BPP_888(0xB<<2, 32位 XRGB8888 存储)
     * 与内核 s3cfb_set_window_control 对 32bpp/transp=0 的配置一致 */
    LCD_WriteReg(FIMD_WINCON0, 0xBu << 2);

    /* 窗口 0 位置: 左上 (0,0), 右下 (799,1279), 尺寸 800*1280
     * VIDOSD0B = RIGHTX[21:11] | BOTTOMY[10:0] */
    LCD_WriteReg(FIMD_VIDOSD0A, 0u);
    LCD_WriteReg(FIMD_VIDOSD0B, (799u << 11) | 1279u);
    LCD_WriteReg(FIMD_VIDOSD0C, (uint32_t)LCD_WIDTH * LCD_HEIGHT);

    /* 帧缓冲地址: 0x50000000 ~ 0x503E8000, 页宽 800*4 字节 */
    LCD_WriteReg(FIMD_VIDW00ADD0B0, LCD_FB_BASE);
    LCD_WriteReg(FIMD_VIDW00ADD1B0, LCD_FB_BASE + LCD_FB_SIZE);
    LCD_WriteReg(FIMD_VIDW00ADD2,   (uint32_t)LCD_WIDTH * 4);

    /* 使能窗口 0 通道 (WINSHMAP[0]=1, 与内核 s3cfb_window_on 一致) */
    LCD_WriteReg(FIMD_WINSHMAP, 1u);

    /* 5. 开窗口 + 开显示 (ENVID|ENVID_F) */
    LCD_SetBits(FIMD_WINCON0, 1u << 0);
    LCD_WriteReg(FIMD_VIDCON0, vidcon0 | 0x3u);

    System_Delay(0x40000);
}

/**
 * @brief  用指定颜色清屏 (0xRRGGBB)
 */
void LCD_Clear(uint32_t Color)
{
    volatile uint32_t *fb = (volatile uint32_t *)LCD_FB_BASE;
    uint32_t i;

    Color &= 0x00FFFFFFUL;
    for (i = 0; i < (uint32_t)LCD_WIDTH * LCD_HEIGHT; i++) {
        fb[i] = Color;
    }
}

/**
 * @brief  画一个像素点
 */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint32_t Color)
{
    volatile uint32_t *fb = (volatile uint32_t *)LCD_FB_BASE;

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }
    fb[(uint32_t)y * LCD_WIDTH + x] = Color & 0x00FFFFFFUL;
}

/**
 * @brief 画实心矩形
 */
void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  uint32_t Color)
{
    volatile uint32_t *fb = (volatile uint32_t *)LCD_FB_BASE;
    uint16_t x, y;

    Color &= 0x00FFFFFFUL;
    for (y = y0; y <= y1 && y < LCD_HEIGHT; y++) {
        for (x = x0; x <= x1 && x < LCD_WIDTH; x++) {
            fb[(uint32_t)y * LCD_WIDTH + x] = Color;
        }
    }
}

/**
 * @brief 画一条线段 (简化 Bresenham)
 */
void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  uint32_t Color)
{
    int16_t dx = (int16_t)x1 - x0;
    int16_t dy = (int16_t)y1 - y0;
    int16_t sx = (dx > 0) ? 1 : -1;
    int16_t sy = (dy > 0) ? 1 : -1;
    int16_t err = (dx > 0 ? dx : -dx) - (dy > 0 ? dy : -dy);

    for (;;) {
        LCD_DrawPixel(x0, y0, Color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int16_t e2 = 2 * err;
            if (e2 > -(dy > 0 ? dy : -dy)) {
                err -= (dy > 0 ? dy : -dy);
                x0 += sx;
            }
            if (e2 < (dx > 0 ? dx : -dx)) {
                err += (dx > 0 ? dx : -dx);
                y0 += sy;
            }
        }
    }
}

/**
 * @brief 绘制全屏测试图案 (按 800x1280 设计):
 *   - 四象限四色: 判断画面是否被裁切/偏移/镜像
 *   - 四边 2px 白色边框: 判断是否满屏
 *   - 四角 TL/TR/BL/BR 标签: 判断方向/位置
 *   - 两条对角线: 判断是否有缩放/拉伸
 *   - 中央 "800x1280" 文字: 判断中心是否在正中
 */
void LCD_TestPattern(void)
{
    LCD_FillRect(0,   0,   399, 639,  LCD_COLOR_RED);
    LCD_FillRect(400, 0,   799, 639,  LCD_COLOR_GREEN);
    LCD_FillRect(0,   640, 399, 1279, LCD_COLOR_BLUE);
    LCD_FillRect(400, 640, 799, 1279, LCD_COLOR_YELLOW);

    LCD_FillRect(0,   0,   799, 1,    LCD_COLOR_WHITE);
    LCD_FillRect(0,   1278, 799, 1279, LCD_COLOR_WHITE);
    LCD_FillRect(0,   0,   1,   1279, LCD_COLOR_WHITE);
    LCD_FillRect(798, 0,   799, 1279, LCD_COLOR_WHITE);

    LCD_DrawLine(0, 0, 799, 1279, LCD_COLOR_WHITE);
    LCD_DrawLine(799, 0, 0, 1279, LCD_COLOR_WHITE);

    LCD_ShowString_Scaled(8,   8,    "TL", 3, LCD_COLOR_WHITE, LCD_COLOR_RED);
    LCD_ShowString_Scaled(744, 8,    "TR", 3, LCD_COLOR_WHITE, LCD_COLOR_GREEN);
    LCD_ShowString_Scaled(8,   1224, "BL", 3, LCD_COLOR_WHITE, LCD_COLOR_BLUE);
    LCD_ShowString_Scaled(744, 1224, "BR", 3, LCD_COLOR_WHITE, LCD_COLOR_YELLOW);

    LCD_ShowString_Scaled(336, 624, "800x1280", 2, LCD_COLOR_WHITE, LCD_COLOR_BLACK);
}

/**
 * @brief  在 (x,y) 画一个 8x16 字符
 */
void LCD_DrawChar(uint16_t x, uint16_t y, char ch,
                  uint32_t fgColor, uint32_t bgColor)
{
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;

    if (ch < LCD_FONT_START || ch >= LCD_FONT_START + LCD_FONT_CHARS) {
        ch = '?';
    }
    glyph = LCD_Font8x16[ch - LCD_FONT_START];

    for (row = 0; row < LCD_FONT_HEIGHT; row++) {
        for (col = 0; col < LCD_FONT_WIDTH; col++) {
            if (glyph[row] & (0x80u >> col)) {
                LCD_DrawPixel(x + col, y + row, fgColor);
            } else {
                LCD_DrawPixel(x + col, y + row, bgColor);
            }
        }
    }
}

/**
 * @brief  在 (x,y) 显示字符串
 */
void LCD_ShowString(uint16_t x, uint16_t y, const char *str,
                    uint32_t fgColor, uint32_t bgColor)
{
    while (*str != '\0') {
        LCD_DrawChar(x, y, *str, fgColor, bgColor);
        x += LCD_FONT_WIDTH;
        str++;
    }
}

/**
 * @brief  按 scale 倍放大画一个 ASCII 字符
 * @param  scale 放大倍数 (1~8), 字模每个点画成 scale x scale 的方块
 * @note   800x1280 高分屏上原字号太小, 放大后清晰可读
 */
void LCD_DrawChar_Scaled(uint16_t x, uint16_t y, char ch, uint8_t scale,
                         uint32_t fgColor, uint32_t bgColor)
{
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;
    uint16_t bx;
    uint16_t by;

    if (scale == 0) {
        scale = 1;
    }
    if (ch < LCD_FONT_START || ch >= LCD_FONT_START + LCD_FONT_CHARS) {
        ch = '?';
    }
    glyph = LCD_Font8x16[ch - LCD_FONT_START];

    for (row = 0; row < LCD_FONT_HEIGHT; row++) {
        for (col = 0; col < LCD_FONT_WIDTH; col++) {
            uint32_t color = (glyph[row] & (0x80u >> col)) ? fgColor : bgColor;

            /* 把字模 1 个点放大成 scale x scale 的实心方块 */
            for (by = 0; by < scale; by++) {
                for (bx = 0; bx < scale; bx++) {
                    LCD_DrawPixel(x + (uint16_t)col * scale + bx,
                                  y + (uint16_t)row * scale + by, color);
                }
            }
        }
    }
}

/**
 * @brief  按 scale 倍放大显示字符串
 * @note   每字符占 8*scale 宽, 行高 16*scale
 */
void LCD_ShowString_Scaled(uint16_t x, uint16_t y, const char *str,
                           uint8_t scale,
                           uint32_t fgColor, uint32_t bgColor)
{
    while (*str != '\0') {
        LCD_DrawChar_Scaled(x, y, *str, scale, fgColor, bgColor);
        x += LCD_FONT_WIDTH * scale;
        str++;
    }
}
