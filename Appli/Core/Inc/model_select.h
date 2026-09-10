/**
 ****************************************************************************************************
 * @file        model_select.h
 * @brief       远程OTA - AI模型选择页面(触屏)
 * @attention   依赖: rgblcd(RGB屏) + touch(电容触摸GT9xxx)
 ****************************************************************************************************
 */

#ifndef __MODEL_SELECT_H
#define __MODEL_SELECT_H

#include "main.h"

/* 模型列表项数 */
#define MODEL_SEL_NUM       5

void model_select_run(void);    /* 运行模型选择页面(内部死循环) */

#endif
