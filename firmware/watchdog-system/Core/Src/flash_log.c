/**
 * @file    flash_log.c
 * @brief   Phan mo rong: Luu log loi vao bo nho Flash (Page 63)
 */
#include "flash_log.h"
#include "system_status.h"
#include <string.h>

static uint16_t s_entry_count = 0;

static uint16_t calc_checksum(const FlashLogEntry_t *entry)
{
    const uint16_t *p = (const uint16_t *)entry;
    uint32_t sum = 0;
    /* Tinh tong tat ca cac Half-Word tru checksum va reserved */
    size_t words = (sizeof(FlashLogEntry_t) - 4U) / 2U;
    for (size_t i = 0; i < words; i++) {
        sum += p[i];
    }
    return (uint16_t)(sum & 0xFFFFU);
}

void FlashLog_Init(void)
{
    s_entry_count = 0;

    for (uint16_t i = 0; i < FLASH_LOG_MAX_ENTRIES; i++) {
        uint32_t addr = FLASH_LOG_PAGE_ADDR + (uint32_t)(i * sizeof(FlashLogEntry_t));
        const FlashLogEntry_t *entry = (const FlashLogEntry_t *)addr;

        if (entry->magic == FLASH_LOG_MAGIC) {
            if (calc_checksum(entry) == entry->checksum) {
                s_entry_count++;
            } else {
                /* Ban ghi bi loi checksum -> dung lai */
                break;
            }
        } else if (entry->magic == 0xFFFFU) {
            /* O nho trong chua ghi -> ket thuc danh sach */
            break;
        } else {
            /* Du lieu ranh gioi khong hop le */
            break;
        }
    }
}

uint16_t FlashLog_GetCount(void)
{
    return s_entry_count;
}

uint8_t FlashLog_Read(uint16_t index, FlashLogEntry_t *out_entry)
{
    if (index >= s_entry_count || out_entry == NULL) {
        return 0;
    }
    uint32_t addr = FLASH_LOG_PAGE_ADDR + (uint32_t)(index * sizeof(FlashLogEntry_t));
    memcpy(out_entry, (const void *)addr, sizeof(FlashLogEntry_t));
    return 1;
}

void FlashLog_Clear(void)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;

    HAL_FLASH_Unlock();

    erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_LOG_PAGE_ADDR;
    erase_init.NbPages     = 1;

    if (HAL_FLASHEx_Erase(&erase_init, &page_error) == HAL_OK) {
        s_entry_count = 0;
    }

    HAL_FLASH_Lock();
}

uint8_t FlashLog_Write(uint16_t reset_count, uint8_t reset_reason, uint8_t fault_code, uint32_t uptime_s, uint32_t pc, uint32_t lr)
{
    FlashLogEntry_t entry;

    /* Neu page da day 32 ban ghi -> xoa page de ghi lai tu dau */
    if (s_entry_count >= FLASH_LOG_MAX_ENTRIES) {
        FlashLog_Clear();
    }

    entry.magic        = FLASH_LOG_MAGIC;
    entry.reset_count  = reset_count;
    entry.reset_reason = reset_reason;
    entry.fault_code   = fault_code;
    entry.uptime_s     = uptime_s;
    entry.fault_pc     = pc;
    entry.fault_lr     = lr;
    entry.reserved     = 0;
    entry.checksum     = calc_checksum(&entry);

    uint32_t target_addr = FLASH_LOG_PAGE_ADDR + (uint32_t)(s_entry_count * sizeof(FlashLogEntry_t));
    const uint16_t *p_data = (const uint16_t *)&entry;
    size_t half_words = sizeof(FlashLogEntry_t) / 2U;

    HAL_FLASH_Unlock();

    for (size_t i = 0; i < half_words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, target_addr + (i * 2U), p_data[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return 0;
        }
    }

    HAL_FLASH_Lock();
    s_entry_count++;
    return 1;
}

static const char *reason_str(uint8_t r)
{
    switch (r) {
        case 0: return "POWER_ON";
        case 1: return "IWDG_TIMEOUT";
        case 2: return "WWDG_TIMEOUT";
        case 3: return "SOFTWARE";
        case 4: return "PIN_RESET";
        case 5: return "LOW_POWER";
        default: return "UNKNOWN";
    }
}

static const char *fault_str(uint8_t f)
{
    switch (f) {
        case 1: return "TREO VONG LAP";
        case 2: return "CHO CAM BIEN";
        case 3: return "HARDFAULT";
        default: return "KHONG CO";
    }
}

void FlashLog_PrintAll(void)
{
    Status_LogMessage("[FLASH] ================= LICH SU LOG TRONG FLASH (Page 63) =================");
    Status_Printf("[FLASH] Tong so ban ghi da luu: %u / %u", (unsigned)s_entry_count, (unsigned)FLASH_LOG_MAX_ENTRIES);

    if (s_entry_count == 0) {
        Status_LogMessage("[FLASH] (Chua co ban ghi su co nao trong Flash)");
    } else {
        for (uint16_t i = 0; i < s_entry_count; i++) {
            FlashLogEntry_t e;
            if (FlashLog_Read(i, &e)) {
                if (e.fault_code == 3 && e.fault_pc != 0) {
                    /* HardFault co thong tin PC / LR */
                    Status_Printf("[FLASH] #%02u: Lan %u | %s | %s | PC: 0x%08lX | LR: 0x%08lX | Uptime: %lu s",
                                  (unsigned)(i + 1), (unsigned)e.reset_count,
                                  reason_str(e.reset_reason), fault_str(e.fault_code),
                                  (unsigned long)e.fault_pc, (unsigned long)e.fault_lr,
                                  (unsigned long)e.uptime_s);
                } else {
                    Status_Printf("[FLASH] #%02u: Lan %u | %s | %s | Uptime: %lu s",
                                  (unsigned)(i + 1), (unsigned)e.reset_count,
                                  reason_str(e.reset_reason), fault_str(e.fault_code),
                                  (unsigned long)e.uptime_s);
                }
            }
        }
    }
    Status_LogMessage("[FLASH] ======================================================================");
}
