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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "watchdog_manager.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 1: giu nut BTN_FAULT1 (PA0) -> tac vu HEARTBEAT ngung bao "khoe"
 *    => IWDG khong duoc nap lai => MCU reset sau ~1 s (dung de demo chien luoc).
 *    SV2 se thay bang cac kich ban loi day du (treo vong lap, HardFault...). */
#define DEMO_BTN_STOPS_HEARTBEAT   1
#define UART_LOG_TIMEOUT_MS        20U
/* Chu ky nhip LED / log, quy ra so lan chay tac vu (tu theo WDG_TASK_PERIOD_MS, toi thieu 1) */
#define HEARTBEAT_TOGGLE_MS        500U
#define UART_ALIVE_LOG_MS          1000U
#define CYCLES_OF(ms)              ((((ms) / WDG_TASK_PERIOD_MS) > 0U) ? ((ms) / WDG_TASK_PERIOD_MS) : 1U)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;

WWDG_HandleTypeDef hwwdg;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_WWDG_Init(void);
/* USER CODE BEGIN PFP */
static void Uart_Puts(const char *s);
static void Uart_PutU32(uint32_t v);
static void Uart_PutHex8(uint32_t v);
static void Task_Heartbeat(void);
static void Task_Uart(void);
static void Task_Sched(void);
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
  uint8_t was_iwdg_reset;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* SV1 - giai doan BOOT: bat IWDG TRUOC SystemClock_Config.
   * IWDG chay bang LSI nen khong phu thuoc clock he thong => treo ngay o buoc cau hinh
   * clock hay o khoi tao I2C/LCD cung duoc reset. Timeout boot dai (WDG_BOOT_TIMEOUT_MS). */
  if (WDG_BootStart() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  WDG_BootKick();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Luu nguyen nhan reset TRUOC khi xoa co (ban day du do SV3 lam bang RCC + backup register) */
  /* WWDG (cau hinh cua nhom: Prescaler 8, Window 80, Counter 127) CHUA duoc bat o day:
   * HAL_WWDG_Init khoi dong WWDG ngay, ma can nap trong cua so 43..58 ms, neu khong chip
   * reset sau ~58 ms. Phan nay thuoc SV4 (so sanh IWDG/WWDG): khi can, goi MX_WWDG_Init(). */
  (void)MX_WWDG_Init;

  was_iwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) ? 1U : 0U;
  __HAL_RCC_CLEAR_RESET_FLAGS();

  Uart_Puts(was_iwdg_reset ? "\r\n[BOOT] Reset do IWDG\r\n"
                           : "\r\n[BOOT] Khoi dong binh thuong (Power-on / Reset khac)\r\n");
  Uart_Puts("[WDG] BOOT: IWDG chay tu truoc SystemClock_Config, timeout boot ");
  Uart_PutU32(WDG_BOOT_TIMEOUT_MS);
  Uart_Puts(" ms\r\n[WDG] Dang do LSI bang RTC...\r\n");

  /* Do f_LSI that (RTC cung chay bang LSI) de timeout thuc te bam sat muc tieu */
  (void)WDG_MeasureLsi();
  WDG_BootKick();

  /* SV1: WDG_Init la NOI DUY NHAT cau hinh IWDG cho giai doan van hanh
   * (khong con MX_IWDG_Init). Cau hinh lay tu WDG_PROFILE trong watchdog_manager.h. */
  if (WDG_Init() != HAL_OK)
  {
    Error_Handler();
  }

  if (WDG_IsLsiMeasured() != 0U)
  {
    Uart_Puts("[WDG] LSI do duoc = ");  Uart_PutU32(WDG_GetLsiHz());
    Uart_Puts(" Hz (danh dinh ");       Uart_PutU32(WDG_LSI_NOMINAL_HZ);
    Uart_Puts(" Hz)\r\n");
  }
  else
  {
    Uart_Puts("[WDG] Do LSI that bai -> dung danh dinh ");
    Uart_PutU32(WDG_LSI_NOMINAL_HZ);
    Uart_Puts(" Hz, timeout that nam trong ");
    Uart_PutU32(WDG_TIMEOUT_MIN_MS);  Uart_Puts("..");
    Uart_PutU32(WDG_TIMEOUT_MAX_MS);  Uart_Puts(" ms\r\n");
  }

  Uart_Puts("[WDG] RUN: Prescaler=");  Uart_PutU32(WDG_GetPrescalerDiv());
  Uart_Puts(" Reload=");               Uart_PutU32(WDG_GetReload());
  Uart_Puts(" -> timeout=");           Uart_PutU32(WDG_GetTimeoutMs());
  Uart_Puts(" ms (muc tieu ");         Uart_PutU32(WDG_TIMEOUT_TARGET_MS);
  Uart_Puts(" ms)\r\n");
