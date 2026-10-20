#include "doc_loi.h"

#define BKP_FAULT    (BKP->DR1)
#define BKP_COUNT    (BKP->DR2)
#define BKP_PC_LO    (BKP->DR3)
#define BKP_PC_HI    (BKP->DR4)
#define BKP_LR_LO    (BKP->DR5)
#define BKP_LR_HI    (BKP->DR6)
#define BKP_HF_MAGIC (BKP->DR7)

void doc_loi_Init(ResetInfor *infor)
{
    __HAL_RCC_BKP_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    uint32_t cause = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    if (cause & RCC_CSR_PORRSTF) {
        infor->cause = "POWER";
        BKP_FAULT = FAULT_NONE;
        BKP_COUNT = 0;
        BKP_HF_MAGIC = 0;
    } else if (cause & RCC_CSR_SFTRSTF) {
        infor->cause = "SOFT";
        BKP_COUNT++;
    } else if (cause & RCC_CSR_IWDGRSTF) {
        infor->cause = "IWDG";
        BKP_COUNT++;
    } else if (cause & RCC_CSR_WWDGRSTF) {
        infor->cause = "WWDG";
        BKP_COUNT++;
    } else {
        infor->cause = "NRST";
        BKP_COUNT++;
    }

    infor->count = BKP_COUNT;
    if (BKP_FAULT != 0) {
        infor->fault = (FaultCode)BKP_FAULT;
    } else {
        infor->fault = FAULT_NONE;
    }

    /* Doc thong tin PC va LR neu lan reset truoc do bi HardFault */
    if (BKP_HF_MAGIC == 0xA55AU) {
        infor->fault_pc = ((uint32_t)BKP_PC_HI << 16) | (uint32_t)BKP_PC_LO;
        infor->fault_lr = ((uint32_t)BKP_LR_HI << 16) | (uint32_t)BKP_LR_LO;
        infor->has_hardfault_info = 1;
        BKP_HF_MAGIC = 0; /* Xoa co sau khi da doc thanh cong */
    } else {
        infor->fault_pc = 0;
        infor->fault_lr = 0;
        infor->has_hardfault_info = 0;
    }

    BKP_FAULT = FAULT_NONE;
}

void ghi_lai_loi(FaultCode f)
{
    BKP_FAULT = f;
}

static const char *doc_ten_loi(int i)
{
    if (i == FAULT_NONE) {
        return "FAULT NONE";
    } else if (i == FAULT_LOOP) {
        return "FAULT LOOP";
    } else if (i == FAULT_SENSOR) {
        return "FAULT SENSOR";
    } else if (i == FAULT_HARDFAULT) {
        return "FAULT HARDFAULT";
    }
    return "FAULT NONE";
}

void hien_thi_loi(ResetInfor *infor)
{
    char msg[20];

    snprintf(msg, sizeof(msg), "Reset: %s %u", infor->cause, (unsigned)infor->count);
    lcd_set_cursor(1, 1);
    lcd_print_string(msg);

    memset(msg, 0, sizeof(msg));
    if (infor->fault == FAULT_HARDFAULT && infor->has_hardfault_info) {
        /* Hien thi dia chi PC gay ra HardFault ngay tren LCD */
        snprintf(msg, sizeof(msg), "PC:0x%08lX", (unsigned long)infor->fault_pc);
    } else {
        snprintf(msg, sizeof(msg), "%s", doc_ten_loi(infor->fault));
    }
    lcd_set_cursor(2, 1);
    lcd_print_string(msg);
}
