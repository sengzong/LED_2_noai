/**
 * @brief  I2C2 总线桥接：给 STM32_MW_CAMERA 提供 HAL I2C2 访问（IMX335 传感器）
 *         IMX335(0x34) 与 GT911 触摸(0x14) 共用 I2C2 总线（PD4=SDA/PD14=SCL）。
 */
#include "bsp_bus.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c2;

static uint8_t bsp_i2c2_init_counter = 0;

int32_t bsp_i2c2_init(void)
{
    if (bsp_i2c2_init_counter == 0)
    {
        bsp_i2c2_init_counter++;

        MX_I2C2_Init();
    }

    return 0;
}

int32_t bsp_i2c2_deinit(void)
{
    if (bsp_i2c2_init_counter == 0)
    {
        return 0;
    }

    bsp_i2c2_init_counter--;
    if (bsp_i2c2_init_counter == 0)
    {
        HAL_I2C_DeInit(&hi2c2);
    }

    return 0;
}

int32_t bsp_i2c2_write_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
    if (HAL_I2C_Mem_Write(&hi2c2, address, reg, I2C_MEMADD_SIZE_16BIT, data, length, 1000) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

int32_t bsp_i2c2_read_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
    if (HAL_I2C_Mem_Read(&hi2c2, address, reg, I2C_MEMADD_SIZE_16BIT, data, length, 1000) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

int32_t bsp_bus_get_tick(void)
{
    return (int32_t)HAL_GetTick();
}