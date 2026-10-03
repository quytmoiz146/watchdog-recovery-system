/**
 * @file    system_status.c
 * @brief   Module trang thai he thong - SV4 phu trach.
 *          LED heartbeat + UART log + lenh UART + WWDG (so sanh voi IWDG).
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
static UART_HandleTypeDef *s_huart   = NULL;
static WWDG_HandleTypeDef *s_hwwdg   = NULL;
static volatile uint8_t    s_wwdg_stop = 0;   /* 1 = ngung refresh (test LATE) */

static HeartbeatMode_t s_hb_mode   = HB_NORMAL;
static uint32_t        s_hb_start  = 0;       /* tick luc vao che do hien tai */
static int8_t          s_led_state = -1;      /* -1 = chua biet, 0 = tat, 1 = sang */

/* ------------------------------------------------------------------------- */
/* Ham noi bo                                                                */
/* ------------------------------------------------------------------------- */
static void led_write(uint8_t on)
{
    if (s_led_state == (int8_t)on) return;          /* chi ghi khi thay doi */
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
        case HB_WARNING:   return "WARNING (double blink)";
        case HB_FAULT:     return "FAULT (solid)";
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
            if (el >= HB_RECOVERED_MS) {            /* het thoi gian -> ve NORMAL */
                Status_SetHeartbeat(HB_NORMAL);
                Status_LogMessage("Heartbeat -> NORMAL (he thong da on dinh)");
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

static StatusCmd_t uart_poll_cmd(void)
{
    if (s_huart == NULL) return CMD_NONE;

    /* Xoa loi overrun neu co (doc SR roi DR) */
    if (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(s_huart);
    }
    if (!__HAL_UART_GET_FLAG(s_huart, UART_FLAG_RXNE)) return CMD_NONE;

    char c = (char)(s_huart->Instance->DR & 0xFFU);
    switch (c) {
        case '1':           return CMD_FAULT_LOOP;
        case '2':           return CMD_FAULT_HARD;
        case '3':           return CMD_FAULT_SENSOR;
        case 'r': case 'R': return CMD_SOFT_RESET;
        case 'e': case 'E': return CMD_WWDG_EARLY;
        case 'l': case 'L': return CMD_WWDG_LATE;
        case 's': case 'S': return CMD_STATUS;
        case 'h': case 'H': case '?': return CMD_HELP;
        default:            return CMD_NONE;   /* bo qua \r \n va ky tu la */
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

    uart_send("\r\n", 2);
    Status_LogMessage("==================================================");
    Status_LogMessage("  WATCHDOG RECOVERY SYSTEM - STM32F103C8T6");
    Status_Printf    ("  Build: %s %s", __DATE__, __TIME__);
    Status_LogMessage("  Go 'h' de xem danh sach lenh");
    Status_LogMessage("==================================================");
}

StatusCmd_t Status_Process(void)
{
    Status_WWDG_Service();
    heartbeat_update();
    return uart_poll_cmd();
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

void Status_HeartbeatToggle(void)
{
    HAL_GPIO_TogglePin(LED_HEARTBEAT_GPIO_Port, LED_HEARTBEAT_Pin);
    s_led_state = -1;
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

const char *Status_FaultStr(FaultType_t f)
{
    switch (f) {
        case FAULT_NONE:          return "NONE";
        case FAULT_INFINITE_LOOP: return "INFINITE LOOP";
        case FAULT_HARD_FAULT:    return "HARDFAULT";
        case FAULT_SENSOR_ERROR:  return "SENSOR ERROR";
        default:                  return "?";
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
        case HEALTH_OK:       if (s_hb_mode != HB_RECOVERED) Status_SetHeartbeat(HB_NORMAL); break;
        case HEALTH_WARNING:  Status_SetHeartbeat(HB_WARNING); break;
        case HEALTH_CRITICAL: Status_SetHeartbeat(HB_FAULT);   break;
        default: break;
    }
}

void Status_LogFault(FaultType_t fault)
{
    Status_Printf("[FAULT] Kich hoat loi mo phong: %s", Status_FaultStr(fault));
}

void Status_PrintHelp(void)
{
    Status_LogMessage("------------- DANH SACH LENH -------------");
    Status_LogMessage(" 1 : Treo vong lap      -> IWDG reset");
    Status_LogMessage(" 2 : HardFault          -> IWDG reset");
    Status_LogMessage(" 3 : Loi cam bien");
    Status_LogMessage(" r : Software reset");
    Status_LogMessage(" e : WWDG refresh QUA SOM -> WWDG reset");
    Status_LogMessage(" l : WWDG refresh QUA MUON -> WWDG reset");
    Status_LogMessage(" s : Trang thai he thong");
    Status_LogMessage(" h : Tro giup");
    Status_LogMessage("------------------------------------------");
}

void Status_PrintStatus(void)
{
    uint32_t t = HAL_GetTick();
    Status_Printf("[STATUS] Uptime: %lu.%03lu s | Heartbeat: %s",
                  (unsigned long)(t / 1000U), (unsigned long)(t % 1000U),
                  hb_str(s_hb_mode));
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
/*                                                                           */
/*  PCLK1 = 36 MHz, Prescaler = 8  -> 1 tick = 4096*8/36MHz ~ 0.91 ms         */
/*  Counter = 127 (0x7F), Window = 80 (0x50), reset khi counter < 64 (0x40)  */
/*   - Refresh khi counter > 80 (som hon ~42.8 ms)  -> RESET                 */
/*   - Khong refresh truoc khi counter < 64 (~58.3 ms) -> RESET              */
/*   => Cua so hop le: ~42.8 ms .. ~58.3 ms sau lan refresh truoc            */
/* ------------------------------------------------------------------------- */
void Status_WWDG_Attach(WWDG_HandleTypeDef *hwwdg)
{
    s_hwwdg = hwwdg;
    s_wwdg_stop = 0;
    Status_LogMessage("[WWDG] Da kich hoat - cua so refresh ~42.8..58.3 ms");
}

uint8_t Status_WWDG_IsActive(void)
{
    return (s_hwwdg != NULL) ? 1U : 0U;
}

void Status_WWDG_Service(void)
{
    if (s_hwwdg == NULL || s_wwdg_stop) return;

    uint32_t cnt = WWDG->CR  & WWDG_CR_T;
    uint32_t win = WWDG->CFR & WWDG_CFR_W;

    /* Chi refresh khi counter da xuong toi cua so (counter <= window) */
    if (cnt <= win) {
        HAL_WWDG_Refresh(s_hwwdg);
    }
}

void Status_WWDG_TestEarly(void)
{
    if (s_hwwdg == NULL) {
        Status_LogMessage("[WWDG] Chua bat WWDG (APP_ENABLE_WWDG = 0)");
        return;
    }
    Status_LogMessage("[WWDG] TEST: refresh QUA SOM (counter > window) -> sap reset...");

    /* Doi toi cua so hop le, refresh dung 1 lan (counter = 0x7F)... */
    while ((WWDG->CR & WWDG_CR_T) > (WWDG->CFR & WWDG_CFR_W)) { }
    HAL_WWDG_Refresh(s_hwwdg);
    /* ...roi refresh NGAY lap tuc: 0x7F > 0x50 -> vi pham cua so -> RESET */
    HAL_WWDG_Refresh(s_hwwdg);

    while (1) { }   /* khong toi duoc day */
}

void Status_WWDG_TestLate(void)
{
    if (s_hwwdg == NULL) {
        Status_LogMessage("[WWDG] Chua bat WWDG (APP_ENABLE_WWDG = 0)");
        return;
    }
    Status_LogMessage("[WWDG] TEST: ngung refresh -> reset sau toi da ~58 ms...");
    s_wwdg_stop = 1;
}
