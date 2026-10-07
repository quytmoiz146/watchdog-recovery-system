/**
 * @file    temp_sensor.h
 * @brief   Driver DHT11 (doc nhiet do + do am) - SV4 phu trach.
 *
 * @note    - Chan DATA: PA8 (open-drain, module DHT11 3 chan da co dien tro keo len).
 *          - Dinh thoi micro-giay dung bo dem chu ky DWT->CYCCNT (khong ton Timer).
 *          - NON-BLOCKING: xung start 20 ms duoc chia qua nhieu vong lap;
 *            chi phan doc 40 bit (~4-5 ms) la blocking.
 */
#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "main.h"

#define DHT11_PIN             GPIO_PIN_8
#define DHT11_PORT            GPIOA
#define TEMP_READ_PERIOD_MS   2000U      /* DHT11 can >= 1 s giua 2 lan doc */

typedef enum {
    TEMP_OK = 0,
    TEMP_ERR_NO_RESPONSE,   /* cam bien khong tra loi (chua cam / dut day) */
    TEMP_ERR_TIMEOUT,       /* mat dong bo khi dang doc bit                */
    TEMP_ERR_CHECKSUM       /* du lieu sai checksum                        */
} TempStatus_t;

typedef struct {
    int16_t      temp_x10;    /* nhiet do x10 (285 = 28.5 do C) */
    uint16_t     hum_x10;     /* do am x10   (650 = 65.0 %)     */
    TempStatus_t status;      /* ket qua lan doc gan nhat       */
    uint8_t      valid;       /* 1 = da tung doc thanh cong     */
    uint32_t     tick;        /* HAL_GetTick() luc doc          */
    uint32_t     ok_count;
    uint32_t     err_count;
} TempData_t;

void              Temp_Init(void);
/* Goi lien tuc trong main loop. Tra ve 1 khi vua co ket qua doc moi. */
uint8_t           Temp_Process(void);
const TempData_t *Temp_GetData(void);
const char       *Temp_StatusStr(TempStatus_t s);

#endif /* TEMP_SENSOR_H */
