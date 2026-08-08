/**
 * @file    led.h
 * @brief   iTOP-4412 底板 LED 应用模块头文件
 *
 * 硬件 (iTOP-4412 精英版/POP 底板):
 *   - LED1: GPL2_0  (底板 LED, 高电平点亮)
 *   - LED2: GPK1_1  (底板 LED, 高电平点亮)
 */
#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/** @brief LED 编号: 1 或 2 */
#define LED1    1
#define LED2    2

/** @brief 初始化两个 LED 引脚为推挽输出, 初始熄灭 */
void LED_Init(void);

/** @brief 设置 LED 状态: 1=点亮, 0=熄灭 */
void LED_Set(uint8_t led, uint8_t on);

/** @brief 翻转 LED 状态 */
void LED_Toggle(uint8_t led);

/** @brief 读取 LED 当前软件状态: 1=亮, 0=灭 */
uint8_t LED_GetState(uint8_t led);

/** @brief LED 状态字符串: "ON" / "OFF" (供屏幕显示) */
const char *LED_GetStateString(uint8_t led);

/** @brief 开启 LED 交替闪烁 (对应 BACK 键) */
void LED_BlinkEnable(void);

/** @brief 停止 LED 交替闪烁并熄灭 (对应 HOME 键) */
void LED_BlinkDisable(void);

/** @brief 查询闪烁是否开启: 1=闪烁中, 0=已停止 (供屏幕显示) */
uint8_t LED_IsBlinking(void);

/**
 * @brief 非阻塞交替闪烁: 500ms 翻转一次, 两个 LED 交替亮灭
 * @param nowMs 当前毫秒 (System_GetMs())
 */
void LED_BlinkTick(uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H */
