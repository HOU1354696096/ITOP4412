/**
 * @file    exynos4412_buzzer.c
 * @brief   Exynos4412 蜂鸣器驱动实现 (STM32 库风格, PWM 变调版)
 *
 * 参考:
 *   - 迅为 iTOP-4412 精英版使用手册 61.2 PWM 控制定时器实验
 *     (GPD0CON[3:0]=0x2 -> TOUT_0; TCON ch0: manual=0x2, start=0x1,
 *      auto=0x8; TCMPB0/TCNTB0 决定占空比)
 *   - 内核 drivers/char/itop4412_buzzer.c (POP 板 BUZZER_GPIO=GPD0_0)
 *   - 内核 drivers/pwm/pwm-samsung.c (Exynos4 TCON 位定义)
 *   - u-boot s5pv310.h (TCNTB0=0x139D000C, TCMPB0=0x139D0010)
 *
 * 时钟链: PCLK(ACLK_100)=100MHz -> 预分频0(49+1=50) -> 2MHz
 *         -> MUX0 1/1 -> PWM0 计数频率 2MHz
 * 频率换算: TCNTB0 = 2000000 / freq, TCMPB0 = TCNTB0 / 2 (50%)
 *
 * 与 LCD 背光的共存:
 *   背光 PWM1 使用 TCFG0[7:0](共用的预分频0) + TCFG1[7:4](MUX1) +
 *   TCON[11:8](通道1). 本文件所有寄存器操作都做"读-改-写", 只动
 *   自己拥有的位域, 避免把背光配置冲掉。
 *
 * 诊断结论 (2026-08-08):
 *   有源/无源之争以实测为准: GPIO 直流驱动只响一声 -> 无源压电蜂鸣器,
 *   必须用 PWM 变调。若接上有源蜂鸣器, 可把本文件切回 GPIO 通断节奏版。
 */
#include "exynos4412_buzzer.h"
#include "exynos4412_gpio.h"
#include "exynos4412_clock.h"

/*------------------- PWM 定时器寄存器 (基址 0x139D0000) -------------------*/
#define PWM_TCFG0       0x139D0000UL   /* 预分频0 [7:0] / 预分频1 [15:8] */
#define PWM_TCFG1       0x139D0004UL   /* MUX0 [3:0] / MUX1 [7:4] / MUX2 [11:8] */
#define PWM_TCON        0x139D0008UL   /* 通道0: start=0 manual=1 inv=2 auto=3 */
#define PWM_TCNTB0      0x139D000CUL
#define PWM_TCMPB0      0x139D0010UL

#define CLK_GATE_IP_PERIL  0x1003C950UL  /* PWM 定时器时钟门 [24] */

/* PWM0 计数频率 = 100MHz / 50 = 2MHz */
#define BUZZER_TICK_HZ      2000000UL

/* 音符之间的顿音停顿 (ms), 让旋律更清晰 */
#define BUZZER_GAP_MS       25

/*------------------- 内置曲谱:《祝你生日快乐》(Happy Birthday) -------------------*/
/* 1=C, 3/4 拍, 四分音符≈500ms (与常见简谱逐音对应):
 *   乐句1: 5 5 6 5 1' 7         祝你生日快乐
 *   乐句2: 5 5 6 5 2' 1'        祝你生日快乐
 *   乐句3: 5 5 5' 3' 1' 7 6     祝你生日快乐 (旋律升高)
 *   乐句4: 4' 4' 3' 1' 2' 1'    祝你生日快乐 (尾句, 末音长收)
 * 频率(1=C 高音区, 无源蜂鸣器响应好):
 *   5=784 6=880 7=988 1'=1046 2'=1175 3'=1319 5'=1568 4'=1397
 * 整曲约 15 秒, 循环播放
 */
const Buzzer_NoteTypeDef Buzzer_Song_HappyBirthday[] = {
    /* 乐句1: 祝你生日快乐 */
    {784, 250}, {784, 250}, {880, 500}, {784, 500}, {1046, 500}, {988, 750},
    {0, 250},                                                   /* 乐句间短停 */
    /* 乐句2: 祝你生日快乐 */
    {784, 250}, {784, 250}, {880, 500}, {784, 500}, {1175, 500}, {1046, 750},
    {0, 250},
    /* 乐句3: 祝你生日快乐 (旋律升高) */
    {784, 250}, {784, 250}, {1568, 500}, {1319, 500}, {1046, 500},
    {988, 500}, {880, 750},
    {0, 500},                                                   /* 尾句前短停 */
    /* 乐句4: 祝你生日快乐 (尾句, 末音长收) */
    {1397, 250}, {1397, 250}, {1319, 500}, {1046, 500}, {1175, 500}, {1046, 2000},
    /* 整曲结束短停 */
    {0, 600},
};
const uint16_t Buzzer_Song_HappyBirthday_Len =
    (uint16_t)(sizeof(Buzzer_Song_HappyBirthday) / sizeof(Buzzer_Song_HappyBirthday[0]));

