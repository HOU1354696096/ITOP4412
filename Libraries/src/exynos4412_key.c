/**
 * @file    exynos4412_key.c
 * @brief   Exynos4412 底板 5 按键库实现 (STM32 库风格, 含软件消抖)
 *
 * 引脚 (与内核 mach-itop4412.c 的 gpio_buttons 一致, 全部 active_low):
 *   VOL- : GPX2(0)   VOL+ : GPX2(1)
 *   SLEEP: GPX3(3)
 *   BACK : GPX1(2)   HOME : GPX1(1)
 *
 * 消抖策略: 每 10ms 扫描一次, 连续 2 次读到相同电平才认可状态变化,
 * 仅在"松开 -> 按下"的边沿返回按键号, 避免长按重复触发。
 */
#include "exynos4412_key.h"
#include "exynos4412_gpio.h"

#define KEY_SCAN_MS        10   /* 扫描周期 */
#define KEY_STABLE_REQ     2    /* 连续一致次数 (20ms 消抖) */

typedef struct {
    GPIO_TypeDef *Port;
    uint32_t      Pin;
} KeyPinDef;

static const KeyPinDef KeyPins[KEY_NUM] = {
    { GPX2, GPIO_Pin_0 },   /* VOL- */
    { GPX2, GPIO_Pin_1 },   /* VOL+ */
    { GPX3, GPIO_Pin_3 },   /* SLEEP */
    { GPX1, GPIO_Pin_2 },   /* BACK */
    { GPX1, GPIO_Pin_1 },   /* HOME */
};

static const char *const KeyNames[KEY_NUM] = {
    "VOL-", "VOL+", "SLEEP", "BACK", "HOME"
};

static uint8_t KeyRaw[KEY_NUM];        /* 最近一次原始电平 */
static uint8_t KeyStable[KEY_NUM];     /* 消抖后的稳定电平 */
static uint8_t KeyCount[KEY_NUM];      /* 连续一致计数 */
static uint8_t KeyPrevPressed[KEY_NUM];/* 上一稳定状态是否为按下 */

/** @brief 读取单个按键原始电平 (active_low: 引脚低 = 按下) */
static uint8_t Key_RawRead(Key_TypeDef key)
{
    return (GPIO_ReadInputDataBit(KeyPins[key].Port, KeyPins[key].Pin) == 0) ? 1 : 0;
}

/**
 * @brief 初始化 5 个按键引脚: GPIO 输入 + 上拉
 */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint8_t i;

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_AF    = 0;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPX2, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_Init(GPX3, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_Init(GPX1, &GPIO_InitStructure);

    for (i = 0; i < KEY_NUM; i++) {
        KeyRaw[i] = Key_RawRead((Key_TypeDef)i);
        KeyStable[i] = KeyRaw[i];
        KeyCount[i] = 0;
        KeyPrevPressed[i] = KeyStable[i];
    }
}

/**
 * @brief 读取按键当前电平状态 (不去抖)
 */
uint8_t Key_IsPressed(Key_TypeDef key)
{
    if (key >= KEY_NUM) {
        return 0;
    }
    return Key_RawRead(key);
}

/**
 * @brief 消抖扫描按键: 每 10ms 扫描, 返回新按下的按键 (边沿触发)
 */
Key_TypeDef Key_Scan(uint32_t nowMs)
{
    static uint32_t LastScanMs = 0;
    uint8_t i;

    if ((uint32_t)(nowMs - LastScanMs) < KEY_SCAN_MS) {
        return KEY_NUM;
    }
    LastScanMs = nowMs;

    /* 1. 采集并消抖 */
    for (i = 0; i < KEY_NUM; i++) {
        uint8_t raw = Key_RawRead((Key_TypeDef)i);
        if (raw == KeyRaw[i]) {
            if (KeyCount[i] < 0xFF) {
                KeyCount[i]++;
            }
            if (KeyCount[i] >= KEY_STABLE_REQ) {
                KeyStable[i] = raw;
            }
        } else {
            KeyRaw[i] = raw;
            KeyCount[i] = 0;
        }
    }

    /* 2. 检测"松开 -> 按下"边沿 */
    for (i = 0; i < KEY_NUM; i++) {
        if (KeyStable[i] != 0 && KeyPrevPressed[i] == 0) {
            KeyPrevPressed[i] = 1;
            return (Key_TypeDef)i;
        }
        KeyPrevPressed[i] = KeyStable[i];
    }
    return KEY_NUM;
}

/**
 * @brief 按键名称字符串
 */
const char *Key_GetName(Key_TypeDef key)
{
    if (key >= KEY_NUM) {
        return "?";
    }
    return KeyNames[key];
}
