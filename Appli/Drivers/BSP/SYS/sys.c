/**
 ****************************************************************************************************
 * @file        sys.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       SYS驱动代码
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

#include "sys.h"

/**
 * @brief   配置系统时钟
 * @param   无
 * @retval  无
 */
void sys_clock_config_debug(void)
{
    RCC_OscInitTypeDef rcc_osc_init_struct = {0};
    RCC_ClkInitTypeDef rcc_clk_init_struct = {0};

    HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY);

    rcc_osc_init_struct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    rcc_osc_init_struct.HSIState = RCC_HSI_ON;
    rcc_osc_init_struct.HSIDiv = RCC_HSI_DIV1;
    rcc_osc_init_struct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    rcc_osc_init_struct.PLL1.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL2.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL3.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL4.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&rcc_osc_init_struct);

    HAL_RCC_GetClockConfig(&rcc_clk_init_struct);
    if ((rcc_clk_init_struct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) || (rcc_clk_init_struct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
    {
        rcc_clk_init_struct.ClockType = RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK;
        rcc_clk_init_struct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
        rcc_clk_init_struct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        HAL_RCC_ClockConfig(&rcc_clk_init_struct);
    }

    rcc_osc_init_struct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
    rcc_osc_init_struct.PLL1.PLLState = RCC_PLL_ON;
    rcc_osc_init_struct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
    rcc_osc_init_struct.PLL1.PLLM = 4;
    rcc_osc_init_struct.PLL1.PLLN = 75;
    rcc_osc_init_struct.PLL1.PLLFractional = 0;
    rcc_osc_init_struct.PLL1.PLLP1 = 1;
    rcc_osc_init_struct.PLL1.PLLP2 = 1;
    rcc_osc_init_struct.PLL2.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL3.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL4.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&rcc_osc_init_struct);

    rcc_clk_init_struct.ClockType = RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK5 | RCC_CLOCKTYPE_PCLK4;
    rcc_clk_init_struct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
    rcc_clk_init_struct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
    rcc_clk_init_struct.AHBCLKDivider = RCC_HCLK_DIV2;
    rcc_clk_init_struct.APB1CLKDivider = RCC_APB1_DIV1;
    rcc_clk_init_struct.APB2CLKDivider = RCC_APB2_DIV1;
    rcc_clk_init_struct.APB4CLKDivider = RCC_APB4_DIV1;
    rcc_clk_init_struct.APB5CLKDivider = RCC_APB5_DIV1;
    rcc_clk_init_struct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC1Selection.ClockDivider = 2;
    rcc_clk_init_struct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC2Selection.ClockDivider = 3;
    rcc_clk_init_struct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC6Selection.ClockDivider = 4;
    rcc_clk_init_struct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC11Selection.ClockDivider = 3;
    HAL_RCC_ClockConfig(&rcc_clk_init_struct);
}

/**
 * @brief   微秒级延时
 * @param   us: 延时时间
 * @retval  无
 */
void sys_delay_us(uint32_t us)
{
    uint32_t reload;
    uint32_t ticks;
    uint32_t told;
    uint32_t tnow;
    uint32_t tcnt = 0;

    reload = SysTick->LOAD;
    ticks = us * (SystemCoreClock / 1000000);
    told = SysTick->VAL;

    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;
            }
            else
            {
                tcnt += reload - tnow + told;
            }

            told = tnow;

            if (tcnt >= ticks)
            {
                break;
            }
        }
    }
}
