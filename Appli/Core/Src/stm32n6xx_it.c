/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32n6xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32n6xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* 相机/CMW 已移除; DCMIPP_IRQHandler 已改为空处理 */
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DCMIPP_HandleTypeDef hdcmipp;
extern ETH_HandleTypeDef heth1;
extern UART_HandleTypeDef huart1;
/* USER CODE BEGIN EV */
extern void _tx_timer_interrupt(void);   /* ThreadX 内核 tick */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/* OTA 调试: 打印异常现场(硬件压栈帧: R0-R3/R12/LR/PC/xPSR)。
 * 串口需已初始化(uart_init, 在 uart_init 之后发生的故障才打得出来)。
 * LED0(PG10, 低亮)慢闪 = 卡在 fault。fault 若发生在 ThreadX 线程(PSP)也能读对 SP。 */
static void ota_fault_dump(const char *why)
{
    volatile uint32_t *f;
    uint32_t sp;

    __DSB();
    sp = (__get_CONTROL() & 2u) ? (uint32_t)__get_PSP() : (uint32_t)__get_MSP();
    f = (volatile uint32_t *)sp;
    printf("\r\n >>> [%s]\r\n"
           "  R0=%08lX  R1=%08lX  R2=%08lX  R3=%08lX\r\n"
           "  R12=%08lX LR=%08lX\r\n"
           "  >>>崩点 PC=%08lX<<<  (0x34------=SRAM 代理内; 0x70------=NOR XIP 恢复失败)\r\n"
           "  xPSR=%08lX  BFARHINT=xPSR[31:16]\r\n",
           why, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
    for (;;)   /* LED0 慢闪 = 卡在 fault, 便于肉眼确认不是死等 */
    {
        *(volatile uint32_t *)0x46020814u &= ~(1u << 10);   /* LED0 亮 */
        { volatile uint32_t d = 2500000u; while (d) d--; }
        *(volatile uint32_t *)0x46020814u |= (1u << 10);    /* LED0 灭 */
        { volatile uint32_t d = 2500000u; while (d) d--; }
    }
}
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  ota_fault_dump("HardFault");
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  ota_fault_dump("MemManage");
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  ota_fault_dump("BusFault");
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  ota_fault_dump("UsageFault");
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  /* USER CODE BEGIN SecureFault_IRQn 0 */

  /* USER CODE END SecureFault_IRQn 0 */
  ota_fault_dump("SecureFault");
  while (1)
  {
    /* USER CODE BEGIN W1_SecureFault_IRQn 0 */
    /* USER CODE END W1_SecureFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
__attribute__((weak)) void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
__attribute__((weak)) void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
  extern volatile uint8_t g_rtos_started;

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  /* 预内核禁止调用 _tx_timer_interrupt():其内部解引用 _tx_timer_current_ptr，
     ThreadX 未初始化时为 NULL -> HardFault(时序相关崩溃,重生成 TIM 时基被删后触发) */
  if (g_rtos_started)
  {
    _tx_timer_interrupt();
  }
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32N6xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32n6xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DCMIPP global interrupt.
  */
void DCMIPP_IRQHandler(void)
{
  /* USER CODE BEGIN DCMIPP_IRQn 0 */

  /* USER CODE END DCMIPP_IRQn 0 */
  /* 相机已移除: DCMIPP 不再使能, 中断不会触发; 保留空处理避免引用已删 CMW */
  /* USER CODE BEGIN DCMIPP_IRQn 1 */

  /* USER CODE END DCMIPP_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles ETH1 global interrupt.
  */
void ETH1_IRQHandler(void)
{
  /* USER CODE BEGIN ETH1_IRQn 0 */

  /* USER CODE END ETH1_IRQn 0 */
  HAL_ETH_IRQHandler(&heth1);
  /* USER CODE BEGIN ETH1_IRQn 1 */

  /* USER CODE END ETH1_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
