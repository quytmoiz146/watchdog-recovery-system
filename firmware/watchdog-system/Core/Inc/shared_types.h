/**
 * @file    shared_types.h
 * @brief   Kieu du lieu dung chung giua tat ca module (SV1..SV4).
 * @note    FILE NAY LA "GIAO KEO" CHUNG - khong ai tu y sua mot minh.
 *          Muon them/sua phai thong nhat ca nhom.
 */
#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>

/* ===== Nguyen nhan reset (SV3 implement, SV4 log, SV3 hien thi LCD) ===== */
typedef enum {
    RESET_POWER_ON  = 0x00,   /* Cap nguon (POR/PDR)            */
    RESET_IWDG      = 0x01,   /* IWDG timeout                   */
    RESET_WWDG      = 0x02,   /* WWDG timeout / refresh sai cua so */
    RESET_SOFTWARE  = 0x03,   /* NVIC_SystemReset()             */
    RESET_PIN       = 0x04,   /* Nhan nut NRST                  */
    RESET_LOW_POWER = 0x05,   /* Low-power reset                */
    RESET_UNKNOWN   = 0xFF
} ResetReason_t;

/* ===== Trang thai suc khoe he thong (SV1 implement, SV4 hien thi) ===== */
typedef enum {
    HEALTH_OK       = 0x00,
    HEALTH_WARNING  = 0x01,
    HEALTH_CRITICAL = 0x02
} HealthStatus_t;

/* Loai loi mo phong: dung FaultCode trong doc_loi.h (SV3) */

#endif /* SHARED_TYPES_H */
