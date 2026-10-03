/**
 * @file    system_status.h
 * @brief   Module trang thai he thong - SV4 phu trach.
 *          - LED heartbeat (PC13, non-blocking, nhieu che do nhay)
 *          - UART log co timestamp (USART1 @115200)
 *          - Lenh dieu khien qua UART (phuc vu demo)
 *          - WWDG: refresh dung cua so + kich ban demo de so sanh voi IWDG
 *
 * @note    Tat ca ham trong module la NON-BLOCKING (khong dung HAL_Delay)
 *          de main loop luon kip feed watchdog.
 */
#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include "main.h"
#include "shared_types.h"

/* ===== Che do nhay LED heartbeat ===== */
typedef enum {
    HB_NORMAL = 0,   /* 1 Hz (500 ms sang / 500 ms tat) - he thong khoe      */
    HB_RECOVERED,    /* 10 Hz trong HB_RECOVERED_MS sau reset do watchdog     */
    HB_WARNING,      /* 2 lan chop ngan moi giay - canh bao                  */
    HB_FAULT,        /* sang lien tuc - dang loi                              */
    HB_OFF
} HeartbeatMode_t;

/* ===== Lenh nhan tu UART (main/integration quyet dinh xu ly) ===== */
typedef enum {
    CMD_NONE = 0,
    CMD_FAULT_LOOP,      /* '1' : yeu cau SV2 gay treo vong lap   */
    CMD_FAULT_HARD,      /* '2' : yeu cau SV2 gay HardFault       */
    CMD_FAULT_SENSOR,    /* '3' : yeu cau SV2 gia lap loi cam bien */
    CMD_SOFT_RESET,      /* 'r' : NVIC_SystemReset()               */
    CMD_WWDG_EARLY,      /* 'e' : refresh WWDG qua som -> reset    */
    CMD_WWDG_LATE,       /* 'l' : ngung refresh WWDG -> reset      */
    CMD_STATUS,          /* 's' : in trang thai                    */
    CMD_HELP             /* 'h' : in danh sach lenh                */
} StatusCmd_t;

#define HB_RECOVERED_MS   5000U   /* thoi gian nhay nhanh sau khi phuc hoi */

/* ===== Khoi tao ===== */
void Status_Init(UART_HandleTypeDef *huart);

/* Goi LIEN TUC trong main loop (moi vong lap, chu ky < 10 ms).
 * Lam: heartbeat + service WWDG + doc lenh UART.
 * Tra ve lenh nhan duoc (CMD_NONE neu khong co). */
StatusCmd_t Status_Process(void);

/* ===== Heartbeat ===== */
void            Status_SetHeartbeat(HeartbeatMode_t mode);
HeartbeatMode_t Status_GetHeartbeat(void);
void            Status_HeartbeatToggle(void);   /* toggle thu cong (debug) */

/* ===== UART log ===== */
void Status_LogMessage(const char *msg);
void Status_Printf(const char *fmt, ...);
void Status_LogReset(ResetReason_t reason, uint32_t count);
void Status_LogHealth(HealthStatus_t health);
void Status_LogFault(FaultType_t fault);
void Status_PrintHelp(void);
void Status_PrintStatus(void);

const char *Status_ResetReasonStr(ResetReason_t reason);
const char *Status_HealthStr(HealthStatus_t health);
const char *Status_FaultStr(FaultType_t fault);

/* ===== WWDG ===== */
/* Goi SAU MX_WWDG_Init(): module se tu refresh WWDG dung cua so */
void    Status_WWDG_Attach(WWDG_HandleTypeDef *hwwdg);
uint8_t Status_WWDG_IsActive(void);
void    Status_WWDG_Service(void);      /* da duoc goi trong Status_Process */
void    Status_WWDG_TestEarly(void);    /* refresh khi counter > window -> reset */
void    Status_WWDG_TestLate(void);     /* ngung refresh -> counter < 0x40 -> reset */

#endif /* SYSTEM_STATUS_H */