/*------------------- 内置曲谱:《小星星》------------------*/
/* 1=C, 4/4 拍, 四分音符≈500ms:
 *   1 1 5 5 6 6 5- | 4 4 3 3 2 2 1- | 5 5 4 4 3 3 2- | 5 5 4 4 3 3 2-
 *   | 1 1 5 5 6 6 5- | 4 4 3 3 2 2 1--
 * 频率: 1=523 2=587 3=659 4=698 5=784 6=880
 */
const Buzzer_NoteTypeDef Buzzer_Song_Twinkle[] = {
    {523, 250}, {523, 250}, {784, 250}, {784, 250}, {880, 250}, {880, 250}, {784, 1000},
    {698, 250}, {698, 250}, {659, 250}, {659, 250}, {587, 250}, {587, 250}, {523, 1000},
    {784, 250}, {784, 250}, {698, 250}, {698, 250}, {659, 250}, {659, 250}, {587, 1000},
    {784, 250}, {784, 250}, {698, 250}, {698, 250}, {659, 250}, {659, 250}, {587, 1000},
    {523, 250}, {523, 250}, {784, 250}, {784, 250}, {880, 250}, {880, 250}, {784, 1000},
    {698, 250}, {698, 250}, {659, 250}, {659, 250}, {587, 250}, {587, 250}, {523, 2000},
    {0, 600},
};
const uint16_t Buzzer_Song_Twinkle_Len =
    (uint16_t)(sizeof(Buzzer_Song_Twinkle) / sizeof(Buzzer_Song_Twinkle[0]));

/*------------------- 内置曲谱:《两只老虎》------------------*/
/* 1=C, 4/4 拍, 四分音符≈500ms:
 *   1 2 3 1 | 1 2 3 1 | 3 4 5- | 3 4 5- | 5 6 5 4 3 1 | 5 6 5 4 3 1 | 1 5 1- | 1 5 1-
 * 频率: 1=523 2=587 3=659 4=698 5=784 6=880
 */
const Buzzer_NoteTypeDef Buzzer_Song_TwoTigers[] = {
    {523, 500}, {587, 500}, {659, 500}, {523, 500},
    {523, 500}, {587, 500}, {659, 500}, {523, 500},
    {659, 500}, {698, 500}, {784, 1000},
    {659, 500}, {698, 500}, {784, 1000},
    {784, 250}, {880, 250}, {784, 250}, {698, 250}, {659, 500}, {523, 500},
    {784, 250}, {880, 250}, {784, 250}, {698, 250}, {659, 500}, {523, 500},
    {523, 500}, {784, 1000},
    {523, 500}, {784, 1000},
    {0, 600},
};
const uint16_t Buzzer_Song_TwoTigers_Len =
    (uint16_t)(sizeof(Buzzer_Song_TwoTigers) / sizeof(Buzzer_Song_TwoTigers[0]));

/*------------------- 歌曲列表 (VOL-/VOL+ 上下曲切换) -------------------*/
typedef struct {
    const Buzzer_NoteTypeDef *Notes;
    uint16_t Len;
    const char *Name;
} Buzzer_SongInfoTypeDef;

static const Buzzer_SongInfoTypeDef Buzzer_SongList[] = {
    { Buzzer_Song_HappyBirthday, Buzzer_Song_HappyBirthday_Len, "HAPPY BIRTHDAY" },
    { Buzzer_Song_Twinkle,       Buzzer_Song_Twinkle_Len,       "TWINKLE STAR"   },
    { Buzzer_Song_TwoTigers,     Buzzer_Song_TwoTigers_Len,     "TWO TIGERS"     },
};
#define BUZZER_SONG_COUNT  ((uint8_t)(sizeof(Buzzer_SongList) / sizeof(Buzzer_SongList[0])))

static uint8_t Buzzer_CurSongIdx = 0;

/*------------------- 私有状态 -------------------*/
typedef enum {
    BZ_ST_NOTE = 0,   /* 正在发声/休止 */
    BZ_ST_GAP         /* 音符间顿音停顿 */
} BuzzerState;

