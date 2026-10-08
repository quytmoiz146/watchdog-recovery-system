/* Sinh vien 2: button-triggered fault injection only.
   Uses the existing CubeMX pins BTN_FAULT1 (PA0), BTN_FAULT2 (PA1).
   Does not initialize GPIO/timers, refresh watchdogs, or control the LED. */
#include "fault_injection.h"
#include "main.h"

#define FAULT_DEBOUNCE_MS 30U
#define FAULT_BUTTON_1    1U
#define FAULT_BUTTON_2    2U
#define FAULT_BOTH        (FAULT_BUTTON_1 | FAULT_BUTTON_2)

static volatile Fault_Type_t current_fault = FAULT_NONE;
static uint32_t candidate_since;
static uint8_t candidate_mask;
static uint8_t gesture_mask;
static uint8_t armed;
static uint8_t initialized;

static uint8_t Fault_ReadButtons(void)
{
    uint8_t mask = 0U;

    /* Existing GPIO pull-ups: pressed = LOW. */
    if (HAL_GPIO_ReadPin(BTN_FAULT1_GPIO_Port, BTN_FAULT1_Pin) == GPIO_PIN_RESET) {
        mask |= FAULT_BUTTON_1;
    }
    if (HAL_GPIO_ReadPin(BTN_FAULT2_GPIO_Port, BTN_FAULT2_Pin) == GPIO_PIN_RESET) {
        mask |= FAULT_BUTTON_2;
    }
    return mask;
}

static void Fault_ScanButtons(void)
{
    uint8_t mask;
    uint32_t now;

    if ((initialized == 0U) || (current_fault != FAULT_NONE)) {
        return;
    }

    mask = Fault_ReadButtons();
    now = HAL_GetTick();
    if (mask != candidate_mask) {
        candidate_mask = mask;
        candidate_since = now;
        return;
    }
    /* Unsigned subtraction also works when the millisecond tick wraps. */
    if ((uint32_t)(now - candidate_since) < FAULT_DEBOUNCE_MS) {
        return;
    }

    if (armed == 0U) {
        /* Require BOTH buttons released after boot: a held button must not
           restart the same fault immediately after watchdog recovery. */
        if (mask == 0U) {
            armed = 1U;
        }
        return;
    }

    if (mask == FAULT_BOTH) {
        /* Combo wins even if one button was pressed earlier than the other. */
        current_fault = FAULT_HARDFAULT;
    } else if (mask != 0U) {
        if (gesture_mask == 0U) {
            gesture_mask = mask;
        }
    } else if (gesture_mask == FAULT_BUTTON_1) {
        /* Execute a single-button gesture only after stable release.
           This leaves time to press the second button for the combo. */
        current_fault = FAULT_INFINITE_LOOP;
    } else if (gesture_mask == FAULT_BUTTON_2) {
        current_fault = FAULT_SENSOR_HANG;
    } else {
        /* Both buttons released, no valid press yet. */
    }
}

void Fault_Init(void)
{
    current_fault = FAULT_NONE;
    gesture_mask = 0U;
    armed = 0U;
    candidate_mask = Fault_ReadButtons();
    candidate_since = HAL_GetTick();
    initialized = 1U;
}

Fault_Type_t Fault_GetActive(void)
{
    return current_fault;
}

void Fault_Process(void)
{
    Fault_ScanButtons();

    switch (current_fault) {
    case FAULT_INFINITE_LOOP:
        /* 1. Main task is stuck forever; interrupts can still run. */
        while (1) {
            __NOP();
        }

    case FAULT_SENSOR_HANG:
    {
        /* 2. Simulate a sensor-ready flag which never becomes true.
           No sensor driver or physical sensor is needed for this test. */
        volatile uint8_t sensor_ready = 0U;
        while (sensor_ready == 0U) {
            __NOP();
        }
        /* Keep the fault latched if a debugger changes sensor_ready. */
        while (1) {
            __NOP();
        }
    }

    case FAULT_HARDFAULT:
    {
        volatile uint32_t invalid_read;

        /* 3. Read a reserved STM32F103C8 address. Disable the configurable
           BusFault handler so this bus error escalates to HardFault.
           The project's existing HardFault_Handler handles the exception. */
        SCB->SHCSR &= ~SCB_SHCSR_BUSFAULTENA_Msk;
        __DSB();
        __ISB();
        invalid_read = *(volatile uint32_t *)0xFFFFFFF0UL;
        (void)invalid_read;
        __DSB();

        /* Some simulators do not implement faults for reserved addresses.
           Entering HardFault_Handler must be checked on the actual MCU. */
        while (1) {
            __NOP();
        }
    }

    case FAULT_NONE:
    default:
        break;
    }
}
