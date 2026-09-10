#ifndef __NX_STM32_ETH_CONFIG_H
#define __NX_STM32_ETH_CONFIG_H

#include "stm32n6xx_hal.h"

#define MULTI_QUEUE_FEATURE

extern ETH_HandleTypeDef heth1;
#ifdef NX_DRIVER_ETH_HW_IP_INIT
extern void MX_ETH1_Init(void);
#endif

#define eth_handle heth1
#ifdef NX_DRIVER_ETH_HW_IP_INIT
#define nx_eth_init MX_ETH1_Init
#endif

#endif