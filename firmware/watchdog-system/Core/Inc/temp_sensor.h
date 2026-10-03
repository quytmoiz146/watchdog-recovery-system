/**
 * @file    temp_sensor.h
 * @brief   Driver DHT11 / DHT22 (doc nhiet do + do am) - SV4 phu trach.
 *
 * @note    - Chan DATA mac dinh: PA8 (open-drain, can dien tro keo len 4.7k-10k;
 *            module DHT 3 chan thuong da co san dien tro nay).
 *          - Dinh thoi micro-giay dung bo dem chu ky DWT->CYCCNT (khong ton Timer).
 *          - NON-BLOCKING: xung start 18 ms duoc chia qua nhieu vong lap;
 *            chi phan doc 40 bit (~4-5 ms) la blocking.
 */
#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "main.h"

/* ===== Chon loai cam bien ===== */
#define DHT_TYPE_DHT11   11
#define DHT_TYPE_DHT22   22
#ifndef DHT_TYPE
#define DHT_TYPE         DHT_TYPE_DHT11      /* <-- doi thanh DHT_TYPE_DHT22 neu dung DHT22 */
#endif

/* Neu da dat label "DHT_DATA" trong CubeMX thi main.h se co macro nay */
#ifndef DHT_DATA_Pin
#define DHT_DATA_Pin        GPIO_PIN_8
#define DHT_DATA_GPIO_Port  GPIOA
#endif

/* Chu ky doc (DHT11 >= 1 s, DHT22 >= 2 s) */
#define TEMP_READ_PERIOD_MS   2000U

typedef enum {
    TEMP_OK = 0,
    TEMP_ERR_NO_RESPONSE,   /* cam bien khong tra loi (chua cam / dut day)  */
    TEMP_ERR_TIMEOUT,       /* mat dong bo khi dang doc bit                 */
    TEMP_ERR_CHECKSUM       /* du lieu sai checksum                         */
} TempStatus_t;

typedef struct {
    int16_t      temp_x10;    /* nhiet do x10 (285 = 28.5 do C)  */
    uint16_t     hum_x10;     /* do am x10   (650 = 65.0 %)      */
    TempStatus_t status;      /* ket qua lan doc gan nhat        */
    uint8_t      valid;       /* 1 = da tung doc thanh cong      */
    uint32_t     tick;        /* HAL_GetTick() luc doc           */
    uint32_t     ok_count;
    uint32_t     err_count;
} TempData_t;

void              Temp_Init(void);
/* Goi lien tuc trong main loop. Tra ve 1 khi vua co ket qua doc moi. */
uint8_t           Temp_Process(void);
const TempData_t *Temp_GetData(void);
const char       *Temp_StatusStr(TempStatus_t s);

#endif /* TEMP_SENSOR_H */
