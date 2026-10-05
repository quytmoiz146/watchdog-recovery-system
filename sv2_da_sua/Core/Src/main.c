/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "iwdg.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fault_injection.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  RESET_UNKNOWN = 0, RESET_POWER_ON, RESET_PIN, RESET_SOFTWARE,
  RESET_IWDG, RESET_WWDG, RESET_LOW_POWER
} Reset_Cause_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Set to 1 only when deliberately pausing the watchdog at a breakpoint. */
#ifndef SV2_FREEZE_IWDG_ON_DEBUG
#define SV2_FREEZE_IWDG_ON_DEBUG 0
#endif
#define WATCHDOG_REFRESH_MS 100U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Snapshot before RMVF clears hardware flags. Visible in Keil Watch. */
volatile uint32_t g_reset_flags;
volatile Reset_Cause_t g_reset_cause;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static Reset_Cause_t Reset_Decode(uint32_t flags);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t t_led;
  uint32_t t_refresh;
  uint32_t last_scan;
  uint8_t startup_toggles;

  g_reset_flags = RCC->CSR;
  g_reset_cause = Reset_Decode(g_reset_flags);
  __HAL_RCC_CLEAR_RESET_FLAGS();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
#if SV2_FREEZE_IWDG_ON_DEBUG
  __HAL_DBGMCU_FREEZE_IWDG();
#else
  __HAL_DBGMCU_UNFREEZE_IWDG();
#endif

  Fault_Init();
  /* HAL timer init generates an update event. Discard it before scanning. */
  __HAL_TIM_SET_COUNTER(&htim2, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
  HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
    Error_Handler();
  }
  t_led = HAL_GetTick();
  t_refresh = t_led;
  last_scan = Fault_GetScanCount();
  /* Active-low PC13: 3 short flashes for IWDG, 2 software, 1 other boot. */
  startup_toggles = (g_reset_cause == RESET_IWDG) ? 6U :
                    ((g_reset_cause == RESET_SOFTWARE) ? 4U : 2U);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now;
    uint32_t scans;
    Fault_Process();
    now = HAL_GetTick();

    if ((uint32_t)(now - t_led) >= ((startup_toggles != 0U) ? 100U : 500U)) {
      t_led = now;
      HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
      if (startup_toggles != 0U) {
        --startup_toggles;
      }
    }

    /* Standalone SV2 demo: both main and the button scanner must progress.
       In the group project, SV1 also checks the sensor/display task health. */
    scans = Fault_GetScanCount();
    if (((uint32_t)(now - t_refresh) >= WATCHDOG_REFRESH_MS) &&
        (scans != last_scan) && (Fault_GetActive() == FAULT_NONE)) {
      if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK) {
        Error_Handler();
      }
      last_scan = scans;
      t_refresh = now;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2) {
    Fault_Scan_10ms();
  }
}

static Reset_Cause_t Reset_Decode(uint32_t flags)
{
  /* PINRSTF can accompany other sources; give the specific source priority. */
  if ((flags & RCC_CSR_IWDGRSTF) != 0U) { return RESET_IWDG; }
  if ((flags & RCC_CSR_WWDGRSTF) != 0U) { return RESET_WWDG; }
  if ((flags & RCC_CSR_SFTRSTF) != 0U)  { return RESET_SOFTWARE; }
  if ((flags & RCC_CSR_PORRSTF) != 0U) { return RESET_POWER_ON; }
  if ((flags & RCC_CSR_LPWRRSTF) != 0U) { return RESET_LOW_POWER; }
  if ((flags & RCC_CSR_PINRSTF) != 0U) { return RESET_PIN; }
  return RESET_UNKNOWN;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
