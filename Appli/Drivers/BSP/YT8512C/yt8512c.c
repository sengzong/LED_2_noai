/**
 ****************************************************************************************************
 * @file        yt8512c.c
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

#include "yt8512c.h"
#include "yt8512c_reg.h"

extern ETH_HandleTypeDef heth1;

/**
 * @brief   初始化YT8512C
 * @param   无
 * @retval  无
 */
void yt8512c_init(void)
{
    YT8512C_RESET(0);
    HAL_Delay(50);
    YT8512C_RESET(1);

    HAL_ETH_SetMDIOClockRange(&heth1);
}

/**
 * @brief   获取YT8512C链路状态
 * @param   无
 * @retval  YT8512C链路状态
 */
uint8_t yt8512c_get_link_state(void)
{
    uint32_t data;

    if (HAL_ETH_ReadPHYRegister(&heth1, YT8512C_ADDR, YT8512C_BSR, &data) != HAL_OK)
    {
        return YT8512C_LINK_STATE_ERROR;
    }

    if (HAL_ETH_ReadPHYRegister(&heth1, YT8512C_ADDR, YT8512C_BSR, &data) != HAL_OK)
    {
        return YT8512C_LINK_STATE_ERROR;
    }

    if ((data & YT8512C_BSR_LINK_STATUS_Msk) == YT8512C_BSR_LINK_STATUS_DOWN)
    {
        return YT8512C_LINK_STATE_DOWN;
    }

    if (HAL_ETH_ReadPHYRegister(&heth1, YT8512C_ADDR, YT8512C_BCR, &data) != HAL_OK)
    {
        return YT8512C_LINK_STATE_ERROR;
    }

    if ((data & YT8512C_BCR_AUTONEGO_EN_Msk) != YT8512C_BCR_AUTONEGO_EN_ENABLE)
    {
        if (((data & (YT8512C_BCR_SPEED_SELECTION_MSB_Msk | YT8512C_BCR_SPEED_SELECTION_LSB_Msk)) == YT8512C_BCR_SPEED_SELECTION_100MBITS) && ((data & YT8512C_BCR_DUPLEX_MODE_Msk) == YT8512C_BCR_DUPLEX_MODE_FULL))
        {
            return YT8512C_LINK_STATE_100MBITS_FULLDUPLEX;
        }
        else if ((data & (YT8512C_BCR_SPEED_SELECTION_MSB_Msk | YT8512C_BCR_SPEED_SELECTION_LSB_Msk)) == YT8512C_BCR_SPEED_SELECTION_100MBITS)
        {
            return YT8512C_LINK_STATE_100MBITS_HALFDUPLEX;
        }
        else if ((data & YT8512C_BCR_DUPLEX_MODE_Msk) == YT8512C_BCR_DUPLEX_MODE_FULL)
        {
            return YT8512C_LINK_STATE_10MBITS_FULLDUPLEX;
        }
        else
        {
            return YT8512C_LINK_STATE_10MBITS_HALFDUPLEX;
        }
    }
    else
    {
        if (HAL_ETH_ReadPHYRegister(&heth1, YT8512C_ADDR, YT8512C_PHYSCSR, &data) != HAL_OK)
        {
            return YT8512C_LINK_STATE_ERROR;
        }

        if ((data & YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_Msk) == YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_NOT_RESOLVED)
        {
            return YT8512C_LINK_STATE_AUTONEGO_NOTDONE;
        }

        if (((data & YT8512C_PHYSCSR_SPEED_MODE_Msk) == YT8512C_PHYSCSR_SPEED_MODE_100MBITS) && ((data & YT8512C_PHYSCSR_DUPLEX_Msk) == YT8512C_PHYSCSR_DUPLEX_FULL))
        {
            return YT8512C_LINK_STATE_100MBITS_FULLDUPLEX;
        }
        else if (((data & YT8512C_PHYSCSR_SPEED_MODE_Msk) == YT8512C_PHYSCSR_SPEED_MODE_100MBITS) && ((data & YT8512C_PHYSCSR_DUPLEX_Msk) == YT8512C_PHYSCSR_DUPLEX_HALF))
        {
            return YT8512C_LINK_STATE_100MBITS_HALFDUPLEX;
        }
        else if (((data & YT8512C_PHYSCSR_SPEED_MODE_Msk) == YT8512C_PHYSCSR_SPEED_MODE_10MBITS) && ((data & YT8512C_PHYSCSR_DUPLEX_Msk) == YT8512C_PHYSCSR_DUPLEX_FULL))
        {
            return YT8512C_LINK_STATE_10MBITS_FULLDUPLEX;
        }
        else
        {
            return YT8512C_LINK_STATE_10MBITS_HALFDUPLEX;
        }
    }
}
