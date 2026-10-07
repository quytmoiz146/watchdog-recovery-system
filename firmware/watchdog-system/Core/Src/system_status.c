/**
 * @file    system_status.c
 * @brief   Ung dung nen - SV4 phu trach.
 *          Do nhiet (DHT) + LED heartbeat + UART log + WWDG service.
 */
#include "system_status.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Cau hinh                                                                  */
/* ------------------------------------------------------------------------- */
#define LOG_BUF_SIZE        160U
#define UART_CHUNK_SIZE     16U     /* ~1.4 ms @115200 -> service WWDG giua cac chunk */
#define UART_TX_TIMEOUT_MS  50U

/* LED tren Blue Pill (PC13) la tich cuc THAP */
#define LED_ON_LEVEL        GPIO_PIN_RESET
#define LED_OFF_LEVEL       GPIO_PIN_SET

/* ------------------------------------------------------------------------- */
/* Bien noi bo                                                               */
/* ------------------------------------------------------------------------- */
static UART_HandleTypeDef *s_huart = NULL;
static WWDG_HandleTypeDef *s_hwwdg = NULL;

static HeartbeatMode_t s_hb_mode   = HB_NORMAL;
static uint32_t        s_hb_start  = 0;
static int8_t          s_led_state = -1;     /* -1 = chua biet, 0 = tat, 1 = sang */

static uint8_t         s_temp_fail_streak = 0;
static uint8_t         s_temp_warning     = 0;

/* ------------------------------------------------------------------------- */
/* Ham noi bo                                                                */
/* ------------------------------------------------------------------------- */
static void led_write(uint8_t on)
{
    if (s_led_state == (int8_t)on) return;
    HAL_GPIO_WritePin(LED_HEARTBEAT_GPIO_Port, LED_HEARTBEAT_Pin,
                      on ? LED_ON_LEVEL : LED_OFF_LEVEL);
    s_led_state = (int8_t)on;
}

static void uart_send(const char *s, uint16_t len)
{
    if (s_huart == NULL) return;
    while (len > 0U) {
        uint16_t n = (len > UART_CHUNK_SIZE) ? UART_CHUNK_SIZE : len;
        HAL_UART_Transmit(s_huart, (uint8_t *)s, n, UART_TX_TIMEOUT_MS);
        s   += n;
        len -= n;
        Status_WWDG_Service();   /* log dai khong lam lo cua so WWDG */
    }
}

static void log_prefix(void)
{
    char p[20];
    uint32_t t = HAL_GetTick();
    int n = snprintf(p, sizeof(p), "[%5lu.%03lu] ",
                     (unsigned long)(t / 1000U), (unsigned long)(t % 1000U));
    if (n > 0) uart_send(p, (uint16_t)n);
}

static const char *hb_str(HeartbeatMode_t m)
{
    switch (m) {
        case HB_NORMAL:    return "NORMAL (1Hz)";
        case HB_RECOVERED: return "RECOVERED (10Hz)";
        case HB_WARNING:   return "WARNING (chop kep)";
        case HB_FAULT:     return "FAULT (sang lien tuc)";
        case HB_OFF:       return "OFF";
        default:           return "?";
    }
}

static void heartbeat_update(void)
{
    uint32_t el = HAL_GetTick() - s_hb_start;

    switch (s_hb_mode) {
        case HB_NORMAL:
            led_write((el % 1000U) < 500U);
            break;

        case HB_RECOVERED:
            if (el >= HB_RECOVERED_MS) {
                Status_SetHeartbeat(s_temp_warning ? HB_WARNING : HB_NORMAL);
                Status_LogMessage("Heartbeat -> on dinh");
                return;
            }
            led_write((el % 200U) < 100U);
            break;

        case HB_WARNING: {
            uint32_t ph = el % 1000U;
            led_write((ph < 100U) || (ph >= 200U && ph < 300U));
            break;
        }

        case HB_FAULT:
            led_write(1);
            break;

        case HB_OFF:
        default:
            led_write(0);
            break;
    }
}

/* Chuyen gia tri x10 thanh chuoi "-12.3" */
static void fmt_x10(char *out, size_t sz, int32_t v)
{
    const char *sign = (v < 0) ? "-" : "";
    if (v < 0) v = -v;
    snprintf(out, sz, "%s%ld.%ld", sign, (long)(v / 10), (long)(v % 10));
}

static void set_temp_warning(uint8_t on)
{
    if (on == s_temp_warning) return;
    s_temp_warning = on;
    if (s_hb_mode == HB_RECOVERED) return;        /* de LED nhay phuc hoi chay het */
    Status_LogHealth(on ? HEALTH_WARNING : HEALTH_OK);
}

static void temp_task(void)
{
    if (!Temp_Process()) return;                  /* chua co ket qua moi */

    const TempData_t *d = Temp_GetData();

    if (d->status == TEMP_OK) {
        char t[12], h[12];
        fmt_x10(t, sizeof(t), d->temp_x10);
        fmt_x10(h, sizeof(h), d->hum_x10);
        Status_Printf("[TEMP] Nhiet do: %s C | Do am: %s %%", t, h);

        s_temp_fail_streak = 0;
        if (d->temp_x10 >= TEMP_ALARM_X10) {
            Status_LogMessage("[TEMP] CANH BAO: nhiet do vuot nguong!");
            set_temp_warning(1);
        } else {
            set_temp_warning(0);
        }
    } else {
        if (s_temp_fail_streak < 255U) s_temp_fail_streak++;
        Status_Printf("[TEMP] Loi doc cam bien: %s (lien tiep %u lan)",
                      Temp_StatusStr(d->status), (unsigned)s_temp_fail_streak);
        if (s_temp_fail_streak >= TEMP_FAIL_WARN_COUNT) set_temp_warning(1);
    }
}

