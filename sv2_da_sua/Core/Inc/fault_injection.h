#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#include "main.h"

typedef enum {
    FAULT_NONE = 0,
    FAULT_INFINITE_LOOP,
    FAULT_SENSOR_HANG,
    FAULT_HARDFAULT,
    FAULT_SOFT_RESET
} Fault_Type_t;

/* Call once before enabling the 10 ms timer interrupt. */
void Fault_Init(void);
/* ISR: sample buttons only. Never refresh IWDG or execute a fault here. */
void Fault_Scan_10ms(void);
/* Main context only. A selected fault intentionally does not return. */
void Fault_Process(void);
/* Read-only status for the supervisor/debugger. The fault stays latched. */
Fault_Type_t Fault_GetActive(void);
uint32_t Fault_GetScanCount(void);

#endif
