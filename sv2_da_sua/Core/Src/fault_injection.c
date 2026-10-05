#include "fault_injection.h"

/* Three consecutive samples at 10 ms; filter both press and release. */
#define FAULT_DEBOUNCE_COUNT 3U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    Fault_Type_t fault;
    uint8_t press_count;
    uint8_t release_count;
    uint8_t armed;
} Fault_Button_t;

static volatile Fault_Type_t current_fault = FAULT_NONE;
static volatile uint32_t scan_count = 0U;

/* Simultaneous presses: first entry wins; RESET cannot hide another test. */
static Fault_Button_t buttons[] = {
    { BTN_LOOP_GPIO_Port, BTN_LOOP_Pin, FAULT_INFINITE_LOOP, 0U, 0U, 0U },
    { BTN_SENSOR_GPIO_Port, BTN_SENSOR_Pin, FAULT_SENSOR_HANG, 0U, 0U, 0U },
    { BTN_HARDFAULT_GPIO_Port, BTN_HARDFAULT_Pin, FAULT_HARDFAULT, 0U, 0U, 0U },
    { BTN_RESET_GPIO_Port, BTN_RESET_Pin, FAULT_SOFT_RESET, 0U, 0U, 0U }
};

#define BUTTON_COUNT (sizeof(buttons) / sizeof(buttons[0]))

void Fault_Init(void)
{
    current_fault = FAULT_NONE;
    scan_count = 0U;
    for (uint32_t i = 0U; i < BUTTON_COUNT; ++i) {
        buttons[i].press_count = 0U;
        buttons[i].release_count = 0U;
        /* A button held across reset must first be released stably. */
        buttons[i].armed = 0U;
    }
}

void Fault_Scan_10ms(void)
{
    ++scan_count;
    if (current_fault != FAULT_NONE) {
        return;
    }
    for (uint32_t i = 0U; i < BUTTON_COUNT; ++i) {
        Fault_Button_t *button = &buttons[i];
        if (HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_SET) {
            button->press_count = 0U;
            if (button->release_count < FAULT_DEBOUNCE_COUNT) {
                ++button->release_count;
            }
            if (button->release_count == FAULT_DEBOUNCE_COUNT) {
                button->armed = 1U;
            }
        } else {
            button->release_count = 0U;
            if (button->armed != 0U) {
                if (button->press_count < FAULT_DEBOUNCE_COUNT) {
                    ++button->press_count;
                }
                if (button->press_count == FAULT_DEBOUNCE_COUNT) {
                    current_fault = button->fault;
                    return;
                }
            }
        }
    }
}

Fault_Type_t Fault_GetActive(void)
{
    return current_fault;
}

uint32_t Fault_GetScanCount(void)
{
    return scan_count;
}

void Fault_Process(void)
{
    switch (Fault_GetActive()) {
    case FAULT_INFINITE_LOOP:
        while (1) {
            /* Interrupts still run, but IWDG is never fed. */
            HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
            HAL_Delay(50U);
        }

    case FAULT_SENSOR_HANG: {
        volatile uint8_t sensor_ready = 0U;
        while (sensor_ready == 0U) {
            /* Simulated sensor never responds. Deliberately no timeout. */
            __NOP();
        }
        /* Do not resume feeding if a debugger changes sensor_ready. */
        while (1) { __NOP(); }
    }

    case FAULT_HARDFAULT: {
        /* Reserved STM32F103C8 address. A disabled BusFault escalates
           to HardFault. Keep this access volatile, even with optimization. */
        SCB->SHCSR &= ~SCB_SHCSR_BUSFAULTENA_Msk;
        __DSB();
        __ISB();
        volatile uint32_t invalid_read = *(volatile uint32_t *)0xFFFFFFF0UL;
        (void)invalid_read;
        __DSB();
        /* A simulator may not model this bus fault. No feeding even then.
           Entering HardFault_Handler is required to prove this test passed. */
        while (1) { __NOP(); }
    }

    case FAULT_SOFT_RESET:
        NVIC_SystemReset();
        while (1) { __NOP(); }

    case FAULT_NONE:
    default:
        break;
    }
}
