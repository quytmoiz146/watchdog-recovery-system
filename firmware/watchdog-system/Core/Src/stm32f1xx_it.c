/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
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
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "doc_loi.h"
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
static void uart_direct_putc(char c)
{
  if ((RCC->APB2ENR & RCC_APB2ENR_USART1EN) && (USART1->CR1 & USART_CR1_UE)) {
    while (!(USART1->SR & USART_SR_TXE)) {
    }
    USART1->DR = (uint16_t)c;
  }
}

static void uart_direct_puts(const char *s)
{
  while (*s) {
    if (*s == '\n') {
      uart_direct_putc('\r');
    }
    uart_direct_putc(*s++);
  }
}

static void uart_direct_puthex32(uint32_t val)
{
  const char hex[] = "0123456789ABCDEF";
  int i;
  for (i = 7; i >= 0; i--) {
    uart_direct_putc(hex[(val >> (i * 4)) & 0x0F]);
  }
}

void HardFault_Handler_C(uint32_t *stacked_regs)
{
  uint32_t stacked_lr = stacked_regs[5];
  uint32_t stacked_pc = stacked_regs[6];

  /* Bat clock va quyen ghi vao Backup Register */
  __HAL_RCC_BKP_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
  PWR->CR |= PWR_CR_DBP;

  /* Luu thong tin su co vao BKP (khong bi xoa khi Watchdog reset) */
  BKP->DR1 = (uint16_t)FAULT_HARDFAULT;
  BKP->DR3 = (uint16_t)(stacked_pc & 0xFFFFU);
  BKP->DR4 = (uint16_t)((stacked_pc >> 16) & 0xFFFFU);
  BKP->DR5 = (uint16_t)(stacked_lr & 0xFFFFU);
  BKP->DR6 = (uint16_t)((stacked_lr >> 16) & 0xFFFFU);
  BKP->DR7 = 0xA55AU; /* Co magic xac nhan da luu PC/LR */

  /* In truc tiep ra UART1 (polling thanh ghi khong phu thuoc ngat/SysTick) */
  uart_direct_puts("\r\n==================================================\r\n");
  uart_direct_puts("  !!! NGOAI LE HARDFAULT - PHAT HIEN SU CO !!!\r\n");
  uart_direct_puts("  PC (Dia chi lenh gay loi): 0x");
  uart_direct_puthex32(stacked_pc);
  uart_direct_puts("\r\n  LR (Dia chi tra ve)      : 0x");
  uart_direct_puthex32(stacked_lr);
  uart_direct_puts("\r\n  Dang cho Watchdog Reset (~1 s) de tu phuc hoi...\r\n");
  uart_direct_puts("==================================================\r\n");

  /* Dung cho Watchdog tran va Reset MCU */
  while (1)
  {
  }
}
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
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
#if defined(__CC_ARM)
__asm void HardFault_Handler(void)
{
  IMPORT HardFault_Handler_C
  TST LR, #4
  ITE EQ
  MRSEQ R0, MSP
  MRSNE R0, PSP
  B HardFault_Handler_C
}
#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
void HardFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4 \n"
    "ite eq \n"
    "mrseq r0, msp \n"
    "mrsne r0, psp \n"
    "b HardFault_Handler_C \n"
  );
}
#else
void HardFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4 \n"
    "ite eq \n"
    "mrseq r0, msp \n"
    "mrsne r0, psp \n"
    "b HardFault_Handler_C \n"
  );
}
#endif

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
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
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
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
void PendSV_Handler(void)
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

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line0 interrupt.
  */
void EXTI0_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI0_IRQn 0 */

  /* USER CODE END EXTI0_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
  /* USER CODE BEGIN EXTI0_IRQn 1 */

  /* USER CODE END EXTI0_IRQn 1 */
}

/**
  * @brief This function handles EXTI line1 interrupt.
  */
void EXTI1_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI1_IRQn 0 */

  /* USER CODE END EXTI1_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
  /* USER CODE BEGIN EXTI1_IRQn 1 */

  /* USER CODE END EXTI1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line2 interrupt.
  */
void EXTI2_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI2_IRQn 0 */

  /* USER CODE END EXTI2_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
  /* USER CODE BEGIN EXTI2_IRQn 1 */

  /* USER CODE END EXTI2_IRQn 1 */
}

/**
  * @brief This function handles EXTI line3 interrupt.
  */
void EXTI3_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI3_IRQn 0 */

  /* USER CODE END EXTI3_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
  /* USER CODE BEGIN EXTI3_IRQn 1 */

  /* USER CODE END EXTI3_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
