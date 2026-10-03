
#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

#define WDG_PROFILE_CUSTOM      0
#define WDG_PROFILE_FAST_0_5S   1
#define WDG_PROFILE_STD_1S      2
#define WDG_PROFILE_SLOW_2S     3
#define WDG_PROFILE_LONG_5S     4

#ifndef WDG_PROFILE
#define WDG_PROFILE             WDG_PROFILE_STD_1S
#endif

#if   WDG_PROFILE == WDG_PROFILE_FAST_0_5S
  #define WDG_TIMEOUT_TARGET_MS     500UL
  #define WDG_TASK_PERIOD_MS         50UL
  #define WDG_HEALTHY_CYCLE_MAX_MS  150UL
  #define WDG_WARN_AFTER_MS         250UL
#elif WDG_PROFILE == WDG_PROFILE_STD_1S
  #define WDG_TIMEOUT_TARGET_MS    1000UL
  #define WDG_TASK_PERIOD_MS        100UL
  #define WDG_HEALTHY_CYCLE_MAX_MS  250UL
  #define WDG_WARN_AFTER_MS         500UL
#elif WDG_PROFILE == WDG_PROFILE_SLOW_2S
  #define WDG_TIMEOUT_TARGET_MS    2000UL
  #define WDG_TASK_PERIOD_MS        100UL
  #define WDG_HEALTHY_CYCLE_MAX_MS  500UL
  #define WDG_WARN_AFTER_MS        1000UL
#elif WDG_PROFILE == WDG_PROFILE_LONG_5S
  #define WDG_TIMEOUT_TARGET_MS    5000UL
  #define WDG_TASK_PERIOD_MS        100UL
  #define WDG_HEALTHY_CYCLE_MAX_MS 1000UL
  #define WDG_WARN_AFTER_MS        2500UL
#elif WDG_PROFILE == WDG_PROFILE_CUSTOM
  #if !defined(WDG_TIMEOUT_TARGET_MS) || !defined(WDG_TASK_PERIOD_MS) || \
      !defined(WDG_HEALTHY_CYCLE_MAX_MS) || !defined(WDG_WARN_AFTER_MS)
    #error "WDG_PROFILE_CUSTOM: phai dinh nghia du 4 macro WDG_TIMEOUT_TARGET_MS, WDG_TASK_PERIOD_MS, WDG_HEALTHY_CYCLE_MAX_MS, WDG_WARN_AFTER_MS (kieu UL)"
  #endif
#else
  #error "WDG_PROFILE khong hop le (dung WDG_PROFILE_FAST_0_5S / STD_1S / SLOW_2S / LONG_5S / CUSTOM)"
#endif

#define WDG_LSI_NOMINAL_HZ        40000UL
#define WDG_LSI_MIN_HZ            30000UL
#define WDG_LSI_MAX_HZ            60000UL

#define WDG_BOOT_TIMEOUT_MS       8000UL

#define WDG_MEASURE_LSI           1

#define WDG_AUTO_TRIM_TO_LSI      1

#define WDG_LSI_MEAS_TIMEOUT_MS   3000UL

#define WDG_FREEZE_IN_DEBUG       1

#define WDG_KICK_MARKER_ENABLE    1
#define WDG_MARKER_PORT           GPIOB
#define WDG_MARKER_PIN            GPIO_PIN_0
#define WDG_MARKER_CLK_ENABLE()   __HAL_RCC_GPIOB_CLK_ENABLE()

#define WDG_TICKS_AT(div) \
    ((WDG_TIMEOUT_TARGET_MS * WDG_LSI_NOMINAL_HZ) / ((div) * 1000UL))

#if   WDG_TICKS_AT(4UL)   <= 4096UL
  #define WDG_PRESCALER_DIV   4UL
  #define WDG_PR_BITS         0U
#elif WDG_TICKS_AT(8UL)   <= 4096UL
  #define WDG_PRESCALER_DIV   8UL
  #define WDG_PR_BITS         1U
#elif WDG_TICKS_AT(16UL)  <= 4096UL
  #define WDG_PRESCALER_DIV   16UL
  #define WDG_PR_BITS         2U
#elif WDG_TICKS_AT(32UL)  <= 4096UL
  #define WDG_PRESCALER_DIV   32UL
  #define WDG_PR_BITS         3U
#elif WDG_TICKS_AT(64UL)  <= 4096UL
  #define WDG_PRESCALER_DIV   64UL
  #define WDG_PR_BITS         4U
#elif WDG_TICKS_AT(128UL) <= 4096UL
  #define WDG_PRESCALER_DIV   128UL
  #define WDG_PR_BITS         5U