/* ------------------------------------------------------------------------- */
/* Khoi tao & vong lap                                                       */
/* ------------------------------------------------------------------------- */
void Status_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_led_state = -1;
    Status_SetHeartbeat(HB_NORMAL);
    Temp_Init();

    uart_send("\r\n", 2);
    Status_LogMessage("==================================================");
    Status_LogMessage("  WATCHDOG RECOVERY SYSTEM - STM32F103C8T6");
    Status_Printf    ("  Build: %s %s", __DATE__, __TIME__);
    Status_Printf    ("  Cam bien: DHT11 (PA8), chu ky doc %lu ms",
                      (unsigned long)TEMP_READ_PERIOD_MS);
    Status_LogMessage("==================================================");
}

void Status_Process(void)
{
    Status_WWDG_Service();
    heartbeat_update();
    temp_task();
}

/* ------------------------------------------------------------------------- */
/* Heartbeat                                                                 */
/* ------------------------------------------------------------------------- */
void Status_SetHeartbeat(HeartbeatMode_t mode)
{
    s_hb_mode  = mode;
    s_hb_start = HAL_GetTick();
}

HeartbeatMode_t Status_GetHeartbeat(void)
{
    return s_hb_mode;
}

/* ------------------------------------------------------------------------- */
/* UART log                                                                  */
/* ------------------------------------------------------------------------- */
void Status_LogMessage(const char *msg)
{
    log_prefix();
    uart_send(msg, (uint16_t)strlen(msg));
    uart_send("\r\n", 2);
}

void Status_Printf(const char *fmt, ...)
{
    char buf[LOG_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;

    log_prefix();
    uart_send(buf, (uint16_t)n);
    uart_send("\r\n", 2);
}

const char *Status_ResetReasonStr(ResetReason_t r)
{
    switch (r) {
        case RESET_POWER_ON:  return "POWER-ON";
        case RESET_IWDG:      return "IWDG";
        case RESET_WWDG:      return "WWDG";
        case RESET_SOFTWARE:  return "SOFTWARE";
        case RESET_PIN:       return "NRST PIN";
        case RESET_LOW_POWER: return "LOW-POWER";
        default:              return "UNKNOWN";
    }
}

const char *Status_HealthStr(HealthStatus_t h)
{
    switch (h) {
        case HEALTH_OK:       return "OK";
        case HEALTH_WARNING:  return "WARNING";
        case HEALTH_CRITICAL: return "CRITICAL";
        default:              return "?";
    }
}

void Status_LogReset(ResetReason_t reason, uint32_t count)
{
    Status_Printf("[RESET] Nguyen nhan: %s | So lan reset: %lu",
                  Status_ResetReasonStr(reason), (unsigned long)count);

    if (reason == RESET_IWDG || reason == RESET_WWDG) {
        Status_LogMessage("[RESET] He thong vua TU PHUC HOI sau loi -> LED nhay nhanh");
        Status_SetHeartbeat(HB_RECOVERED);
    }
}

void Status_LogHealth(HealthStatus_t health)
{
    Status_Printf("[HEALTH] %s", Status_HealthStr(health));
    switch (health) {
        case HEALTH_OK:       Status_SetHeartbeat(HB_NORMAL);  break;
        case HEALTH_WARNING:  Status_SetHeartbeat(HB_WARNING); break;
        case HEALTH_CRITICAL: Status_SetHeartbeat(HB_FAULT);   break;
        default: break;
    }
}

void Status_PrintStatus(void)
{
    uint32_t t = HAL_GetTick();
    const TempData_t *d = Temp_GetData();

    Status_Printf("[STATUS] Uptime: %lu.%03lu s | Heartbeat: %s",
                  (unsigned long)(t / 1000U), (unsigned long)(t % 1000U),
                  hb_str(s_hb_mode));
    Status_Printf("[STATUS] DHT: doc OK %lu lan, loi %lu lan",
                  (unsigned long)d->ok_count, (unsigned long)d->err_count);
    if (Status_WWDG_IsActive()) {
        Status_Printf("[STATUS] WWDG: ON | counter=0x%02lX window=0x%02lX",
                      (unsigned long)(WWDG->CR & WWDG_CR_T),
                      (unsigned long)(WWDG->CFR & WWDG_CFR_W));
    } else {
        Status_LogMessage("[STATUS] WWDG: OFF");
    }
}

/* ------------------------------------------------------------------------- */
/* WWDG                                                                      */
/*  PCLK1 = 36 MHz, Prescaler = 8  -> 1 tick = 4096*8/36MHz ~ 0.91 ms         */
/*  Counter = 127 (0x7F), Window = 80 (0x50), reset khi counter < 64 (0x40)  */
/*   - Refresh khi counter > 80 (som hon ~42.8 ms)     -> RESET              */
/*   - Khong refresh truoc khi counter < 64 (~58.3 ms) -> RESET              */
/* ------------------------------------------------------------------------- */
void Status_WWDG_Attach(WWDG_HandleTypeDef *hwwdg)
{
    s_hwwdg = hwwdg;
    Status_LogMessage("[WWDG] Da kich hoat - cua so refresh ~42.8..58.3 ms");
}

uint8_t Status_WWDG_IsActive(void)
{
    return (s_hwwdg != NULL) ? 1U : 0U;
}

void Status_WWDG_Service(void)
{
    if (s_hwwdg == NULL) return;

    /* Chi refresh khi counter da xuong toi cua so (counter <= window) */
    if ((WWDG->CR & WWDG_CR_T) <= (WWDG->CFR & WWDG_CFR_W)) {
        HAL_WWDG_Refresh(s_hwwdg);
    }
}
