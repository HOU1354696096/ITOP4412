/**
 * @file    panel.h
 * @brief   屏幕分块状态面板模块头文件
 *
 * 在 800x1280 LVDS 屏上绘制分栏状态面板:
 *   - 左半边: UART 串口状态块 (U2/U3 收发计数 + RX LOG)
 *   - 右半边从上到下: LED 块 / BUZZER 块 / SYSTEM 块
 */
#ifndef __PANEL_H
#define __PANEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "exynos4412.h"

/** @brief 初始化面板: 清屏 + 画顶栏 + 画 4 个分块边框与标题 */
void Panel_Init(void);

/**
 * @brief 刷新面板内容 (每 300ms 重绘一次, 非阻塞调度)
 * @param nowMs 当前毫秒 (System_GetMs())
 */
void Panel_Update(uint32_t nowMs);

/** @brief 通知面板: UART3 收到一个字符 (用于接收统计与日志显示) */
void Panel_NotifyUartRx(char ch);

/** @brief 通知面板: UART2 调试口收到一个字符 (只写日志, 不计入 UART3 统计) */
void Panel_NotifyUart2Rx(char ch);

/** @brief 通知面板: UART2 又完成一次心跳打印 */
void Panel_NotifyUartTx(void);

/** @brief 获取 UART3 已接收字符计数 (供主程序串口心跳打印, 便于核对屏幕显示) */
uint32_t Panel_GetUartRxCount(void);

/** @brief 获取 UART2 调试口已接收字符计数 (供主程序串口心跳打印, 便于核对屏幕显示) */
uint32_t Panel_GetUart2RxCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __PANEL_H */