#if WDG_KICK_MARKER_ENABLE
  Uart_Puts("[WDG] Kick marker: PB0 dao muc truoc moi lan nap IWDG\r\n");
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Moi tac vu tu kiem tra suc khoe roi moi WDG_Checkin(). */
    Task_Sched();
    Task_Heartbeat();
    Task_Uart();

    /* Cho DUY NHAT de nap IWDG: chi refresh khi tat ca tac vu da khoe */
    WDG_Supervise();
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
  DateToUpdate.Month = RTC_MONTH_JANUARY;
  DateToUpdate.Date = 0x1;
  DateToUpdate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief WWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_WWDG_Init(void)
{

  /* USER CODE BEGIN WWDG_Init 0 */

  /* USER CODE END WWDG_Init 0 */

  /* USER CODE BEGIN WWDG_Init 1 */

  /* USER CODE END WWDG_Init 1 */
  hwwdg.Instance = WWDG;
  hwwdg.Init.Prescaler = WWDG_PRESCALER_8;
  hwwdg.Init.Window = 80;
  hwwdg.Init.Counter = 127;
  hwwdg.Init.EWIMode = WWDG_EWI_DISABLE;
  if (HAL_WWDG_Init(&hwwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN WWDG_Init 2 */

  /* USER CODE END WWDG_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_HEARTBEAT_GPIO_Port, LED_HEARTBEAT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_HEARTBEAT_Pin */
  GPIO_InitStruct.Pin = LED_HEARTBEAT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_HEARTBEAT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_FAULT1_Pin BTN_FAULT2_Pin */
  GPIO_InitStruct.Pin = BTN_FAULT1_Pin|BTN_FAULT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ---- In log UART1 (blocking, timeout ngan de khong ket lau, khong dung printf) ---- */
static void Uart_Puts(const char *str)
{
  uint16_t len = 0U;
  while (str[len] != '\0') { len++; }
  if (len > 0U)
  {
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)str, len, UART_LOG_TIMEOUT_MS);
  }
}

static void Uart_PutU32(uint32_t v)
{
  char tmp[11];
  char out[11];
  uint8_t i = 0U;
  uint8_t k = 0U;
  do
  {
    tmp[i++] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v != 0U);

  while (i > 0U) { out[k++] = tmp[--i]; }
  out[k] = '\0';
  Uart_Puts(out);
}

static void Uart_PutHex8(uint32_t v)
{
  static const char hex[] = "0123456789ABCDEF";
  char out[3];
  out[0] = hex[(v >> 4) & 0xFU];
  out[1] = hex[v & 0xFU];
  out[2] = '\0';
  Uart_Puts(out);
}

/** Tac vu 1: LED heartbeat (LED_HEARTBEAT, PC13; doi trang thai moi 0.5 s). Chi bao khoe khi chay binh thuong. */
static void Task_Heartbeat(void)
{
  static uint32_t next = 0U;
  static uint32_t cnt  = 0U;
  uint32_t now = HAL_GetTick();

  if ((int32_t)(now - next) < 0) { return; }
  next = now + WDG_TASK_PERIOD_MS;

  cnt++;
  if ((cnt % CYCLES_OF(HEARTBEAT_TOGGLE_MS)) == 0U)
  {
    HAL_GPIO_TogglePin(LED_HEARTBEAT_GPIO_Port, LED_HEARTBEAT_Pin);
  }

#if DEMO_BTN_STOPS_HEARTBEAT
  /* Gia lap "tac vu khong khoe": giu PA0 (muc thap) thi khong check-in */
  if (HAL_GPIO_ReadPin(BTN_FAULT1_GPIO_Port, BTN_FAULT1_Pin) == GPIO_PIN_RESET)
  {
    return;
  }
#endif
  WDG_Checkin(WDG_TASK_HEARTBEAT);
}

/** Tac vu 2: kenh UART. Khoe neu UART san sang va truyen log duoc (khong timeout). */
static void Task_Uart(void)
{
  static uint32_t next = 0U;
  static uint32_t cnt  = 0U;
  uint32_t now = HAL_GetTick();

  if ((int32_t)(now - next) < 0) { return; }
  next = now + WDG_TASK_PERIOD_MS;

  if (huart1.gState != HAL_UART_STATE_READY) { return; }   /* ket/loi -> khong khoe */

  cnt++;
  if ((cnt % CYCLES_OF(UART_ALIVE_LOG_MS)) == 0U)           /* log moi ~1 s */
  {
    static const char msg[] = "[WDG] alive\r\n";
    if (HAL_UART_Transmit(&huart1, (uint8_t *)msg, (uint16_t)(sizeof(msg) - 1U),
                          UART_LOG_TIMEOUT_MS) != HAL_OK)
    {
      return;                                               /* truyen loi -> khong khoe */
    }
  }
  WDG_Checkin(WDG_TASK_UART);
}

/** Tac vu 3: kiem tra time base / bo lap lich (khoang cach giua 2 lan chay hop ly). */
static void Task_Sched(void)
{
  static uint32_t next    = 0U;
  static uint32_t last    = 0U;
  static uint8_t  started = 0U;
  uint32_t now = HAL_GetTick();
  uint32_t delta;

  if ((int32_t)(now - next) < 0) { return; }
  next = now + WDG_TASK_PERIOD_MS;

  if (started == 0U)
  {
    /* Lan chay dau: chua co lan truoc de do khoang cach (thoi gian khoi dong
     * khong phai "tre lap lich") -> lay moc tai day, coi delta = 0. */
    started = 1U;
    last    = now;
  }

  delta = now - last;
  last  = now;
  if (delta > WDG_HEALTHY_CYCLE_MAX_MS) { return; }        /* lap lich bi tre qua muc */

  WDG_Checkin(WDG_TASK_SCHED);
}

/* Hook tu watchdog_manager: IWDG sap het han ma van thieu tac vu khoe */
void WDG_OnUnhealthy(uint32_t missing_mask, uint32_t ms_since_refresh)
{
  Uart_Puts("[WDG] CANH BAO: ");  Uart_PutU32(ms_since_refresh);
  Uart_Puts(" ms chua refresh, thieu tac vu mask=0x"); Uart_PutHex8(missing_mask);
  Uart_Puts(" -> sap reset\r\n");
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* KHONG goi __disable_irq(): can SysTick cho timeout cua HAL_UART_Transmit, va day
   * la he thong tu phuc hoi - de IWDG het han va reset chip thay vi treo im lang.
   * IWDG da duoc bat tu WDG_BootStart() nen moi duong vao day deu duoc reset. */
  if (huart1.Instance != NULL)                     /* UART da khoi tao chua? */
  {
    Uart_Puts("\r\n[FATAL] Error_Handler - dung nap IWDG, cho reset\r\n");
  }
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
