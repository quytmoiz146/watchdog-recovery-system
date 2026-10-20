/**
 * @file    flash_log.h
 * @brief   Phan mo rong: Luu log loi vao bo nho Flash (Page 63)
 *          Luu giu lich su cac lan Reset va su co (bao gom ca PC/LR khi HardFault).
 */
#ifndef FLASH_LOG_H
#define FLASH_LOG_H

#include "main.h"
#include <stdint.h>

/* STM32F103C8T6: 64 KB Flash (0x08000000 - 0x0800FFFF), moi page 1 KB (1024 bytes).
 * Page 63 la trang cuoi cung: 0x0800FC00 - 0x0800FFFF.
 */
#define FLASH_LOG_PAGE_ADDR     0x0800FC00U
#define FLASH_LOG_PAGE_SIZE     1024U
#define FLASH_LOG_MAGIC         0xA55AU
#define FLASH_LOG_MAX_ENTRIES   32U

#pragma pack(push, 2)
typedef struct {
    uint16_t magic;          /* 0xA55A neu ban ghi hop le */
    uint16_t reset_count;    /* So lan reset tich luy tai thoi diem ghi */
    uint8_t  reset_reason;   /* Nguyen nhan reset (0=PWR, 1=IWDG, 2=WWDG, 3=SOFT, 4=PIN) */
    uint8_t  fault_code;     /* Loai loi: 0=NONE, 1=LOOP, 2=SENSOR, 3=HARDFAULT */
    uint32_t uptime_s;       /* Thoi gian hoat dong (giay) */
    uint32_t fault_pc;       /* Program Counter khi HardFault (hoac 0) */
    uint32_t fault_lr;       /* Link Register khi HardFault (hoac 0) */
    uint16_t checksum;       /* Checksum kiem tra toan ven ban ghi */
    uint16_t reserved;       /* Padding de kich thuoc la 24 bytes (12 Half-Words) */
} FlashLogEntry_t;
#pragma pack(pop)

void     FlashLog_Init(void);
uint16_t FlashLog_GetCount(void);
uint8_t  FlashLog_Write(uint16_t reset_count, uint8_t reset_reason, uint8_t fault_code, uint32_t uptime_s, uint32_t pc, uint32_t lr);
uint8_t  FlashLog_Read(uint16_t index, FlashLogEntry_t *out_entry);
void     FlashLog_PrintAll(void);
void     FlashLog_Clear(void);

#endif /* FLASH_LOG_H */
