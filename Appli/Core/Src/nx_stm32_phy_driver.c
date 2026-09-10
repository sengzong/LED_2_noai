/* nx_stm32_phy_driver.c -- NetX PHY link 状态映射到本板 YT8512C */
#include "nx_stm32_phy_driver.h"
#include "nx_stm32_eth_config.h"
#include "../../Drivers/BSP/YT8512C/yt8512c.h"

int32_t nx_eth_phy_init(void)
{
    yt8512c_init();
    return ETH_PHY_STATUS_OK;
}

int32_t nx_eth_phy_get_link_state(void)
{
    uint8_t link_state = yt8512c_get_link_state();

    if (link_state == YT8512C_LINK_STATE_DOWN)
        return ETH_PHY_STATUS_LINK_DOWN;
    if (link_state == YT8512C_LINK_STATE_100MBITS_FULLDUPLEX)
        return ETH_PHY_STATUS_100MBITS_FULLDUPLEX;
    if (link_state == YT8512C_LINK_STATE_100MBITS_HALFDUPLEX)
        return ETH_PHY_STATUS_100MBITS_HALFDUPLEX;
    if (link_state == YT8512C_LINK_STATE_10MBITS_FULLDUPLEX)
        return ETH_PHY_STATUS_10MBITS_FULLDUPLEX;
    if (link_state == YT8512C_LINK_STATE_10MBITS_HALFDUPLEX)
        return ETH_PHY_STATUS_10MBITS_HALFDUPLEX;
    if (link_state == YT8512C_LINK_STATE_AUTONEGO_NOTDONE)
        return ETH_PHY_STATUS_AUTONEGO_NOT_DONE;
    return ETH_PHY_STATUS_LINK_ERROR;
}

nx_eth_phy_handle_t nx_eth_phy_get_handle(void)
{
    return NULL;
}