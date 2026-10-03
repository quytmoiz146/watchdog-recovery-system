/* USER CODE BEGIN Header */

/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "watchdog_manager.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* USER CODE BEGIN PD */

#define DEMO_BTN_STOPS_HEARTBEAT   1
#define UART_LOG_TIMEOUT_MS        20U

#define HEARTBEAT_TOGGLE_MS        500U
#define UART_ALIVE_LOG_MS          1000U
#define CYCLES_OF(ms)              ((((ms) / WDG_TASK_PERIOD_MS) > 0U) ? ((ms) / WDG_TASK_PERIOD_MS) : 1U)
/* USER CODE END PD */

/* USER CODE BEGIN PM */

/* USER CODE END PM */

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;

WWDG_HandleTypeDef hwwdg;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

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

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
  uint8_t was_iwdg_reset;
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */

  if (WDG_BootStart() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  WDG_BootKick();
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  (void)MX_WWDG_Init;

  was_iwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) ? 1U : 0U;
  __HAL_RCC_CLEAR_RESET_FLAGS();

  Uart_Puts(was_iwdg_reset ? "\r\n[BOOT] Reset do IWDG\r\n"
                           : "\r\n[BOOT] Khoi dong binh thuong (Power-on / Reset khac)\r\n");
  Uart_Puts("[WDG] BOOT: IWDG chay tu truoc SystemClock_Config, timeout boot ");
  Uart_PutU32(WDG_BOOT_TIMEOUT_MS);
  Uart_Puts(" ms\r\n[WDG] Dang do LSI bang RTC...\r\n");

  (void)WDG_MeasureLsi();
  WDG_BootKick();

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

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    Task_Sched();
    Task_Heartbeat();
    Task_Uart();

    WDG_Supervise();
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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

static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

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

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_HEARTBEAT_GPIO_Port, LED_HEARTBEAT_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LED_HEARTBEAT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_HEARTBEAT_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BTN_FAULT1_Pin|BTN_FAULT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

  if (HAL_GPIO_ReadPin(BTN_FAULT1_GPIO_Port, BTN_FAULT1_Pin) == GPIO_PIN_RESET)
  {
    return;
  }
#endif
  WDG_Checkin(WDG_TASK_HEARTBEAT);
}

static void Task_Uart(void)
{
  static uint32_t next = 0U;
  static uint32_t cnt  = 0U;
  uint32_t now = HAL_GetTick();

  if ((int32_t)(now - next) < 0) { return; }
  next = now + WDG_TASK_PERIOD_MS;

  if (huart1.gState != HAL_UART_STATE_READY) { return; }

  cnt++;
  if ((cnt % CYCLES_OF(UART_ALIVE_LOG_MS)) == 0U)
  {
    static const char msg[] = "[WDG] alive\r\n";
    if (HAL_UART_Transmit(&huart1, (uint8_t *)msg, (uint16_t)(sizeof(msg) - 1U),
                          UART_LOG_TIMEOUT_MS) != HAL_OK)
    {
      return;
    }
  }
  WDG_Checkin(WDG_TASK_UART);
}

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

    started = 1U;
    last    = now;
  }

  delta = now - last;
  last  = now;
  if (delta > WDG_HEALTHY_CYCLE_MAX_MS) { return; }

  WDG_Checkin(WDG_TASK_SCHED);
}

void WDG_OnUnhealthy(uint32_t missing_mask, uint32_t ms_since_refresh)
{
  Uart_Puts("[WDG] CANH BAO: ");  Uart_PutU32(ms_since_refresh);
  Uart_Puts(" ms chua refresh, thieu tac vu mask=0x"); Uart_PutHex8(missing_mask);
  Uart_Puts(" -> sap reset\r\n");
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  if (huart1.Instance != NULL)
  {
    Uart_Puts("\r\n[FATAL] Error_Handler - dung nap IWDG, cho reset\r\n");
  }
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */
}
#endif
