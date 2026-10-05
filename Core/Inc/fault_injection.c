#include "fault_injection.h"

#define FAULT_DEBOUNCE_COUNT  3U   /* 3 lan quet x 10 ms = 30 ms */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    Fault_Type_t  fault;
    uint8_t       count;
} Fault_Button_t;

static volatile Fault_Type_t current_fault = FAULT_NONE;

static Fault_Button_t buttons[] = {
    { BTN_LOOP_GPIO_Port,      BTN_LOOP_Pin,      FAULT_INFINITE_LOOP, 0 },
    { BTN_SENSOR_GPIO_Port,    BTN_SENSOR_Pin,    FAULT_SENSOR_HANG,   0 },
    { BTN_HARDFAULT_GPIO_Port, BTN_HARDFAULT_Pin, FAULT_HARDFAULT,     0 },
    { BTN_RESET_GPIO_Port,     BTN_RESET_Pin,     FAULT_SOFT_RESET,    0 },
};

#define BUTTON_COUNT  (sizeof(buttons) / sizeof(buttons[0]))

void Fault_Init(void)
{
    current_fault = FAULT_NONE;
    for (uint32_t i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].count = 0;
    }
}

/* Chay trong ngat: chi debounce va dat co, khong lam viec nang */
void Fault_Scan_10ms(void)
{
    for (uint32_t i = 0; i < BUTTON_COUNT; i++) {
        Fault_Button_t *b = &buttons[i];

        if (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_RESET) {  /* nhan = muc thap */
            if (b->count < FAULT_DEBOUNCE_COUNT) {
                b->count++;
                if (b->count == FAULT_DEBOUNCE_COUNT) {
                    current_fault = b->fault;
                }
            }
        } else {
            b->count = 0;
        }
    }
}

void Fault_Process(void)
{
    Fault_Type_t fault = current_fault;   /* doc mot lan */

    switch (fault) {
        case FAULT_INFINITE_LOOP:
            while (1) {
                HAL_GPIO_TogglePin(LED_TEST_GPIO_Port, LED_TEST_Pin);
                HAL_Delay(50);
            }

        case FAULT_SENSOR_HANG: {
            volatile uint8_t sensor_ready = 0;
            while (sensor_ready == 0) {
                /* cho vo han */
            }
            break;
        }

        case FAULT_HARDFAULT: {
            void (* volatile bad_function)(void) = (void (*)(void))0xFFFFFFFFUL;
            bad_function();
            break;
        }

        case FAULT_SOFT_RESET:
            NVIC_SystemReset();
            break;

        default:
            break;
    }
}