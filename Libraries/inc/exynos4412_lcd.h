/**
 * @file    exynos4412_lcd.h
 * @brief   Exynos4412 FIMD/LVDS-LCD 库头文件 (STM32 库风格)
 *
 * 开发板: 迅为 iTOP-4412 精英版 (POP 封装)
 * 屏幕  : 迅为 7.0 寸 IPS 高清屏, LVDS 接口
 *         有效扫描 800x1280 (CLAA070WP03XG), 电容屏
 *
 * 引脚/电源说明 (参考开发板手册 71.6 节):
 *   - 背光电源使能    : GPL0_4  输出高电平
 *   - LVDS 桥 GM8285C : GPL1_0  输出高电平 (SHTDN=高时正常发送)
 *   - RGB 数据引脚    : GPF0~GPF3 全部复用为 FIMD 功能
 *
 * 显存: 固定放在 DDR 0x50000000 (800*1280*4 字节 ≈ 4MB),
 *       主程序从 DDR 0x43E00000 运行, 栈在 0x7FFFE000, 互不冲突。
 */
#ifndef __EXYNOS4412_LCD_H
#define __EXYNOS4412_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/*------------------- 屏幕参数 (迅为 7.0 寸 IPS LVDS, 有效扫描 800x1280) ---------*/
#define LCD_WIDTH       800
#define LCD_HEIGHT      1280

/* 帧缓冲 (framebuffer) 基地址与大小: 32bpp (每像素 4 字节) */
#define LCD_FB_BASE     0x50000000UL
#define LCD_FB_SIZE     ((uint32_t)LCD_WIDTH * LCD_HEIGHT * 4)

/*------------------- 常用颜色 (0xRRGGBB) -------------------*/
#define LCD_COLOR_BLACK     0x000000UL
#define LCD_COLOR_WHITE     0xFFFFFFUL
#define LCD_COLOR_RED       0xFF0000UL
#define LCD_COLOR_GREEN     0x00FF00UL
#define LCD_COLOR_BLUE      0x0000FFUL
#define LCD_COLOR_YELLOW    0xFFFF00UL
#define LCD_COLOR_CYAN      0x00FFFFUL
#define LCD_COLOR_MAGENTA   0xFF00FFUL

/*------------------- 函数声明 -------------------*/

/**
 * @brief  初始化 FIMD 并点亮 LVDS-LCD
 * @note   内部流程: 背光/桥使能 -> LCD 时钟 -> GPIO 复用 -> FIMD 寄存器 -> 清屏
 */
void LCD_Init(void);

/**
 * @brief 用指定颜色清空整个屏幕
 */
void LCD_Clear(uint32_t Color);

/**
 * @brief 在指定坐标画一个像素点
 * @param x 横坐标 (0~799)
 * @param y 纵坐标 (0~1279)
 */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint32_t Color);

/**
 * @brief 在指定坐标画一个 8x16 ASCII 字符 (带背景色)
 */
void LCD_DrawChar(uint16_t x, uint16_t y, char ch,
                  uint32_t fgColor, uint32_t bgColor);

/**
 * @brief 在指定坐标显示字符串 (每字符宽 8 像素, 高 16 像素)
 */
void LCD_ShowString(uint16_t x, uint16_t y, const char *str,
                    uint32_t fgColor, uint32_t bgColor);

/**
 * @brief 按 scale 倍放大画一个 ASCII 字符 (每字符宽 8*scale, 高 16*scale)
 * @note  800x1280 高分屏上 8x16 原始字号太小, 用放大倍数让字可读
 */
void LCD_DrawChar_Scaled(uint16_t x, uint16_t y, char ch, uint8_t scale,
                         uint32_t fgColor, uint32_t bgColor);

/**
 * @brief 按 scale 倍放大显示字符串, 字符间距为 8*scale
 */
void LCD_ShowString_Scaled(uint16_t x, uint16_t y, const char *str,
                           uint8_t scale,
                           uint32_t fgColor, uint32_t bgColor);

/**
 * @brief 画实心矩形 (直接写显存)
 */
void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  uint32_t Color);

/**
 * @brief 画一条线段 (简化 Bresenham)
 */
void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  uint32_t Color);

/**
 * @brief 绘制全屏测试图案 (验证满屏/方向/缩放用, 按 800x1280 设计)
 */
void LCD_TestPattern(void);

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_LCD_H */