static const Buzzer_NoteTypeDef *Buzzer_Song = 0;
static uint16_t Buzzer_SongLen = 0;
static uint16_t Buzzer_Idx = 0;
static uint32_t Buzzer_NoteStartMs = 0;
static uint8_t  Buzzer_Loop = 0;
static uint8_t  Buzzer_Playing = 0;
static uint8_t  Buzzer_State = BZ_ST_NOTE;
static uint32_t Buzzer_CurrentFreq = 0;
static uint8_t  Buzzer_PinMuxed = 0;   /* GPD0_0 是否已切换为 TOUT_0 */

/* 切歌延时: 按键后先静音一小段时间, 再开始新歌, 避免切换生硬 */
#define SONG_SWITCH_DELAY_MS   300
static uint8_t  Buzzer_PendingSwitch = 0;   /* 有待执行的切歌请求 */
static uint32_t Buzzer_PendingAtMs = 0;

/*------------------- 私有函数 -------------------*/
static inline void PWM_Write(uint32_t reg, uint32_t val)
{
    *(volatile uint32_t *)reg = val;
}

static inline uint32_t PWM_Read(uint32_t reg)
{
    return *(volatile uint32_t *)reg;
}

/**
 * @brief 把 GPD0_0 从"GPIO 输出低"切换为 TOUT_0 复用功能 (只切一次)
 * @note  初始化阶段引脚保持 GPIO 低电平, 保证系统上电/初始化期间绝对静音;
 *        真正开始发声前才切换到 PWM0 输出。
 */
static void Buzzer_PinToTout0(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    if (Buzzer_PinMuxed) {
        return;
    }
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_AF    = 2;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;
    GPIO_Init(GPD0, &GPIO_InitStructure);
    Buzzer_PinMuxed = 1;
}

/**
 * @brief 把 GPD0_0 切回 GPIO 输出并拉低 (静音保险)
 * @note  PWM 通道停止时 TOUT_0 输出电平不确定, 若停在"高"会把驱动管 Q5
 *        持续导通, 压电片发出尖锐长音; 强制拉低可保证休止期间绝对静音。
 */
static void Buzzer_PinToGpioLow(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    if (Buzzer_PinMuxed == 0) {
        return;
    }
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_AF    = 2;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;
    GPIO_Init(GPD0, &GPIO_InitStructure);
    GPIO_ResetBits(GPD0, GPIO_Pin_0);
    Buzzer_PinMuxed = 0;
}

/*------------------- 对外接口 -------------------*/

/**
 * @brief 初始化蜂鸣器: 引脚先保持 GPIO 低电平(静音), 配好 PWM0 时钟与分频
 */
void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint32_t reg;

    /* 1. GPD0_0 先配置为 GPIO 输出并拉低:
     *    上电后该脚若悬空/高阻, 驱动三极管 Q5 的基极可能被误导通,
     *    导致初始化阶段蜂鸣器误响; 主动输出低电平可保证绝对静音 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_AF    = 2;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Drv   = GPIO_Drv_LV1;
    GPIO_Init(GPD0, &GPIO_InitStructure);
    GPIO_ResetBits(GPD0, GPIO_Pin_0);
    Buzzer_PinMuxed = 0;

    /* 2. 打开 PWM 定时器时钟门 (CLK_GATE_IP_PERIL[24], 与背光共用) */
    PWM_Write(CLK_GATE_IP_PERIL, PWM_Read(CLK_GATE_IP_PERIL) | (1u << 24));

    /* 3. 预分频0 = 49 (50 分频 -> 2MHz).
     *    只改 [7:0], 保留 [15:8] 预分频1 (毫秒节拍 PWM2 用) */
    reg = PWM_Read(PWM_TCFG0);
    reg = (reg & ~0xFFu) | 49u;
    PWM_Write(PWM_TCFG0, reg);

    /* 4. MUX0 = 0 (1/1 分频).
     *    只改 [3:0], 保留 [7:4] MUX1 (背光 PWM1 用) 与 [11:8] MUX2 */
    reg = PWM_Read(PWM_TCFG1);
    reg = (reg & ~0x0Fu) | 0x0u;
    PWM_Write(PWM_TCFG1, reg);

    /* 5. 停止通道0, 初始静音 */
    PWM_Write(PWM_TCON, PWM_Read(PWM_TCON) & ~0x0Fu);
    Buzzer_CurrentFreq = 0;
}

/**
 * @brief 设置输出频率 (50% 占空比), freq=0 停止
 */
