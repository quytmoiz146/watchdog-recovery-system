#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#include "main.h"

typedef enum {
    FAULT_NONE = 0,
    FAULT_INFINITE_LOOP,    /* LED nhap nhay nhanh, ket trong vong lap */
    FAULT_SENSOR_HANG,      /* Cho sensor vo han, LED dung yen */
    FAULT_HARDFAULT,        /* Nhay toi dia chi khong hop le -> HardFault */
    FAULT_SOFT_RESET        /* Reset mem bang NVIC_SystemReset() */
} Fault_Type_t;

void Fault_Init(void);
void Fault_Scan_10ms(void);   /* Goi trong ngat TIM2 moi 10 ms */
void Fault_Process(void);     /* Goi trong vong lap chinh */

#endif /* FAULT_INJECTION_H */