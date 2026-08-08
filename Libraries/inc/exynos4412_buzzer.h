/**
 * @file    exynos4412_buzzer.h
 * @brief   Exynos4412 蜂鸣器库头文件 (STM32 库风格, PWM 变调版)
 *
 * 硬件: iTOP-4412 精英版 (POP 封装) 底板蜂鸣器
 *   - 引脚: GPD0_0 (内核 itop4412_buzzer.c: BUZZER_GPIO = EXYNOS4_GPD0(0))
 *   - 驱动: Q5(L9014) 三极管, 基极串 R23=10K, 网络名 MOTOR_PWM
 *   - PZ1 = PIEZO BUZZER; 实测 GPIO 直流驱动只响一声 -> 无源压电蜂鸣器,
 *     必须用 PWM 输出不同频率才能产生音调
 *
 * 音调生成: PWM 定时器 0 (基址 0x139D0000)
 *   - 预分频0 TCFG0[7:0]=49 -> PCLK 100MHz/50 = 2MHz
 *   - 2级分频 TCFG1[3:0]=0  -> 1/1
 *   - TCNTB0 = 2000000/频率, TCMPB0 = TCNTB0/2 (50% 占空比)
 *   - TCON 通道0: start=bit0, manual=bit1, invert=bit2, auto=bit3
 *
 * 注意:
 *   - GPD0_1(TOUT_1) 是 LCD 背光 PWM, 本库只读写 TCFG0[7:0] /
 *     TCFG1[3:0] / TCON[3:0], 绝不触碰背光使用的通道1位域。
 *   - 歌曲播放为非阻塞方式: 主循环周期调用 Buzzer_Tick(System_GetMs())。
 */
#ifndef __EXYNOS4412_BUZZER_H
#define __EXYNOS4412_BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/*------------------- 音符结构 (STM32 风格) -------------------*/
typedef struct {
    uint16_t Freq;       /* 频率 (Hz), 0 = 休止符 */
    uint16_t DurationMs; /* 时值 (ms) */
} Buzzer_NoteTypeDef;

/*------------------- 内置曲谱:《祝你生日快乐》(Happy Birthday) -------------------*/
/* 1=C, 3/4 拍, 四分音符≈500ms:
 *   乐句1: 5 5 6 5 1' 7      祝你生日快乐
 *   乐句2: 5 5 6 5 2' 1'     祝你生日快乐
 *   乐句3: 5 5 5' 3' 1' 7 6  祝你生日快乐 (旋律升高)
 *   乐句4: 4' 4' 3' 1' 2' 1' 祝你生日快乐 (尾句, 末音长收)
 * 频率: 5=784 6=880 7=988 1'=1046 2'=1175 3'=1319 5'=1568 4'=1397
 * 整曲约 15 秒, 循环播放
 */
extern const Buzzer_NoteTypeDef Buzzer_Song_HappyBirthday[];
extern const uint16_t          Buzzer_Song_HappyBirthday_Len;

/*------------------- 内置曲谱:《小星星》------------------*/
extern const Buzzer_NoteTypeDef Buzzer_Song_Twinkle[];
extern const uint16_t          Buzzer_Song_Twinkle_Len;

/*------------------- 内置曲谱:《两只老虎》------------------*/
extern const Buzzer_NoteTypeDef Buzzer_Song_TwoTigers[];
extern const uint16_t          Buzzer_Song_TwoTigers_Len;

/*------------------- 函数声明 -------------------*/

/**
 * @brief  初始化蜂鸣器: GPD0_0=AF2(TOUT_0), 配置 PWM0 时钟, 初始关闭
 * @note   调用前需先 System_ClockInit(); 若先初始化 LCD 背光,
 *         本函数会保留背光 PWM1 的配置位域
 */
void Buzzer_Init(void);

/**
 * @brief  设置并输出指定频率的方波 (50% 占空比)
 * @param  freq 频率 Hz; 0 表示停止输出
 */
void Buzzer_SetFreq(uint32_t freq);

/** @brief 打开蜂鸣器 (沿用上次设置的频率) */
void Buzzer_On(void);

/** @brief 关闭蜂鸣器 (停止 PWM0 通道) */
void Buzzer_Off(void);

/** @brief 设置蜂鸣器开关: 1=响(沿用频率), 0=静音 */
void Buzzer_Set(uint8_t on);

/**
 * @brief  阻塞式发声: 输出 freq 频率持续 ms 毫秒后关闭
 * @note   依赖 System_GetMs() 毫秒节拍, 需先 System_TickInit()
 */
void Buzzer_ToneMs(uint32_t freq, uint32_t ms);

/**
 * @brief  非阻塞方式开始播放歌曲
 * @param  song 音符表指针
 * @param  len  音符个数
 * @param  loop 1=循环播放, 0=播完停止
 */
void Buzzer_StartSong(const Buzzer_NoteTypeDef *song, uint16_t len,
                      uint8_t loop);

/** @brief 停止歌曲播放并关闭蜂鸣器 */
void Buzzer_StopSong(void);

/**
 * @brief  歌曲播放推进函数, 主循环周期调用 (非阻塞)
 * @param  nowMs 当前毫秒 (System_GetMs())
 */
void Buzzer_Tick(uint32_t nowMs);

/** @brief 是否正在播放歌曲 (返回 SET=播放中) */
FlagStatus Buzzer_IsPlaying(void);

/** @brief 当前音符频率 (0=休止, 供显示) */
uint16_t Buzzer_GetNoteFreq(void);

/** @brief 当前音符时值 (ms, 供显示) */
uint16_t Buzzer_GetNoteDuration(void);

/** @brief 当前播放到第几个音符 (0 起始, 配合 Buzzer_GetSongLength 显示进度) */
uint16_t Buzzer_GetNoteIndex(void);

/** @brief 当前歌曲总音符数 */
uint16_t Buzzer_GetSongLength(void);

/** @brief 播放状态字符串: "PLAYING" / "STOPPED" (供屏幕显示) */
const char *Buzzer_GetStateString(void);

/*------------------- 歌曲列表操作 (VOL-/VOL+ 上下曲) -------------------*/

/** @brief 获取内置歌曲数量 */
uint8_t Buzzer_GetSongCount(void);

/** @brief 获取当前歌曲索引 (0 起) */
uint8_t Buzzer_GetSongIndex(void);

/** @brief 获取当前歌曲名称 (供屏幕/串口显示) */
const char *Buzzer_GetSongName(void);

/** @brief 按索引开始循环播放歌曲 (立即静音, 300ms 后再开始新歌, 切换不突兀) */
void Buzzer_PlaySongByIndex(uint8_t idx);

/** @brief 切到下一首并开始播放 (循环) */
void Buzzer_NextSong(void);

/** @brief 切到上一首并开始播放 (循环) */
void Buzzer_PrevSong(void);

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_BUZZER_H */
