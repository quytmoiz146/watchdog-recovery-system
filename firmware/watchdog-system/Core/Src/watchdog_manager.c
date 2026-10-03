
#include "watchdog_manager.h"

#ifndef WDG_REG_WRITE
#define WDG_REG_WRITE(reg, val)   (IWDG->reg = (uint32_t)(val))
#endif
#ifndef WDG_REG_READ
#define WDG_REG_READ(reg)         (IWDG->reg)
#endif

#define WDG_KEY_RELOAD            0xAAAAU
#define WDG_KEY_UNLOCK            0x5555U
#define WDG_KEY_START             0xCCCCU

#define WDG_SR_UPDATE_FLAGS       (IWDG_SR_PVU | IWDG_SR_RVU)

#define WDG_UPDATE_TIMEOUT_MS     150UL

#define WDG_PR_LEVELS             7U
#define WDG_DIV_FROM_PR(pr)       (4UL << (pr))

#define WDG_LSI_ACCEPT_MIN_HZ     15000UL
#define WDG_LSI_ACCEPT_MAX_HZ     120000UL

WDG_STATIC_ASSERT(lsi_value_matches, LSI_VALUE == WDG_LSI_NOMINAL_HZ);

static volatile uint32_t   s_alive_mask;
static uint32_t            s_refresh_count;
static uint32_t            s_last_refresh_tick;
static uint8_t             s_warned;
static WDG_Phase           s_phase;
static uint32_t            s_lsi_hz;
static uint8_t             s_lsi_measured;
static uint32_t            s_prescaler_div;
static uint32_t            s_reload;

__STATIC_INLINE uint32_t enter_critical(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

__STATIC_INLINE void exit_critical(uint32_t primask)
{
  if (primask == 0U)
  {
    __enable_irq();
  }
}

#if WDG_KICK_MARKER_ENABLE
static void marker_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  WDG_MARKER_CLK_ENABLE();
  HAL_GPIO_WritePin(WDG_MARKER_PORT, WDG_MARKER_PIN, GPIO_PIN_RESET);
  gpio.Pin   = WDG_MARKER_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(WDG_MARKER_PORT, &gpio);
}
#endif

static void wdg_kick(void)
{
#if WDG_KICK_MARKER_ENABLE
  HAL_GPIO_TogglePin(WDG_MARKER_PORT, WDG_MARKER_PIN);
#endif
  WDG_REG_WRITE(KR, WDG_KEY_RELOAD);
}

static uint8_t wdg_calc(uint32_t lsi_hz, uint32_t target_ms, uint8_t *pr_bits, uint32_t *reload)
{
  uint32_t i;
  uint32_t ticks;

  for (i = 0U; i < WDG_PR_LEVELS; i++)
  {
    ticks = (target_ms * lsi_hz) / (WDG_DIV_FROM_PR(i) * 1000UL);
    if ((ticks >= 2UL) && (ticks <= 4096UL))
    {
      *pr_bits = (uint8_t)i;
      *reload  = ticks - 1UL;
      return 1U;
    }
  }
  return 0U;
}

static HAL_StatusTypeDef wdg_program(uint8_t pr_bits, uint32_t reload)
{
  uint32_t t0;

  WDG_REG_WRITE(KR,  WDG_KEY_UNLOCK);
  WDG_REG_WRITE(PR,  pr_bits);
  WDG_REG_WRITE(RLR, reload);

  t0 = HAL_GetTick();
  while ((WDG_REG_READ(SR) & WDG_SR_UPDATE_FLAGS) != 0U)
  {
    if ((HAL_GetTick() - t0) > WDG_UPDATE_TIMEOUT_MS)
    {
      return HAL_TIMEOUT;
    }
  }

  s_prescaler_div = WDG_DIV_FROM_PR(pr_bits);
  s_reload        = reload;
  wdg_kick();
  return HAL_OK;
}

__WEAK void WDG_OnUnhealthy(uint32_t missing_mask, uint32_t ms_since_refresh)
{
  (void)missing_mask;
  (void)ms_since_refresh;
}

HAL_StatusTypeDef WDG_BootStart(void)
{
  uint8_t  pr;
  uint32_t rl;
  HAL_StatusTypeDef st;

  if (s_phase != WDG_PHASE_OFF)
  {
    return HAL_OK;
  }

  s_alive_mask    = 0U;
  s_refresh_count = 0U;
  s_warned        = 0U;
  s_lsi_hz        = WDG_LSI_NOMINAL_HZ;
  s_lsi_measured  = 0U;

#if WDG_FREEZE_IN_DEBUG
  __HAL_DBGMCU_FREEZE_IWDG();
#endif
#if WDG_KICK_MARKER_ENABLE
  marker_init();
#endif

  if (wdg_calc(WDG_LSI_NOMINAL_HZ, WDG_BOOT_TIMEOUT_MS, &pr, &rl) == 0U)
  {
    pr = (uint8_t)(WDG_PR_LEVELS - 1U);
    rl = 4095UL;
  }

  WDG_REG_WRITE(KR, WDG_KEY_START);
  st = wdg_program(pr, rl);
  if (st != HAL_OK)
  {
    return st;
  }

  s_last_refresh_tick = HAL_GetTick();
  s_phase = WDG_PHASE_BOOT;
  return HAL_OK;
}

void WDG_BootKick(void)
{
  if (s_phase == WDG_PHASE_BOOT)
  {
    wdg_kick();
    s_last_refresh_tick = HAL_GetTick();
  }
}

