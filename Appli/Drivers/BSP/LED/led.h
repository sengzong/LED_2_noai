/**
 ****************************************************************************************************
 * @file        led.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       LED驱动代码
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

#ifndef __LED_H
#define __LED_H

#include "main.h"

/* 引脚定义 */
#define LED0_GPIO_PORT  GPIOG
#define LED0_GPIO_PIN   GPIO_PIN_10
#define LED1_GPIO_PORT  GPIOE
#define LED1_GPIO_PIN   GPIO_PIN_10

/* IO操作 */
#define LED0(x)         do { (x) ?                                                              \
                            HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_SET):     \
                            HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_RESET);   \
                        } while (0)
#define LED1(x)         do { (x) ?                                                              \
                            HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET):     \
                            HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET);   \
                        } while (0)
#define LED0_TOGGLE()   do { HAL_GPIO_TogglePin(LED0_GPIO_PORT, LED0_GPIO_PIN); } while (0)
#define LED1_TOGGLE()   do { HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN); } while (0)

/* 函数声明 */
void led_init(void);    /* 初始化LED */

#endif
