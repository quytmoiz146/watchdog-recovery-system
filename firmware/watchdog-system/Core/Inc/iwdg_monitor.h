/**
 * @file    iwdg_monitor.h
 * @brief   SV1 - giam sat va nap lai IWDG.
 *          IWDG chi duoc nap khi moi tac vu da Register deu Checkin
 *          trong chu ky hien tai. Chua ai Register -> Feed nap moi lan goi.
 */
#ifndef IWDG_MONITOR_H
#define IWDG_MONITOR_H

#include "main.h"
#include "shared_types.h"

#define IWDG_MONITOR_MAX_TASKS   32U
#define IWDG_LSI_MIN_HZ          30000U
#define IWDG_LSI_MAX_HZ          60000U

#ifndef IWDG_MONITOR_FREEZE_ON_DEBUG
#define IWDG_MONITOR_FREEZE_ON_DEBUG  0
#endif

void           IWDG_Monitor_Init(IWDG_HandleTypeDef *hiwdg);
void           IWDG_Monitor_Register(uint8_t task_id);
void           IWDG_Monitor_Checkin(uint8_t task_id);
void           IWDG_Monitor_Feed(void);
HealthStatus_t IWDG_Monitor_GetHealth(void);
uint32_t       IWDG_Monitor_GetMissingMask(void);
uint32_t       IWDG_Monitor_GetTimeoutMs(uint32_t lsi_hz);

#endif /* IWDG_MONITOR_H */