#if WDG_MEASURE_LSI

static uint8_t rtc_wait_flag(uint32_t flag, uint32_t timeout_ms, uint32_t *tick_out)
{
  uint32_t t0 = HAL_GetTick();

  RTC->CRL &= ~flag;
  while ((RTC->CRL & flag) == 0U)
  {
    if ((HAL_GetTick() - t0) > timeout_ms)
    {
      return 0U;
    }
  }
  if (tick_out != NULL)
  {
    *tick_out = HAL_GetTick();
  }
  return 1U;
}

uint8_t WDG_MeasureLsi(void)
{
  uint32_t t1 = 0U;
  uint32_t t2 = 0U;
  uint32_t dt;
  uint32_t hz;

  if (rtc_wait_flag(RTC_CRL_RSF, WDG_LSI_MEAS_TIMEOUT_MS, NULL) == 0U)
  {
    return 0U;
  }

  if (rtc_wait_flag(RTC_CRL_SECF, WDG_LSI_MEAS_TIMEOUT_MS, &t1) == 0U)
  {
    return 0U;
  }
  if (rtc_wait_flag(RTC_CRL_SECF, WDG_LSI_MEAS_TIMEOUT_MS, &t2) == 0U)
  {
    return 0U;
  }

  dt = t2 - t1;
  if (dt == 0U)
  {
    return 0U;
  }

  hz = (LSI_VALUE * 1000UL) / dt;
  if ((hz < WDG_LSI_ACCEPT_MIN_HZ) || (hz > WDG_LSI_ACCEPT_MAX_HZ))
  {
    return 0U;
  }

  s_lsi_hz       = hz;
  s_lsi_measured = 1U;
  return 1U;
}
#else
uint8_t WDG_MeasureLsi(void)
{
  return 0U;
}
#endif

HAL_StatusTypeDef WDG_Init(void)
{
  uint8_t  pr;
  uint32_t rl;
  uint32_t hz;
  HAL_StatusTypeDef st;

  if (s_phase == WDG_PHASE_OFF)
  {
    st = WDG_BootStart();
    if (st != HAL_OK)
    {
      return st;
    }
  }

#if WDG_AUTO_TRIM_TO_LSI
  hz = s_lsi_hz;
#else
  hz = WDG_LSI_NOMINAL_HZ;
#endif

  if (wdg_calc(hz, WDG_TIMEOUT_TARGET_MS, &pr, &rl) == 0U)
  {
    pr = WDG_PR_BITS;
    rl = WDG_RELOAD_VALUE;
  }

  s_alive_mask    = 0U;
  s_refresh_count = 0U;
  s_warned        = 0U;

  st = wdg_program(pr, rl);
  if (st != HAL_OK)
  {
    return st;
  }

  s_last_refresh_tick = HAL_GetTick();
  s_phase = WDG_PHASE_RUN;
  return HAL_OK;
}

void WDG_Checkin(WDG_TaskId id)
{
  uint32_t pm;

  if ((uint32_t)id >= (uint32_t)WDG_TASK_COUNT)
  {
    return;
  }

  pm = enter_critical();
  s_alive_mask |= (1UL << (uint32_t)id);
  exit_critical(pm);
}

uint8_t WDG_Supervise(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t pm;
  uint32_t alive;
  uint32_t elapsed;

  if (s_phase != WDG_PHASE_RUN)
  {
    return 0U;
  }

  pm    = enter_critical();
  alive = s_alive_mask;

  if ((alive & WDG_ALL_TASKS_MASK) == WDG_ALL_TASKS_MASK)
  {

    s_alive_mask = 0U;
    exit_critical(pm);

    wdg_kick();
    s_refresh_count++;
    s_last_refresh_tick = now;
    s_warned = 0U;
    return 1U;
  }
  exit_critical(pm);

  elapsed = now - s_last_refresh_tick;
  if ((elapsed >= WDG_WARN_AFTER_MS) && (s_warned == 0U))
  {
    s_warned = 1U;
    WDG_OnUnhealthy((~alive) & WDG_ALL_TASKS_MASK, elapsed);
  }
  return 0U;
}

uint32_t WDG_GetMissingMask(void)
{
  return (~s_alive_mask) & WDG_ALL_TASKS_MASK;
}

uint32_t WDG_GetRefreshCount(void)
{
  return s_refresh_count;
}

uint32_t WDG_GetLastRefreshTick(void)
{
  return s_last_refresh_tick;
}

WDG_Phase WDG_GetPhase(void)
{
  return s_phase;
}

uint32_t WDG_GetLsiHz(void)
{
  return (s_lsi_hz != 0U) ? s_lsi_hz : WDG_LSI_NOMINAL_HZ;
}

uint8_t WDG_IsLsiMeasured(void)
{
  return s_lsi_measured;
}

uint32_t WDG_GetPrescalerDiv(void)
{
  return s_prescaler_div;
}

uint32_t WDG_GetReload(void)
{
  return s_reload;
}

uint32_t WDG_GetTimeoutMsAt(uint32_t lsi_hz)
{
  if ((lsi_hz == 0U) || (s_prescaler_div == 0U))
  {
    return 0U;
  }
  return ((s_reload + 1UL) * s_prescaler_div * 1000UL) / lsi_hz;
}

uint32_t WDG_GetTimeoutMs(void)
{
  return WDG_GetTimeoutMsAt(WDG_GetLsiHz());
}
