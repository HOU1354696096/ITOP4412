/**
 * @file    exynos4412_ddr.h
 * @brief   Exynos4412 DDR 库头文件 (STM32 库风格)
 *
 * Exynos4412 有 2 个动态内存控制器 DMC0 / DMC1:
 *   DMC0 = 0x10600000, DMC1 = 0x10610000
 * POP 封装对应的存储器件是 LPDDR2, DDR 起始地址 0x40000000。
 */
#ifndef __EXYNOS4412_DDR_H
#define __EXYNOS4412_DDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/*------------------- DMC 寄存器组 -------------------*/
typedef struct {
    __IO uint32_t CONCONTROL;    /* 0x00 控制 */
    __IO uint32_t MEMCONTROL;    /* 0x04 存储器控制 */
    __IO uint32_t MEMCONFIG0;    /* 0x08 存储器配置 0 */
    __IO uint32_t MEMCONFIG1;    /* 0x0C 存储器配置 1 */
    __IO uint32_t DIRECTCMD;     /* 0x10 直接命令 */
    __IO uint32_t PRECHCONFIG;   /* 0x14 预充电配置 */
    __IO uint32_t PHYCONTROL0;   /* 0x18 PHY 控制 0 */
    __IO uint32_t PHYCONTROL1;   /* 0x1C PHY 控制 1 */
    __IO uint32_t PHYCONTROL2;   /* 0x20 PHY 控制 2 */
    __IO uint32_t PHYCONTROL3;   /* 0x24 PHY 控制 3 */
    __IO uint32_t PWRDNCONFIG;   /* 0x28 掉电配置 */
    __IO uint32_t RESERVED0;     /* 0x2C */
    __IO uint32_t TIMINGREF;     /* 0x30 刷新时序 */
    __IO uint32_t TIMINGROW;     /* 0x34 行时序 */
    __IO uint32_t TIMINGDATA;    /* 0x38 数据时序 */
    __IO uint32_t TIMINGPOWER;   /* 0x3C 电源时序 */
    __I  uint32_t PHYSTATUS;     /* 0x40 PHY 状态 */
    __IO uint32_t PHYZQCONTROL;  /* 0x44 ZQ 校准控制 */
    __I  uint32_t CHIP0STATUS;   /* 0x48 片选0状态 */
    __I  uint32_t CHIP1STATUS;   /* 0x4C 片选1状态 */
    __I  uint32_t AREFSTATUS;    /* 0x50 自动刷新状态 */
    __I  uint32_t MRSTATUS;      /* 0x54 MR 状态 */
    __I  uint32_t PHYTEST0;      /* 0x58 PHY 测试 0 */
    __I  uint32_t PHYTEST1;      /* 0x5C PHY 测试 1 */
    __IO uint32_t QOSCONTROL0;   /* 0x60 QoS 控制 0 */
    __IO uint32_t QOSCONFIG0;    /* 0x64 QoS 配置 0 */
    __IO uint32_t QOSCONTROL1;   /* 0x68 */
    __IO uint32_t QOSCONFIG1;    /* 0x6C */
    __IO uint32_t QOSCONTROL2;   /* 0x70 */
    __IO uint32_t QOSCONFIG2;    /* 0x74 */
    __IO uint32_t QOSCONTROL3;   /* 0x78 */
    __IO uint32_t QOSCONFIG3;    /* 0x7C */
    __IO uint32_t QOSCONTROL4;   /* 0x80 */
    __IO uint32_t QOSCONFIG4;    /* 0x84 */
    __IO uint32_t QOSCONTROL5;   /* 0x88 */
    __IO uint32_t QOSCONFIG5;    /* 0x8C */
    __IO uint32_t QOSCONTROL6;   /* 0x90 */
    __IO uint32_t QOSCONFIG6;    /* 0x94 */
    __IO uint32_t QOSCONTROL7;   /* 0x98 */
    __IO uint32_t QOSCONFIG7;    /* 0x9C */
    __IO uint32_t QOSCONTROL8;   /* 0xA0 */
    __IO uint32_t QOSCONFIG8;    /* 0xA4 */
    __IO uint32_t QOSCONTROL9;   /* 0xA8 */
    __IO uint32_t QOSCONFIG9;    /* 0xAC */
    __IO uint32_t QOSCONTROL10;  /* 0xB0 */
    __IO uint32_t QOSCONFIG10;   /* 0xB4 */
    __IO uint32_t QOSCONTROL11;  /* 0xB8 */
    __IO uint32_t QOSCONFIG11;   /* 0xBC */
    __IO uint32_t QOSCONTROL12;  /* 0xC0 */
    __IO uint32_t QOSCONFIG12;   /* 0xC4 */
    __IO uint32_t QOSCONTROL13;  /* 0xC8 */
    __IO uint32_t QOSCONFIG13;   /* 0xCC */
    __IO uint32_t QOSCONTROL14;  /* 0xD0 */
    __IO uint32_t QOSCONFIG14;   /* 0xD4 */
    __IO uint32_t QOSCONTROL15;  /* 0xD8 */
    __IO uint32_t QOSCONFIG15;   /* 0xDC */
    __IO uint32_t QOSTIMEOUT0;   /* 0xE0 */
    __IO uint32_t QOSTIMEOUT1;   /* 0xE4 */
    __IO uint32_t RESERVED2[2];  /* 0xE8 */
    __IO uint32_t IVCONTROL;     /* 0xF0 DMC 间交织控制 */
} DMC_TypeDef;

/*------------------- DMC 外设地址 -------------------*/
#define DMC0    ((DMC_TypeDef *)EXYNOS4412_DMC0_BASE)
#define DMC1    ((DMC_TypeDef *)EXYNOS4412_DMC1_BASE)

/*------------------- 函数声明 -------------------*/
void     DDR_Init(void);                  /* 自动检测 1GB/2GB 并初始化 DMC0+DMC1 */
void     DDR_InitEx(uint32_t ram_size_mb);/* 指定容量: 1024/2048, 0=自动 */
void     DDR_DllStartPost(void);          /* 时钟切换后的 DLL 启动(官方 system_clock_init 尾块) */

void     DDR_WriteByte(uint32_t addr, uint8_t data);
uint8_t  DDR_ReadByte(uint32_t addr);
void     DDR_WriteWord(uint32_t addr, uint32_t data);
uint32_t DDR_ReadWord(uint32_t addr);
void     DDR_WriteBuffer(uint32_t addr, const uint8_t *buf, uint32_t len);
void     DDR_ReadBuffer(uint32_t addr, uint8_t *buf, uint32_t len);
void     DDR_MemSet(uint32_t addr, uint8_t val, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_DDR_H */
