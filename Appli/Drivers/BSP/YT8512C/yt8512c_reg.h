/**
 ****************************************************************************************************
 * @file        yt8512c_reg.h
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

#ifndef __YT8512C_REG_H
#define __YT8512C_REG_H

#define YT8512C_BCR                                             ((uint32_t)0x00U)
#define YT8512C_BSR                                             ((uint32_t)0x01U)
#define YT8512C_PHYSCSR                                         ((uint32_t)0x11U)

#define YT8512C_BCR_SPEED_SELECTION_MSB_Pos                     (6U)
#define YT8512C_BCR_SPEED_SELECTION_MSB_Msk                     (0x1UL << YT8512C_BCR_SPEED_SELECTION_MSB_Pos)
#define YT8512C_BCR_SPEED_SELECTION_MSB                         YT8512C_BCR_SPEED_SELECTION_MSB_Msk
#define YT8512C_BCR_DUPLEX_MODE_Pos                             (8U)
#define YT8512C_BCR_DUPLEX_MODE_Msk                             (0x1UL << YT8512C_BCR_DUPLEX_MODE_Pos)
#define YT8512C_BCR_DUPLEX_MODE                                 YT8512C_BCR_DUPLEX_MODE_Msk
#define YT8512C_BCR_AUTONEGO_EN_Pos                             (12U)
#define YT8512C_BCR_AUTONEGO_EN_Msk                             (0x1UL << YT8512C_BCR_AUTONEGO_EN_Pos)
#define YT8512C_BCR_AUTONEGO_EN                                 YT8512C_BCR_AUTONEGO_EN_Msk
#define YT8512C_BCR_SPEED_SELECTION_LSB_Pos                     (13U)
#define YT8512C_BCR_SPEED_SELECTION_LSB_Msk                     (0x1UL << YT8512C_BCR_SPEED_SELECTION_LSB_Pos)
#define YT8512C_BCR_SPEED_SELECTION_LSB                         YT8512C_BCR_SPEED_SELECTION_LSB_Msk

#define YT8512C_BSR_LINK_STATUS_Pos                             (2U)
#define YT8512C_BSR_LINK_STATUS_Msk                             (0x1UL << YT8512C_BSR_LINK_STATUS_Pos)
#define YT8512C_BSR_LINK_STATUS                                 YT8512C_BSR_LINK_STATUS_Msk

#define YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_Pos           (11U)
#define YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_Msk           (0x1UL << YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_Pos)
#define YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED               YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_Msk
#define YT8512C_PHYSCSR_DUPLEX_Pos                              (13U)
#define YT8512C_PHYSCSR_DUPLEX_Msk                              (0x1UL << YT8512C_PHYSCSR_DUPLEX_Pos)
#define YT8512C_PHYSCSR_DUPLEX                                  YT8512C_PHYSCSR_DUPLEX_Msk
#define YT8512C_PHYSCSR_SPEED_MODE_Pos                          (14U)
#define YT8512C_PHYSCSR_SPEED_MODE_Msk                          (0x3UL << YT8512C_PHYSCSR_SPEED_MODE_Pos)
#define YT8512C_PHYSCSR_SPEED_MODE_0                            (0x1UL << YT8512C_PHYSCSR_SPEED_MODE_Pos)
#define YT8512C_PHYSCSR_SPEED_MODE_1                            (0x2UL << YT8512C_PHYSCSR_SPEED_MODE_Pos)

#define YT8512C_BCR_SPEED_SELECTION_10MBITS                     0x00000000U
#define YT8512C_BCR_SPEED_SELECTION_100MBITS                    YT8512C_BCR_SPEED_SELECTION_LSB
#define YT8512C_BCR_SPEED_SELECTION_1000MBITS                   YT8512C_BCR_SPEED_SELECTION_MSB
#define YT8512C_BCR_DUPLEX_MODE_HALF                            0x00000000U
#define YT8512C_BCR_DUPLEX_MODE_FULL                            YT8512C_BCR_DUPLEX_MODE
#define YT8512C_BCR_AUTONEGO_EN_DISABLE                         0x00000000U
#define YT8512C_BCR_AUTONEGO_EN_ENABLE                          YT8512C_BCR_AUTONEGO_EN

#define YT8512C_BSR_LINK_STATUS_DOWN                            0x00000000U
#define YT8512C_BSR_LINK_STATUS_UP                              YT8512C_BSR_LINK_STATUS

#define YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_NOT_RESOLVED  0x00000000U
#define YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED_RESOLVED      YT8512C_PHYSCSR_SPEED_AND_DUPLEX_RESOLVED
#define YT8512C_PHYSCSR_DUPLEX_HALF                             0x00000000U
#define YT8512C_PHYSCSR_DUPLEX_FULL                             YT8512C_PHYSCSR_DUPLEX
#define YT8512C_PHYSCSR_SPEED_MODE_10MBITS                      0x00000000U
#define YT8512C_PHYSCSR_SPEED_MODE_100MBITS                     YT8512C_PHYSCSR_SPEED_MODE_0
#define YT8512C_PHYSCSR_SPEED_MODE_1000MBITS                    YT8512C_PHYSCSR_SPEED_MODE_1

#endif
