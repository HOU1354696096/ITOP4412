/**
 * @file    exynos4412_key.h
 * @brief   Exynos4412 底板 5 按键库头文件 (STM32 库风格)
 *
 * 硬件 (iTOP-4412 精英版/POP 底板, 与内核 mach-itop4412.c 一致):
 *   VOL- : GPX2(0)  按键按下为低电平 (active_low)
 *   VOL+ : GPX2(1)
 *   SLEEP: GPX3(3)
 *   BACK : GPX1(2)
 *   HOME : GPX1(1)
 * 所有按键按下时引脚为低, 松开为高, 初始化需开上拉。
 */
#ifndef __EXYNOS4412_KEY_H
#define __EXYNOS4412_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/*------------------- 按键编号 -------------------*/
typedef enum {
    KEY_VOL_DOWN = 0,   /* VOL- */
    KEY_VOL_UP,         /* VOL+ */
    KEY_SLEEP,          /* SLEEP */
    KEY_BACK,           /* BACK */
    KEY_HOME,           /* HOME */
    KEY_NUM             /* 按键总数 */
} Key_TypeDef;

/**
 * @brief 初始化 5 个按键引脚: 输入 + 上拉 (按下为低)
 */
void Key_Init(void);

/**
 * @brief 读取按键当前电平状态 (不去抖)
 * @param key 按键编号
 * @retval 1 = 按下, 0 = 松开
 */
uint8_t Key_IsPressed(Key_TypeDef key);

/**
 * @brief 消抖扫描按键, 主循环周期调用
 * @param nowMs 当前毫秒 (System_GetMs())
 * @retval 本次新按下并消抖确认的按键, 无则返回 KEY_NUM
 */
Key_TypeDef Key_Scan(uint32_t nowMs);

/**
 * @brief 按键名称字符串: "VOL-"/"VOL+"/"SLEEP"/"BACK"/"HOME" (供屏幕显示)
 */
const char *Key_GetName(Key_TypeDef key);

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_KEY_H */