#elif WDG_TICKS_AT(256UL) <= 4096UL
  #define WDG_PRESCALER_DIV   256UL
  #define WDG_PR_BITS         6U
#else
  #error "WDG_TIMEOUT_TARGET_MS qua lon: IWDG toi da ~26 s o LSI 40 kHz"
#endif

#define WDG_RELOAD_VALUE        (WDG_TICKS_AT(WDG_PRESCALER_DIV) - 1UL)

#define WDG_TIMEOUT_MS_AT(lsi_hz) \
    (((WDG_RELOAD_VALUE + 1UL) * WDG_PRESCALER_DIV * 1000UL) / (lsi_hz))

#define WDG_TIMEOUT_NOM_MS      WDG_TIMEOUT_MS_AT(WDG_LSI_NOMINAL_HZ)
#define WDG_TIMEOUT_MIN_MS      WDG_TIMEOUT_MS_AT(WDG_LSI_MAX_HZ)
#define WDG_TIMEOUT_MAX_MS      WDG_TIMEOUT_MS_AT(WDG_LSI_MIN_HZ)

#define WDG_STATIC_ASSERT(name, cond)  typedef char wdg_assert_##name[(cond) ? 1 : -1]

WDG_STATIC_ASSERT(reload_in_range,   WDG_RELOAD_VALUE <= 0x0FFFUL);
WDG_STATIC_ASSERT(reload_not_zero,   WDG_RELOAD_VALUE >= 1UL);
WDG_STATIC_ASSERT(nominal_accuracy,  WDG_TIMEOUT_NOM_MS * 100UL >= WDG_TIMEOUT_TARGET_MS * 99UL);
WDG_STATIC_ASSERT(margin_over_cycle, WDG_TIMEOUT_MIN_MS >= 2UL * WDG_HEALTHY_CYCLE_MAX_MS);
WDG_STATIC_ASSERT(warn_before_reset, WDG_WARN_AFTER_MS  <  WDG_TIMEOUT_MIN_MS);
WDG_STATIC_ASSERT(task_in_cycle,     WDG_TASK_PERIOD_MS <  WDG_HEALTHY_CYCLE_MAX_MS);
WDG_STATIC_ASSERT(warn_after_cycle,  WDG_WARN_AFTER_MS  >  WDG_HEALTHY_CYCLE_MAX_MS);

WDG_STATIC_ASSERT(boot_longer,       WDG_BOOT_TIMEOUT_MS >= WDG_TIMEOUT_TARGET_MS);
WDG_STATIC_ASSERT(boot_representable,
                  ((WDG_BOOT_TIMEOUT_MS * WDG_LSI_NOMINAL_HZ) / (256UL * 1000UL)) <= 4096UL);

WDG_STATIC_ASSERT(no_overflow_run,   WDG_TIMEOUT_TARGET_MS <= 35000UL);
WDG_STATIC_ASSERT(no_overflow_boot,  WDG_BOOT_TIMEOUT_MS   <= 35000UL);

typedef enum
{
  WDG_TASK_HEARTBEAT = 0,
  WDG_TASK_UART,
  WDG_TASK_SCHED,
  WDG_TASK_COUNT
} WDG_TaskId;

#define WDG_ALL_TASKS_MASK  ((1UL << WDG_TASK_COUNT) - 1UL)

typedef enum
{
  WDG_PHASE_OFF = 0,
  WDG_PHASE_BOOT,
  WDG_PHASE_RUN
} WDG_Phase;

HAL_StatusTypeDef WDG_BootStart(void);

void WDG_BootKick(void);

uint8_t WDG_MeasureLsi(void);

HAL_StatusTypeDef WDG_Init(void);

void WDG_Checkin(WDG_TaskId id);

uint8_t WDG_Supervise(void);

uint32_t WDG_GetMissingMask(void);

uint32_t WDG_GetRefreshCount(void);
uint32_t WDG_GetLastRefreshTick(void);

WDG_Phase WDG_GetPhase(void);
uint32_t  WDG_GetLsiHz(void);
uint8_t   WDG_IsLsiMeasured(void);
uint32_t  WDG_GetPrescalerDiv(void);
uint32_t  WDG_GetReload(void);
uint32_t  WDG_GetTimeoutMs(void);
uint32_t  WDG_GetTimeoutMsAt(uint32_t lsi_hz);

void WDG_OnUnhealthy(uint32_t missing_mask, uint32_t ms_since_refresh);

#ifdef __cplusplus
}
#endif

#endif
