/**
 ****************************************************************************************************
 * @file        eeprom.c
 * @brief       EEPROM stub(RAM 版): 触摸校准参数保存用, 本工程无 AT24Cxx 硬件,
 *              用 RAM 模拟(断电丢失). 电容屏不需要校准, 此 stub 仅满足链接.
 ****************************************************************************************************
 */

#include "eeprom.h"
#include "string.h"

#define EEPROM_RAM_SIZE     256U

static uint8_t g_eeprom_ram[EEPROM_RAM_SIZE];   /* RAM 模拟存储 */

/**
 * @brief   初始化EEPROM(stub: 清零RAM)
 * @retval  无
 */
void eeprom_init(void)
{
    memset(g_eeprom_ram, 0xFF, sizeof(g_eeprom_ram));
}

/**
 * @brief   检查EEPROM(stub: 始终就绪)
 * @retval  0, 就绪
 */
uint8_t eeprom_check(void)
{
    return 0;
}

/**
 * @brief   读EEPROM(stub: 读RAM)
 * @param   addr : 起始地址
 * @param   buf  : 数据缓冲区
 * @param   size : 读取长度
 * @retval  无
 */
void eeprom_read(uint8_t addr, uint8_t *buf, uint16_t size)
{
    uint16_t i;

    for (i = 0; (i < size) && (addr + i < EEPROM_RAM_SIZE); i++)
    {
        buf[i] = g_eeprom_ram[addr + i];
    }
}

/**
 * @brief   写EEPROM(stub: 写RAM)
 * @param   addr : 起始地址
 * @param   buf  : 数据缓冲区
 * @param   size : 写入长度
 * @retval  无
 */
void eeprom_write(uint8_t addr, uint8_t *buf, uint16_t size)
{
    uint16_t i;

    for (i = 0; (i < size) && (addr + i < EEPROM_RAM_SIZE); i++)
    {
        g_eeprom_ram[addr + i] = buf[i];
    }
}
