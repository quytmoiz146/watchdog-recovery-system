/**
 * @file    system_status.h
 * @brief   Ung dung nen - SV4 phu trach.
 *          - Do nhiet do/do am (DHT11/DHT22) dinh ky, log qua UART
 *          - LED heartbeat (PC13, non-blocking, nhieu che do nhay)
 *          - UART log co timestamp (USART1 @115200)
 *          - WWDG: refresh dung cua so (phuc vu so sanh voi IWDG)
 *
 * @note    Tat ca ham la NON-BLOCKING (khong dung HAL_Delay)
 *          de main loop luon kip feed watchdog.
 */
#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include "main.h"
#include "shared_types.h"
#include "temp_sensor.h"

/* ===== Che do nhay LED heartbeat ===== */
typedef enum {
    HB_NORMAL = 0,   /* 1 Hz - he thong khoe                              */
    HB_RECOVERED,    /* 10 Hz trong HB_RECOVERED_MS sau reset do watchdog */
    HB_WARNING,      /* chop kep moi giay - canh bao (vd: loi cam bien)   */
    HB_FAULT,        /* sang lien tuc - dang loi                          */
    HB_OFF
} HeartbeatMode_t;

#define HB_RECOVERED_MS        5000U  /* thoi gian nhay nhanh sau khi phuc hoi       */
#define TEMP_FAIL_WARN_COUNT   3U     /* so lan doc loi lien tiep -> WARNING         */
#define TEMP_ALARM_X10         500    /* nguong canh bao nhiet do: 50.0 do C         */

/* ===== Khoi tao & vong lap ===== */
void Status_Init(UART_HandleTypeDef *huart);
/* Goi LIEN TUC trong main loop (chu ky < 10 ms):
 * service WWDG + heartbeat + do nhiet dinh ky */
void Status_Process(void);

/* ===== Heartbeat ===== */
void            Status_SetHeartbeat(HeartbeatMode_t mode);
HeartbeatMode_t Status_GetHeartbeat(void);

/* ===== UART log ===== */
void Status_LogMessage(const char *msg);
void Status_Printf(const char *fmt, ...);
void Status_LogReset(ResetReason_t reason, uint32_t count);
void Status_LogHealth(HealthStatus_t health);
void Status_LogFault(FaultType_t fault);
void Status_PrintStatus(void);

const char *Status_ResetReasonStr(ResetReason_t reason);
const char *Status_HealthStr(HealthStatus_t health);
const char *Status_FaultStr(FaultType_t fault);

/* ===== WWDG ===== */
void    Status_WWDG_Attach(WWDG_HandleTypeDef *hwwdg);  /* goi SAU MX_WWDG_Init() */
uint8_t Status_WWDG_IsActive(void);
void    Status_WWDG_Service(void);                       /* da goi trong Status_Process */

#endif /* SYSTEM_STATUS_H */
