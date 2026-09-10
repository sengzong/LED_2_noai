/**
 ****************************************************************************************************
 * @file        yt8512c.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       YT8512C驱动代码
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

#ifndef __YT8512C_H
#define __YT8512C_H

#include "main.h"

/* 引脚定义 */
#define YT8512C_RESET_GPIO_PORT GPIOF
#define YT8512C_RESET_GPIO_PIN  GPIO_PIN_5

/* IO操作 */
#define YT8512C_RESET(x)        do { (x) ?                                                                              \
                                    HAL_GPIO_WritePin(YT8512C_RESET_GPIO_PORT, YT8512C_RESET_GPIO_PIN, GPIO_PIN_SET):   \
                                    HAL_GPIO_WritePin(YT8512C_RESET_GPIO_PORT, YT8512C_RESET_GPIO_PIN, GPIO_PIN_RESET); \
                                } while (0)

/* YT8512C定义 */
#define YT8512C_ADDR            ((uint32_t)0U)

/* YT8512C链路状态定义 */
#define YT8512C_LINK_STATE_ERROR                0
#define YT8512C_LINK_STATE_DOWN                 1
#define YT8512C_LINK_STATE_100MBITS_FULLDUPLEX  2
#define YT8512C_LINK_STATE_100MBITS_HALFDUPLEX  3
#define YT8512C_LINK_STATE_10MBITS_FULLDUPLEX   4
#define YT8512C_LINK_STATE_10MBITS_HALFDUPLEX   5
#define YT8512C_LINK_STATE_AUTONEGO_NOTDONE     6

/* 函数声明 */
void yt8512c_init(void);                /* 初始化YT8512C */
uint8_t yt8512c_get_link_state(void);   /* 获取YT8512C链路状态 */

#endif
