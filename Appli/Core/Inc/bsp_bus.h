#ifndef __BSP_BUS_H
#define __BSP_BUS_H

#include <stdint.h>

/* I2C2 总线桥接：CMW_CAMERA (IMX335) 通过这里操作硬件 I2C2 */
int32_t bsp_i2c2_init(void);
int32_t bsp_i2c2_deinit(void);
int32_t bsp_i2c2_write_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length);
int32_t bsp_i2c2_read_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length);
int32_t bsp_bus_get_tick(void);

#endif /* __BSP_BUS_H */