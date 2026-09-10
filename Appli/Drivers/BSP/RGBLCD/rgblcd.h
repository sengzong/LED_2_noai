/**
 ****************************************************************************************************
 * @file        rgblcd.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       RGB LCD驱动代码
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

#ifndef __RGBLCD_H
#define __RGBLCD_H

#include "main.h"

/* RGB LCD部分引脚定义 */
#define RGBLCD_BL_GPIO_PORT GPIOA
#define RGBLCD_BL_GPIO_PIN  GPIO_PIN_3

#define RGBLCD_B7_GPIO_PORT GPIOA
#define RGBLCD_B7_GPIO_PIN  GPIO_PIN_2
#define RGBLCD_B7_GPIO_AF   GPIO_AF14_LCD

#define RGBLCD_G7_GPIO_PORT GPIOB
#define RGBLCD_G7_GPIO_PIN  GPIO_PIN_10
#define RGBLCD_G7_GPIO_AF   GPIO_AF14_LCD

#define RGBLCD_R7_GPIO_PORT GPIOG
#define RGBLCD_R7_GPIO_PIN  GPIO_PIN_9
#define RGBLCD_R7_GPIO_AF   GPIO_AF14_LCD

/* RGB LCD引脚操作 */
#define RGBLCD_BL(x)        do { (x) ?                                                                      \
                                HAL_GPIO_WritePin(RGBLCD_BL_GPIO_PORT, RGBLCD_BL_GPIO_PIN, GPIO_PIN_SET):   \
                                HAL_GPIO_WritePin(RGBLCD_BL_GPIO_PORT, RGBLCD_BL_GPIO_PIN, GPIO_PIN_RESET); \
                            } while (0)

/* RGB LCD重要参数集 */
typedef struct
{
    uint16_t id;                    /* LCD ID */
    uint32_t pwidth;                /* LCD面板的宽度 */
    uint32_t pheight;               /* LCD面板的高度 */
    uint16_t hsw;                   /* 水平同步宽度 */
    uint16_t vsw;                   /* 垂直同步宽度 */
    uint16_t hbp;                   /* 水平后廊 */
    uint16_t vbp;                   /* 垂直后廊 */
    uint16_t hfp;                   /* 水平前廊 */
    uint16_t vfp;                   /* 垂直前廊 */
    uint8_t dir;                    /* 横竖屏标志位，0：竖屏；1：横屏 */
    uint16_t width;                 /* LCD宽度 */
    uint16_t height;                /* LCD高度 */
} _rgblcd_dev;

/* 颜色定义 */
#define WHITE               0xFFFF  /* 白色 */
#define BLACK               0x0000  /* 黑色 */
#define RED                 0xF800  /* 红色 */
#define GREEN               0x07E0  /* 绿色 */
#define BLUE                0x001F  /* 蓝色 */
#define MAGENTA             0xF81F  /* 品红色 */
#define YELLOW              0xFFE0  /* 黄色 */
#define CYAN                0x07FF  /* 青色 */
#define BROWN               0xBC40  /* 棕色 */
#define BRRED               0xFC07  /* 棕红色 */
#define GRAY                0x8430  /* 灰色 */
#define DARKBLUE            0x01CF  /* 深蓝色 */
#define LIGHTBLUE           0x7D7C  /* 浅蓝色 */
#define GRAYBLUE            0x5458  /* 灰蓝色 */
#define LIGHTGREEN          0x841F  /* 浅绿色 */
#define LGRAY               0xC618  /* 浅灰色 */
#define LGRAYBLUE           0xA651  /* 浅灰蓝色 */
#define LBBLUE              0x2B12  /* 浅棕蓝色 */

/* RGB LCD参数导出 */
extern _rgblcd_dev rgblcddev;
extern uint32_t g_back_color;
/* 帧缓冲放内部 SRAM: 800x480x2=768KB, 内部 2MB 放得下; 不依赖外部 PSRAM(0x90000000 当前未映射) */
extern uint16_t g_ltdc_lcd_framebuf[800 * 480];

/* 函数声明 */
void rgblcd_init(void);                                                                                                     /* 初始化RGB LED */
void rgblcd_display_dir(uint8_t dir);                                                                                       /* 设置RGB LCD显示方向 */
void rgblcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);                                       /* 在RGB LCD指定区域内填充单个颜色 */
void rgblcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);                                /* 在RGB LCD指定区域内填充指定颜色块 */
void rgblcd_draw_point(uint16_t x, uint16_t y, uint16_t color);                                                             /* RGB LCD画点 */
uint16_t rgblcd_read_point(uint16_t x, uint16_t y);                                                                         /* RGB LCD读点 */
void rgblcd_clear(uint16_t color);                                                                                          /* RGB LCD清屏 */
void rgblcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);                                  /* RGB LCD画线 */
void rgblcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);                                               /* RGB LCD画水平线 */
void rgblcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);                             /* RGB LCD画矩形 */
void rgblcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);                                               /* RGB LCD画圆 */
void rgblcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);                                                /* RGB LCD画实心圆 */
void rgblcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);                        /* RGB LCD在指定位置显示一个字符 */
void rgblcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);                      /* RGB LCD显示len个数字 */
void rgblcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);       /* RGB LCD扩展显示len个数字（显示高位0） */
void rgblcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);    /* RGB LCD显示字符串 */

#endif