void Buzzer_SetFreq(uint32_t freq)
{
    uint32_t period;
    uint32_t reg;
    volatile uint32_t i;

    if (freq == 0) {
        /* 休止: 停止通道0 并强制把引脚拉低 (TOUT_0 停止时电平不定, 可能误响) */
        PWM_Write(PWM_TCON, PWM_Read(PWM_TCON) & ~0x0Fu);
        Buzzer_PinToGpioLow();
        Buzzer_CurrentFreq = 0;
        return;
    }
    /* 真正发声前, 把 GPD0_0 从 GPIO 低电平切换为 TOUT_0 */
    Buzzer_PinToTout0();

    if (freq > BUZZER_TICK_HZ) {
        freq = BUZZER_TICK_HZ;
    }
    if (freq < 20) {
        freq = 20;
    }
    period = BUZZER_TICK_HZ / freq;
    if (period < 2) {
        period = 2;
    }

    /* 1. 先停止通道0 */
    PWM_Write(PWM_TCON, PWM_Read(PWM_TCON) & ~0x0Fu);

    /* 2. 写入周期/比较值 */
    PWM_Write(PWM_TCNTB0, period);
    PWM_Write(PWM_TCMPB0, period / 2u);

    /* 3. 手动装载 (manual=bit1), 把 TCNTB0/TCMPB0 装入递减计数器 */
    reg = PWM_Read(PWM_TCON);
    reg = (reg & ~0x0Fu) | 0x02u;
    PWM_Write(PWM_TCON, reg);
    for (i = 0; i < 0x400; i++) {   /* 等待装载完成 */
    }

    /* 4. 清手动装载, 启动 (start=bit0) + 自动重载 (auto=bit3) */
    reg = PWM_Read(PWM_TCON);
    reg = (reg & ~0x0Fu) | 0x01u | 0x08u;
    PWM_Write(PWM_TCON, reg);

    Buzzer_CurrentFreq = freq;
}

/**
 * @brief 打开蜂鸣器 (沿用上次频率)
 */
void Buzzer_On(void)
{
    if (Buzzer_CurrentFreq != 0) {
        Buzzer_SetFreq(Buzzer_CurrentFreq);
    }
}

/**
 * @brief 关闭蜂鸣器
 */
void Buzzer_Off(void)
{
    PWM_Write(PWM_TCON, PWM_Read(PWM_TCON) & ~0x0Fu);
    Buzzer_PinToGpioLow();   /* 停止后强制拉低引脚, 保证静音 */
}

void Buzzer_Set(uint8_t on)
{
    if (on) {
        Buzzer_On();
    } else {
        Buzzer_Off();
    }
}

/**
 * @brief 阻塞式发声
 */
void Buzzer_ToneMs(uint32_t freq, uint32_t ms)
{
    uint32_t start;

    Buzzer_SetFreq(freq);
    start = System_GetMs();
    while ((System_GetMs() - start) < ms) {
    }
    Buzzer_Off();
}

/**
 * @brief 开始非阻塞播放歌曲
 */
void Buzzer_StartSong(const Buzzer_NoteTypeDef *song, uint16_t len,
                      uint8_t loop)
{
    if (song == 0 || len == 0) {
        return;
    }
    Buzzer_Song        = song;
    Buzzer_SongLen     = len;
    Buzzer_Idx         = 0;
    Buzzer_Loop        = (loop != 0);
    Buzzer_Playing     = 1;
    Buzzer_State       = BZ_ST_NOTE;
    Buzzer_NoteStartMs = System_GetMs();
    Buzzer_SetFreq(Buzzer_Song[0].Freq);
}

/**
 * @brief 停止歌曲播放
 */
void Buzzer_StopSong(void)
{
    Buzzer_Playing = 0;
    Buzzer_Song    = 0;
    Buzzer_SongLen = 0;
    Buzzer_Idx     = 0;
    Buzzer_PendingSwitch = 0;
    Buzzer_Off();
}

/**
 * @brief 歌曲推进: 主循环周期调用 (非阻塞)
 *
 * 状态机:
 *   NOTE: 当前音符/休止持续 DurationMs 后 -> 进入 GAP 并静音
 *   GAP : 停顿 25ms 后 -> 切到下一音符, 设置对应频率
 */
