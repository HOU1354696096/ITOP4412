/**
 * @file    led.c
 * @brief   iTOP-4412 底板 LED 应用模块实现
 *
 * 从 main.c 中拆出的 LED 实例 (归档模块化):
 *   - LED1 = GPL2_0, LED2 = GPK1_1 (高电平点亮)
 *   - 非阻塞闪烁由 System_GetMs() 毫秒节拍驱动
 */
#include "led.h"
#include "exynos4412_gpio.h"

#define LED1_GPIO   GPL2
#define LED1_PIN    GPIO_Pin_0
#define LED2_GPIO   GPK1
#define LED2_PIN    GPIO_Pin_1

#define LED_BLINK_PERIOD_MS   500

static uint8_t LedState[2] = { 0, 0 };   /* [0]=LED1, [1]=LED2 */
static uint32_t LastBlinkMs = 0;
static uint8_t LedBlinkEnable = 1;       /* 是否允许闪烁 (BACK=开, HOME=关) */

/**
 * @brief 初始化 LED 引脚为输出
 */
void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin   = LED1_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_AF    = 0;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;
    GPIO_Init(LED1_GPIO, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = LED2_PIN;
    GPIO_Init(LED2_GPIO, &GPIO_InitStructure);

    LED_Set(LED1, 0);
    LED_Set(LED2, 0);
}

/**
 * @brief 设置 LED 状态
 */
void LED_Set(uint8_t led, uint8_t on)
{
    if (led == LED1) {
        if (on) {
            GPIO_SetBits(LED1_GPIO, LED1_PIN);
        } else {
            GPIO_ResetBits(LED1_GPIO, LED1_PIN);
        }
        LedState[0] = (on != 0);
    } else if (led == LED2) {
        if (on) {
            GPIO_SetBits(LED2_GPIO, LED2_PIN);
        } else {
            GPIO_ResetBits(LED2_GPIO, LED2_PIN);
        }
        LedState[1] = (on != 0);
    }
}

/**
 * @brief 翻转 LED
 */
void LED_Toggle(uint8_t led)
{
    if (led == LED1) {
        LED_Set(LED1, !LedState[0]);
    } else if (led == LED2) {
        LED_Set(LED2, !LedState[1]);
    }
}

uint8_t LED_GetState(uint8_t led)
{
    if (led == LED1) {
        return LedState[0];
    }
    if (led == LED2) {
        return LedState[1];
    }
    return 0;
}

const char *LED_GetStateString(uint8_t led)
{
    return (LED_GetState(led) != 0) ? "ON" : "OFF";
}

/**
 * @brief 开启 LED 交替闪烁 (对应 BACK 键)
 */
void LED_BlinkEnable(void)
{
    LedBlinkEnable = 1;
    LastBlinkMs = 0;
    LED_Set(LED1, 1);
    LED_Set(LED2, 0);
}

/**
 * @brief 停止 LED 交替闪烁并熄灭 (对应 HOME 键)
 */
void LED_BlinkDisable(void)
{
    LedBlinkEnable = 0;
    LED_Set(LED1, 0);
    LED_Set(LED2, 0);
}

/** @brief 查询闪烁是否开启 (供屏幕显示) */
uint8_t LED_IsBlinking(void)
{
    return LedBlinkEnable;
}

/**
 * @brief 非阻塞交替闪烁
 */
void LED_BlinkTick(uint32_t nowMs)
{
    if (LedBlinkEnable == 0) {
        return;
    }
    if ((nowMs - LastBlinkMs) < LED_BLINK_PERIOD_MS) {
        return;
    }
    LastBlinkMs = nowMs;

    /* LED1/LED2 交替亮灭 */
    if (LedState[0] == 0) {
        LED_Set(LED1, 1);
        LED_Set(LED2, 0);
    } else {
        LED_Set(LED1, 0);
        LED_Set(LED2, 1);
    }
}
