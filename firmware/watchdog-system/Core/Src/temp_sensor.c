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
    *(volatile uint32_t *)0xE0001FB0U = 0xC5ACCE55U;  /* DWT->LAR: mo khoa (chip clone can) */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    SystemCoreClockUpdate();
    s_cyc_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
    if (s_cyc_per_us == 0U) s_cyc_per_us = 72U;
}

static inline uint32_t us_since(uint32_t c0)
{
    return (uint32_t)(DWT->CYCCNT - c0) / s_cyc_per_us;
}

static inline void pin_write(GPIO_PinState st)
{
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, st);
}

static inline GPIO_PinState pin_read(void)
{
    return HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN);
}

static inline void pin_mode_input_pullup(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = DHT11_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &g);
}

static inline void pin_mode_output_od(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = DHT11_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &g);
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
static TempStatus_t read_frame_raw(uint8_t b[5])
{
    /* 1. Tha bus: chuyen sang Input Pull-Up de dien tro keo len HIGH */
    pin_mode_input_pullup();

    /* 2. Cho bus duoc keo len HIGH sau xung start (RC rise time ~1-5 us, toi da 30 us) */
    if (wait_while(GPIO_PIN_RESET, 30) < 0) return TEMP_ERR_NO_RESPONSE;

    /* 3. Cho DHT11 keo bus xuong LOW de bat dau phan hoi (sau 20 - 40 us) */
    if (wait_while(GPIO_PIN_SET, 60) < 0) return TEMP_ERR_NO_RESPONSE;

    /* 4. Cho xung phan hoi LOW 80 us cua DHT11 ket thuc (DHT11 keo len HIGH) */
    if (wait_while(GPIO_PIN_RESET, 100) < 0) return TEMP_ERR_NO_RESPONSE;

    /* 5. Cho xung chuan bi HIGH 80 us cua DHT11 ket thuc (DHT11 keo xuong LOW de truyen bit 0) */
    if (wait_while(GPIO_PIN_SET, 100) < 0) return TEMP_ERR_NO_RESPONSE;

    /* 6. Doc 40 bit du lieu */
    for (uint8_t i = 0; i < 40; i++) {
        /* Moi bit bat dau bang xung LOW 50 us */
        if (wait_while(GPIO_PIN_RESET, 80) < 0) return TEMP_ERR_TIMEOUT;

        /* Do do rong xung HIGH: 26-28 us la bit 0, 70 us la bit 1 */
        uint32_t t_start = DWT->CYCCNT;
        int32_t high = wait_while(GPIO_PIN_SET, 100);
        if (high < 0) return TEMP_ERR_TIMEOUT;

        uint32_t width_us = us_since(t_start);

        b[i / 8] <<= 1;
        if (width_us > BIT_ONE_THRESHOLD_US) {
            b[i / 8] |= 1U;
        }
    }

    /* Tra chan ve che do Open-Drain o trang thai HIGH (idle) */
    pin_mode_output_od();
    pin_write(GPIO_PIN_SET);

    /* Khung toan 0 van "dung" checksum -> coi la loi (bus bi chap GND hoac sensor treo) */
    if ((b[0] | b[1] | b[2] | b[3]) == 0U) return TEMP_ERR_TIMEOUT;

    /* Kiem tra Checksum: Byte4 == (Byte0 + Byte1 + Byte2 + Byte3) & 0xFF */
    if ((uint8_t)(b[0] + b[1] + b[2] + b[3]) != b[4]) return TEMP_ERR_CHECKSUM;

    return TEMP_OK;
}

/* Chan ngat trong ~4-5 ms doc khung de SysTick/EXTI/I2C khong lam sai do rong xung */
static TempStatus_t read_frame(uint8_t b[5])
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    TempStatus_t st = read_frame_raw(b);
    __set_PRIMASK(primask);
    return st;
}

static void decode(const uint8_t b[5])
{
    s_data.raw[0] = b[0];
    s_data.raw[1] = b[1];
    s_data.raw[2] = b[2];
    s_data.raw[3] = b[3];
    s_data.raw[4] = b[4];

    /* Tu dong nhan dien chuan du lieu:
     * - Dinh dang DHT22 / AM2302 (16-bit moi dai luong, x10):
     *   b[0] la byte cao do am (voi do am 20-99%, b[0] = 0, 1, 2, 3).
     *   b[2] la byte cao nhiet do (voi nhiet do 10-50 C, b[2] = 0, 1).
     * - Dinh dang DHT11 truyen thong (8-bit nguyen + 8-bit thap phan):
     *   b[0] la do am nguyen (20 - 90%).
     *   b[2] la nhiet do nguyen (0 - 50 C).
     * Neu b[0] <= 4 va (b[2] & 0x7F) <= 2: day la cam bien tra ve dinh dang 16-bit!
     */
    if (b[0] <= 4U && (b[2] & 0x7FU) <= 2U) {
        /* Chuan 16-bit (DHT22 / AM2302) */
        s_data.hum_x10 = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);

        int16_t t = (int16_t)((((uint16_t)(b[2] & 0x7FU)) << 8) | b[3]);
        if (b[2] & 0x80U) {
            t = -t;
        }
        s_data.temp_x10 = t;
    } else {
        /* Chuan 8-bit (DHT11 truyen thong) */
        uint16_t hum_int = b[0];
        uint16_t hum_dec = (b[1] < 10U) ? b[1] : 0U;
        s_data.hum_x10 = (uint16_t)(hum_int * 10U + hum_dec);

        int16_t temp_int = (int16_t)(b[2] & 0x7FU);
        int16_t temp_dec = (int16_t)((b[3] & 0x7FU) < 10U ? (b[3] & 0x7FU) : 0U);
        s_data.temp_x10 = (int16_t)(temp_int * 10 + temp_dec);
        if ((b[2] & 0x80U) || (b[3] & 0x80U)) {
            s_data.temp_x10 = -s_data.temp_x10;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* API                                                                        */
/* ------------------------------------------------------------------------- */
void Temp_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    pin_mode_input_pullup();
    pin_write(GPIO_PIN_SET);

    dwt_init();

    s_state     = ST_IDLE;
    s_last_read = HAL_GetTick(); /* Cho on dinh sau cap nguon */
}

uint8_t Temp_Process(void)
{
    uint32_t now = HAL_GetTick();

    switch (s_state) {
        case ST_IDLE:
            if (now - s_last_read >= TEMP_READ_PERIOD_MS) {
                pin_mode_output_od();
                pin_write(GPIO_PIN_RESET);           /* bat dau xung start */
                s_t_start = now;
                s_state   = ST_START_LOW;
            }
            return 0;

        case ST_START_LOW:
            if (now - s_t_start < DHT11_START_LOW_MS) return 0;   /* cho du 20 ms */
            {
                uint8_t b[5] = {0};
                TempStatus_t st = read_frame(b);
                pin_mode_output_od();
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
