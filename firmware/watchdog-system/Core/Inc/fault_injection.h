#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FAULT_NONE = 0,
    FAULT_INFINITE_LOOP,
    FAULT_SENSOR_HANG,
    FAULT_HARDFAULT
} Fault_Type_t;

/* SV2 only. Call once after GPIO and HAL tick have been initialized. */
void Fault_Init(void);

/* Call frequently from the main loop (suggested interval <= 10 ms).
   Normal operation is non-blocking. A triggered fault does not return.
   Do not call from an ISR. This module never refreshes either watchdog. */
void Fault_Process(void);

/* Selected fault, latched until reset or Fault_Init(). */
Fault_Type_t Fault_GetActive(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_INJECTION_H */
