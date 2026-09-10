/*---------------------------------------------------------------------------------------------
#  * Copyright (c) 2023 STMicroelectronics.
#  * All rights reserved.
#  *
#  * This software is licensed under terms that can be found in the LICENSE file in
#  * the root directory of this software component.
#  * If no LICENSE file comes with this software, it is provided AS-IS.
#  *--------------------------------------------------------------------------------------------*/

/* ---------------    Generated code    ----------------- */
#ifndef __POSTPROCESS_CONF_H__
#define __POSTPROCESS_CONF_H__

#include "arm_math.h"

/* I/O configuration */
#define AI_OBJDETECT_YOLOV8_PP_NB_CLASSES        (3)
#define AI_OBJDETECT_YOLOV8_PP_TOTAL_BOXES       (2100)

/* Quantization parameters for INT8 model output */
#define AI_OBJDETECT_YOLOV8_PP_SCALE             (0.005722436f)          /* best_full_integer_quant */
#define AI_OBJDETECT_YOLOV8_PP_ZERO_POINT        (-125)                  /* best_full_integer_quant */

#endif      /* __POSTPROCESS_CONF_H__  */
