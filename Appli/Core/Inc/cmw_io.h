/**
 ****************************************************************************************************
 * @file        cmw_io.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       cmw_io.h文件
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

#ifndef __CMW_IO_H
#define __CMW_IO_H

#define CAMERA_IMX335_ADDRESS           0x34U
#define CAMERA_VD66GY_ADDRESS           0x20U
#define CAMERA_VD66GY_FREQ_IN_HZ        12000000U
#define CAMERA_VD55G1_ADDRESS           0x20U
#define CAMERA_VD55G1_FREQ_IN_HZ        12000000U

#define NRST_CAM_PIN                    GPIO_PIN_4
#define NRST_CAM_PORT                   GPIOG
#define NRST_CAM_GPIO_ENABLE_VDDIO()
#define NRST_CAM_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOG_CLK_ENABLE()

#define EN_CAM_PIN                      GPIO_PIN_6
#define EN_CAM_PORT                     GPIOG
#define EN_CAM_GPIO_ENABLE_VDDIO()
#define EN_CAM_GPIO_CLK_ENABLE()        __HAL_RCC_GPIOG_CLK_ENABLE()

#define CMW_I2C_INIT                    bsp_i2c2_init
#define CMW_I2C_DEINIT                  bsp_i2c2_deinit
#define CMW_I2C_READREG16               bsp_i2c2_read_reg16
#define CMW_I2C_WRITEREG16              bsp_i2c2_write_reg16
#define BSP_GetTick                     bsp_bus_get_tick

#define CSI2_CLK_ENABLE()               __HAL_RCC_CSI_CLK_ENABLE()
#define CSI2_CLK_SLEEP_DISABLE()        __HAL_RCC_CSI_CLK_SLEEP_DISABLE()
#define CSI2_CLK_FORCE_RESET()          __HAL_RCC_CSI_FORCE_RESET()
#define CSI2_CLK_RELEASE_RESET()        __HAL_RCC_CSI_RELEASE_RESET()

#endif
