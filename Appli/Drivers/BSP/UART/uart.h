/**
 ****************************************************************************************************
 * @file        uart.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       串口驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#ifndef __UART_H
#define __UART_H

#include "main.h"
#include <stdio.h>

/* 硬件定义 */
#define UART_UX         USART1

/* 功能定义 */
#ifndef BSP_UART_RX_DISABLE
#define UART_EN_RX      1                   /* 使能串口接收功能 */
#else
#define UART_EN_RX      0                   /* 禁用串口接收功能 */
#endif
#define RXBUFFERSIZE    1                   /* 串口中断接收缓冲区大小 */
#define UART_REC_LEN    200                 /* 串口接收缓冲区大小 */

/* 变量导出 */
#if UART_EN_RX
extern uint8_t g_rx_buffer[RXBUFFERSIZE];   /* 串口中断接收缓冲区 */
extern uint8_t g_uart_rx_buf[UART_REC_LEN]; /* 串口接收缓冲区 */
extern uint16_t g_uart_rx_sta;              /* 串口接收状态标记 */
#endif

/* 函数声明 */
void uart_init(uint32_t baudrate);             /* 初始化串口 */

#endif
