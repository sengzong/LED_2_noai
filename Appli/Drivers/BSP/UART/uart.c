/**
 ****************************************************************************************************
 * @file        uart.c
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

#include "uart.h"

/* UART句柄 */
extern UART_HandleTypeDef huart1;

#if UART_EN_RX
/* 串口中断接收缓冲区 */
uint8_t g_rx_buffer[RXBUFFERSIZE];
/* 串口接收缓冲区 */
uint8_t g_uart_rx_buf[UART_REC_LEN];
/* 串口接收状态标记 */
uint16_t g_uart_rx_sta = 0;
#endif

/**
 * @brief   初始化串口
 * @param   baudrate: 通信波特率（单位：bps）
 * @retval  无
 */
void uart_init(uint32_t baudrate)
{
    UNUSED(baudrate);

    setvbuf(stdout, NULL, _IONBF, 0);

#if UART_EN_RX
    /* UART中断接收数据 */
    HAL_UART_Receive_IT(&huart1, g_rx_buffer, sizeof(g_rx_buffer));
#endif
}

#if UART_EN_RX
/**
 * @brief   HAL库UART接收完成回调函数
 * @param   huart: UART句柄指针
 * @retval  无
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART_UX)
    {
        if((g_uart_rx_sta & 0x8000) == 0)
        {
            if(g_uart_rx_sta & 0x4000)
            {
                if(g_rx_buffer[0] != 0x0A)
                {
                    g_uart_rx_sta = 0;
                }
                else
                {
                    g_uart_rx_sta |= 0x8000;
                }
            }
            else
            {
                if(g_rx_buffer[0] == 0x0D)
                {
                    g_uart_rx_sta |= 0x4000;
                }
                else
                {
                    g_uart_rx_buf[g_uart_rx_sta & 0x3FFF] = g_rx_buffer[0];
                    g_uart_rx_sta++;
                    if(g_uart_rx_sta > (UART_REC_LEN - 1))
                    {
                        g_uart_rx_sta = 0;
                    }
                }
            }
        }

        HAL_UART_Receive_IT(&huart1, g_rx_buffer, sizeof(g_rx_buffer));
    }
}
#endif

/**
 * @brief   重定向C库的printf函数到串口
 * @param   无
 * @retval  无
 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);

    return ch;
}