void Buzzer_Tick(uint32_t nowMs)
{
    uint32_t elapsed;

    /* 切歌延时等待: 期间保持静音, 到点后才真正开始新歌 */
    if (Buzzer_PendingSwitch) {
        if ((int32_t)(nowMs - Buzzer_PendingAtMs) < 0) {
            return;
        }
        Buzzer_PendingSwitch = 0;
        Buzzer_StartSong(Buzzer_SongList[Buzzer_CurSongIdx].Notes,
                         Buzzer_SongList[Buzzer_CurSongIdx].Len, 1);
        return;
    }

    if (Buzzer_Playing == 0 || Buzzer_Song == 0) {
        return;
    }

    elapsed = nowMs - Buzzer_NoteStartMs;

    if (Buzzer_State == BZ_ST_NOTE) {
        if (elapsed < Buzzer_Song[Buzzer_Idx].DurationMs) {
            return;
        }
        /* 音符/休止结束: 静音, 进入顿音停顿 */
        Buzzer_Off();
        Buzzer_State       = BZ_ST_GAP;
        Buzzer_NoteStartMs = nowMs;
        return;
    }

    /* GAP 状态 */
    if (elapsed < BUZZER_GAP_MS) {
        return;
    }

    /* 切到下一个音符 */
    Buzzer_Idx++;
    if (Buzzer_Idx >= Buzzer_SongLen) {
        if (Buzzer_Loop) {
            Buzzer_Idx = 0;
        } else {
            Buzzer_Playing = 0;
            Buzzer_Off();
            return;
        }
    }
    Buzzer_State       = BZ_ST_NOTE;
    Buzzer_NoteStartMs = nowMs;
    Buzzer_SetFreq(Buzzer_Song[Buzzer_Idx].Freq);
}

FlagStatus Buzzer_IsPlaying(void)
{
    return (Buzzer_Playing != 0) ? SET : RESET;
}

uint16_t Buzzer_GetNoteFreq(void)
{
    if (Buzzer_Song == 0 || Buzzer_Idx >= Buzzer_SongLen) {
        return 0;
    }
    return Buzzer_Song[Buzzer_Idx].Freq;
}

uint16_t Buzzer_GetNoteDuration(void)
{
    if (Buzzer_Song == 0 || Buzzer_Idx >= Buzzer_SongLen) {
        return 0;
    }
    return Buzzer_Song[Buzzer_Idx].DurationMs;
}

uint16_t Buzzer_GetNoteIndex(void)
{
    return Buzzer_Idx;
}

uint16_t Buzzer_GetSongLength(void)
{
    return Buzzer_SongLen;
}

const char *Buzzer_GetStateString(void)
{
    return (Buzzer_Playing != 0) ? "PLAYING" : "STOPPED";
}

/*------------------- 歌曲列表操作 (VOL-/VOL+ 上下曲) -------------------*/

/** @brief 获取内置歌曲数量 */
uint8_t Buzzer_GetSongCount(void)
{
    return BUZZER_SONG_COUNT;
}

/** @brief 获取当前歌曲索引 (0 起) */
uint8_t Buzzer_GetSongIndex(void)
{
    return Buzzer_CurSongIdx;
}

/** @brief 获取当前歌曲名称 */
const char *Buzzer_GetSongName(void)
{
    if (Buzzer_CurSongIdx >= BUZZER_SONG_COUNT) {
        return "?";
    }
    return Buzzer_SongList[Buzzer_CurSongIdx].Name;
}

/**
 * @brief 按索引开始循环播放歌曲 (越界自动回 0)
 * @param idx 歌曲索引 0..Buzzer_GetSongCount()-1
 */
void Buzzer_PlaySongByIndex(uint8_t idx)
{
    if (idx >= BUZZER_SONG_COUNT) {
        idx = 0;
    }
    /* 先立即静音, 显示/歌曲名马上切到新歌, 播放延时 300ms 再开始 */
    Buzzer_Off();
    Buzzer_Playing = 1;
    Buzzer_Song    = 0;
    Buzzer_CurSongIdx = idx;
    Buzzer_PendingSwitch = 1;
    Buzzer_PendingAtMs   = System_GetMs() + SONG_SWITCH_DELAY_MS;
}

/** @brief 切到下一首并开始播放 (循环) */
void Buzzer_NextSong(void)
{
    Buzzer_PlaySongByIndex((uint8_t)((Buzzer_CurSongIdx + 1) % BUZZER_SONG_COUNT));
}

/** @brief 切到上一首并开始播放 (循环) */
void Buzzer_PrevSong(void)
{
    Buzzer_PlaySongByIndex((uint8_t)((Buzzer_CurSongIdx + BUZZER_SONG_COUNT - 1) % BUZZER_SONG_COUNT));
}
