/**
 * @file    temp_sensor.c
 * @brief   Driver DHT11 tren PA8 - SV4 phu trach.
 *
 *  Giao thuc 1 day:
 *   MCU keo LOW >= 18 ms -> nha ra (len HIGH)
 *   DHT11 tra loi: LOW 80 us -> HIGH 80 us
 *   40 bit: moi bit = LOW 50 us + HIGH 26-28 us (bit 0) hoac 70 us (bit 1)
 *   Byte: [Hum_nguyen][Hum_thapphan][Temp_nguyen][Temp_thapphan][Checksum]
 */
#include "temp_sensor.h"

#define DHT11_START_LOW_MS    20U   /* >= 18 ms */
#define BIT_ONE_THRESHOLD_US  40U   /* HIGH > 40 us => bit 1 */

typedef enum { ST_IDLE = 0, ST_START_LOW } TempState_t;

static TempData_t  s_data;
static TempState_t s_state      = ST_IDLE;
static uint32_t    s_t_start    = 0;
static uint32_t    s_last_read  = 0;
static uint32_t    s_cyc_per_us = 72;

/* ------------------------------------------------------------------------- */
/* Dinh thoi micro-giay bang DWT                                              */
/* ------------------------------------------------------------------------- */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    s_cyc_per_us = SystemCoreClock / 1000000U;
}

static inline uint32_t us_since(uint32_t c0)
{
    return (DWT->CYCCNT - c0) / s_cyc_per_us;
}

static inline void pin_write(GPIO_PinState st)
{
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, st);
}

static inline GPIO_PinState pin_read(void)
{
    return HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN);
}

/* Cho trong khi chan con o muc 'level'. Tra ve so us da cho, hoac -1 neu timeout. */
static int32_t wait_while(GPIO_PinState level, uint32_t timeout_us)
{
    uint32_t c0 = DWT->CYCCNT;
    while (pin_read() == level) {
        if (us_since(c0) > timeout_us) return -1;
    }
    return (int32_t)us_since(c0);
}

/* ------------------------------------------------------------------------- */
/* Doc 40 bit (blocking ~4-5 ms)                                              */
/* ------------------------------------------------------------------------- */
static TempStatus_t read_frame(uint8_t b[5])
{
    pin_write(GPIO_PIN_SET);                         /* nha bus */

    if (wait_while(GPIO_PIN_SET,   100) < 0) return TEMP_ERR_NO_RESPONSE;  /* doi DHT keo LOW */
    if (wait_while(GPIO_PIN_RESET, 100) < 0) return TEMP_ERR_NO_RESPONSE;  /* LOW 80 us  */
    if (wait_while(GPIO_PIN_SET,   100) < 0) return TEMP_ERR_NO_RESPONSE;  /* HIGH 80 us */

    for (uint8_t i = 0; i < 40; i++) {
        if (wait_while(GPIO_PIN_RESET, 80) < 0) return TEMP_ERR_TIMEOUT;   /* LOW 50 us */
        int32_t high = wait_while(GPIO_PIN_SET, 100);                      /* HIGH 26/70 us */
        if (high < 0) return TEMP_ERR_TIMEOUT;

        b[i / 8] <<= 1;
        if ((uint32_t)high > BIT_ONE_THRESHOLD_US) b[i / 8] |= 1U;
    }

    if ((uint8_t)(b[0] + b[1] + b[2] + b[3]) != b[4]) return TEMP_ERR_CHECKSUM;
    return TEMP_OK;
}

static void decode(const uint8_t b[5])
{
    s_data.hum_x10  = (uint16_t)(b[0] * 10U + (b[1] % 10U));
    s_data.temp_x10 = (int16_t)((b[2] & 0x7FU) * 10U + (b[3] & 0x0FU) % 10U);
    if (b[3] & 0x80U) s_data.temp_x10 = -s_data.temp_x10;   /* DHT11 doi moi co bit am */
}

/* ------------------------------------------------------------------------- */
/* API                                                                        */
/* ------------------------------------------------------------------------- */
void Temp_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    pin_write(GPIO_PIN_SET);
    g.Pin   = DHT11_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;      /* open-drain: vua ghi vua doc duoc IDR */
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &g);

    dwt_init();

    s_state     = ST_IDLE;
    s_last_read = HAL_GetTick() - TEMP_READ_PERIOD_MS + 1500U; /* DHT11 can ~1 s on dinh sau cap nguon */
}

uint8_t Temp_Process(void)
{
    uint32_t now = HAL_GetTick();

    switch (s_state) {
        case ST_IDLE:
            if (now - s_last_read >= TEMP_READ_PERIOD_MS) {
                pin_write(GPIO_PIN_RESET);           /* bat dau xung start */
                s_t_start = now;
                s_state   = ST_START_LOW;
            }
            return 0;

        case ST_START_LOW:
            if (now - s_t_start < DHT11_START_LOW_MS + 1U) return 0;   /* chua du 20 ms */
            {
                uint8_t b[5] = {0};
                TempStatus_t st = read_frame(b);
                pin_write(GPIO_PIN_SET);

                s_data.status = st;
                s_data.tick   = now;
                if (st == TEMP_OK) {
                    decode(b);
                    s_data.valid = 1;
                    s_data.ok_count++;
                } else {
                    s_data.err_count++;
                }
                s_last_read = now;
                s_state     = ST_IDLE;
            }
            return 1;

        default:
            s_state = ST_IDLE;
            return 0;
    }
}

const TempData_t *Temp_GetData(void)
{
    return &s_data;
}

const char *Temp_StatusStr(TempStatus_t s)
{
    switch (s) {
        case TEMP_OK:              return "OK";
        case TEMP_ERR_NO_RESPONSE: return "KHONG PHAN HOI";
        case TEMP_ERR_TIMEOUT:     return "TIMEOUT";
        case TEMP_ERR_CHECKSUM:    return "SAI CHECKSUM";
        default:                   return "?";
    }
}
