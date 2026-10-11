/**
 * @file    iwdg_monitor.c
 * @brief   SV1 - giam sat va nap lai IWDG.
 */
#include "iwdg_monitor.h"

static IWDG_HandleTypeDef *s_hiwdg;
static volatile uint32_t   s_required_mask;
static volatile uint32_t   s_alive_mask;
static volatile uint32_t   s_last_refresh_tick;

static uint32_t lock(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  return primask;
}

static void unlock(uint32_t primask)
{
  __set_PRIMASK(primask);
}

void IWDG_Monitor_Init(IWDG_HandleTypeDef *hiwdg)
{
  s_hiwdg = hiwdg;
  s_required_mask = 0U;
  s_alive_mask = 0U;
  s_last_refresh_tick = HAL_GetTick();
#if IWDG_MONITOR_FREEZE_ON_DEBUG
  __HAL_DBGMCU_FREEZE_IWDG();
#endif
}

void IWDG_Monitor_Register(uint8_t task_id)
{
  uint32_t primask;

  if (task_id >= IWDG_MONITOR_MAX_TASKS)
  {
    return;
  }
  primask = lock();
  s_required_mask |= (1UL << task_id);
  unlock(primask);
}

void IWDG_Monitor_Checkin(uint8_t task_id)
{
  uint32_t primask;

  if (task_id >= IWDG_MONITOR_MAX_TASKS)
  {
    return;
  }
  primask = lock();
  s_alive_mask |= (1UL << task_id);
  unlock(primask);
}

void IWDG_Monitor_Feed(void)
{
  uint32_t primask;
  uint32_t all_alive;

  if (s_hiwdg == NULL)
  {
    return;
  }
  primask = lock();
  all_alive = ((s_alive_mask & s_required_mask) == s_required_mask);
  if (all_alive)
  {
    s_alive_mask = 0U;
  }
  unlock(primask);

  if (all_alive)
  {
    (void)HAL_IWDG_Refresh(s_hiwdg);
    s_last_refresh_tick = HAL_GetTick();
  }
}

/* Nguong tinh theo timeout ngan nhat (LSI nhanh nhat) */
HealthStatus_t IWDG_Monitor_GetHealth(void)
{
  uint32_t elapsed = HAL_GetTick() - s_last_refresh_tick;
  uint32_t worst = IWDG_Monitor_GetTimeoutMs(IWDG_LSI_MAX_HZ);

  if (elapsed >= (worst * 3U) / 4U)
  {
    return HEALTH_CRITICAL;
  }
  if (elapsed >= worst / 2U)
  {
    return HEALTH_WARNING;
  }
  return HEALTH_OK;
}

uint32_t IWDG_Monitor_GetMissingMask(void)
{
  return s_required_mask & ~s_alive_mask;
}

/* he so chia = 4 << PR */
uint32_t IWDG_Monitor_GetTimeoutMs(uint32_t lsi_hz)
{
  if ((s_hiwdg == NULL) || (lsi_hz == 0U))
  {
    return 0U;
  }
  return ((s_hiwdg->Init.Reload + 1UL) * (4UL << s_hiwdg->Init.Prescaler) * 1000UL) / lsi_hz;
}
