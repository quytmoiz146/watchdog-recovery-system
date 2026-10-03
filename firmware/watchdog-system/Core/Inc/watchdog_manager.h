/**
  ******************************************************************************
  * @file    watchdog_manager.h
  * @brief   SV1 - Cau hinh IWDG, tinh timeout va chien luoc nap lai watchdog.
  *
  * Chien luoc: IWDG CHI duoc nap lai (refresh) khi MOI tac vu da bao "khoe"
  * (check-in) trong cua so hien tai. Mot tac vu ket / khong khoe -> khong refresh
  * -> IWDG het han -> MCU reset.
  *
  * HAI GIAI DOAN:
  *   1) BOOT - WDG_BootStart() goi NGAY SAU HAL_Init(), truoc ca SystemClock_Config().
  *      IWDG chay voi timeout dai (WDG_BOOT_TIMEOUT_MS) de bao ve ca giai doan khoi
  *      tao: treo khi cau hinh clock, I2C/LCD khong phan hoi... deu duoc reset.
  *      Trong giai doan nay nap bang WDG_BootKick() (chi co tac dung o giai doan BOOT).
  *   2) RUN  - WDG_Init() chuyen sang timeout van hanh va khoa chien luoc "moi tac vu
  *      phai khoe": tu day WDG_BootKick() thanh vo hieu, chi WDG_Supervise() nap duoc.
  *
  * MODULE NAY LA NGUON CAU HINH DUY NHAT CUA IWDG:
  *   - Chi can chon WDG_PROFILE (muc 1). Prescaler / Reload / cac nguong tu tinh.
  *   - IWDG KHONG duoc bat trong CubeMX (khong co MX_IWDG_Init), nen khong the
  *     xung dot cau hinh, va generate lai CubeMX khong lam hong module.
  *   - Truy cap thanh ghi truc tiep (KR/PR/RLR/SR), khong phu thuoc HAL IWDG.
  *
  * Cong thuc timeout (STM32F103):
  *     T_timeout = (Reload + 1) * Prescaler / f_LSI
  *
  * LSI cua STM32F103 KHONG chinh xac: 30 kHz (min) .. 40 kHz (danh dinh) .. 60 kHz (max)
  * => neu lay 40 kHz lam chuan thi timeout that lech toi -33 %/+33 %.
  * Module do f_LSI THUC TE luc khoi dong (WDG_MeasureLsi, dung RTC lam nhan chung vi
  * RTC cung chay bang LSI) roi tinh lai Reload => timeout thuc te bam sat muc tieu
  * tren MOI con chip. Neu do that bai, tu dong quay ve hang so tinh luc bien dich va
  * cac rang buoc WDG_STATIC_ASSERT (kiem tra o truong hop LSI xau nhat) van bao dam.
  ******************************************************************************
  */
#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ===================== 1. CHON CHE DO THOI GIAN (sua 1 dong) ================ */

#define WDG_PROFILE_CUSTOM      0   /* tu dinh nghia 4 macro o nhanh CUSTOM ben duoi */
#define WDG_PROFILE_FAST_0_5S   1   /* kiem tra nhanh: timeout 0,5 s */
#define WDG_PROFILE_STD_1S      2   /* van hanh chuan: timeout 1 s   */
#define WDG_PROFILE_SLOW_2S     3   /* tre dai:        timeout 2 s   */
#define WDG_PROFILE_LONG_5S     4   /* tre rat dai:    timeout 5 s   */

/* Co the ghi de bang Keil: Options > C/C++ > Define: WDG_PROFILE=1 */
#ifndef WDG_PROFILE
#define WDG_PROFILE             WDG_PROFILE_STD_1S
#endif

/*
 * Moi che do dat 4 gia tri (ms):
 *   WDG_TIMEOUT_TARGET_MS    : timeout mong muon tai LSI danh dinh (40 kHz)
 *   WDG_TASK_PERIOD_MS       : chu ky chay cua moi tac vu
 *   WDG_HEALTHY_CYCLE_MAX_MS : do tre xau nhat cho phep de MOI tac vu check-in du
 *   WDG_WARN_AFTER_MS        : sau chung nay ms chua refresh -> bao canh
 * Ca 3 rang buoc (xem WDG_STATIC_ASSERT) deu duoc kiem tra luc bien dich.
 */
#if   WDG_PROFILE == WDG_PROFILE_FAST_0_5S
  #define WDG_TIMEOUT_TARGET_MS     500UL
  #define WDG_TASK_PERIOD_MS         50UL
  #define WDG_HEALTHY_CYCLE_MAX_MS  150UL
  #define WDG_WARN_AFTER_MS         250UL
#elif WDG_PROFILE == WDG_PROFILE_STD_1S
  #define WDG_TIMEOUT_TARGET_MS    1000UL
  #define WDG_TASK_PERIOD_MS        100UL
  #define WDG_HEALTHY_CYCLE_MAX_MS  250UL
  #define WDG_WARN_AFTER_MS         500UL
#elif WDG_PROFILE == WDG_PROFILE_SLOW_2S
  #define WDG_TIMEOUT_TARGET_MS    2000UL
  #define WDG_TASK_PERIOD_MS        100UL
  #define WDG_HEALTHY_CYCLE_MAX_MS  500UL
  #define WDG_WARN_AFTER_MS        1000UL
#elif WDG_PROFILE == WDG_PROFILE_LONG_5S
  #define WDG_TIMEOUT_TARGET_MS    5000UL
  #define WDG_TASK_PERIOD_MS        100UL
  #define WDG_HEALTHY_CYCLE_MAX_MS 1000UL
  #define WDG_WARN_AFTER_MS        2500UL
#elif WDG_PROFILE == WDG_PROFILE_CUSTOM
  #if !defined(WDG_TIMEOUT_TARGET_MS) || !defined(WDG_TASK_PERIOD_MS) || \
      !defined(WDG_HEALTHY_CYCLE_MAX_MS) || !defined(WDG_WARN_AFTER_MS)
    #error "WDG_PROFILE_CUSTOM: phai dinh nghia du 4 macro WDG_TIMEOUT_TARGET_MS, WDG_TASK_PERIOD_MS, WDG_HEALTHY_CYCLE_MAX_MS, WDG_WARN_AFTER_MS (kieu UL)"
  #endif
#else
  #error "WDG_PROFILE khong hop le (dung WDG_PROFILE_FAST_0_5S / STD_1S / SLOW_2S / LONG_5S / CUSTOM)"
#endif

/* ===================== 2. THAM SO PHAN CUNG & DEBUG ======================== */

/* Tan so LSI (Hz): danh dinh va hai bien theo datasheet STM32F103 */
#define WDG_LSI_NOMINAL_HZ        40000UL
#define WDG_LSI_MIN_HZ            30000UL
#define WDG_LSI_MAX_HZ            60000UL

/* Timeout cua giai doan BOOT (ms, tai LSI danh dinh).
 * Phai du dai cho: cau hinh clock + khoi tao ngoai vi + do LSI (toi da ~2,7 s). */
#define WDG_BOOT_TIMEOUT_MS       8000UL

/* 1: do f_LSI thuc te bang RTC luc khoi dong (can RTC da duoc khoi tao bang LSI) */
#define WDG_MEASURE_LSI           1
/* 1: dung f_LSI do duoc de tinh lai Reload => timeout thuc te bam sat muc tieu */
#define WDG_AUTO_TRIM_TO_LSI      1
/* Thoi gian cho toi da cho moi buoc do LSI (ms) */
#define WDG_LSI_MEAS_TIMEOUT_MS   3000UL

/* Debug: dung IWDG khi CPU dang halt o debugger (tranh reset khi dat breakpoint) */
#define WDG_FREEZE_IN_DEBUG       1

/* Chan xung moc (Kick Marker): DAO MUC ngay truoc moi lan nap IWDG.
 * Cam oscilloscope/logic analyzer: CH1 = chan nay, CH2 = NRST.
 * Do tu canh cuoi cung cua CH1 den luc NRST sut ve 0 V = timeout thuc te. */
#define WDG_KICK_MARKER_ENABLE    1
#define WDG_MARKER_PORT           GPIOB
#define WDG_MARKER_PIN            GPIO_PIN_0              /* PB0 (chua dung vao viec khac) */
#define WDG_MARKER_CLK_ENABLE()   __HAL_RCC_GPIOB_CLK_ENABLE()

/* ===================== 3. TINH TOAN TIMEOUT (compile-time) ================= */

/* So nhip dem can thiet voi he so chia div: ticks = T * f_LSI / (div * 1000) */
#define WDG_TICKS_AT(div) \
    ((WDG_TIMEOUT_TARGET_MS * WDG_LSI_NOMINAL_HZ) / ((div) * 1000UL))

/* Tu chon he so chia NHO NHAT ma Reload con vua 12 bit (<= 4096 nhip):
 * he so chia cang nho thi do phan giai cang min. WDG_PR_BITS = gia tri ghi vao IWDG_PR. */
#if   WDG_TICKS_AT(4UL)   <= 4096UL
  #define WDG_PRESCALER_DIV   4UL
  #define WDG_PR_BITS         0U
#elif WDG_TICKS_AT(8UL)   <= 4096UL
  #define WDG_PRESCALER_DIV   8UL
  #define WDG_PR_BITS         1U
#elif WDG_TICKS_AT(16UL)  <= 4096UL
  #define WDG_PRESCALER_DIV   16UL
  #define WDG_PR_BITS         2U
#elif WDG_TICKS_AT(32UL)  <= 4096UL
  #define WDG_PRESCALER_DIV   32UL
  #define WDG_PR_BITS         3U
#elif WDG_TICKS_AT(64UL)  <= 4096UL
  #define WDG_PRESCALER_DIV   64UL
  #define WDG_PR_BITS         4U
#elif WDG_TICKS_AT(128UL) <= 4096UL
  #define WDG_PRESCALER_DIV   128UL
  #define WDG_PR_BITS         5U
#elif WDG_TICKS_AT(256UL) <= 4096UL
  #define WDG_PRESCALER_DIV   256UL
  #define WDG_PR_BITS         6U
#else
  #error "WDG_TIMEOUT_TARGET_MS qua lon: IWDG toi da ~26 s o LSI 40 kHz"
#endif

/* Reload = ticks - 1 (thanh ghi IWDG_RLR, 12 bit) */
#define WDG_RELOAD_VALUE        (WDG_TICKS_AT(WDG_PRESCALER_DIV) - 1UL)

/* Timeout (ms) ung voi tan so LSI bat ky */
#define WDG_TIMEOUT_MS_AT(lsi_hz) \
    (((WDG_RELOAD_VALUE + 1UL) * WDG_PRESCALER_DIV * 1000UL) / (lsi_hz))

#define WDG_TIMEOUT_NOM_MS      WDG_TIMEOUT_MS_AT(WDG_LSI_NOMINAL_HZ)
#define WDG_TIMEOUT_MIN_MS      WDG_TIMEOUT_MS_AT(WDG_LSI_MAX_HZ)   /* LSI nhanh nhat -> ngan nhat */
#define WDG_TIMEOUT_MAX_MS      WDG_TIMEOUT_MS_AT(WDG_LSI_MIN_HZ)   /* LSI cham nhat  -> dai nhat  */

/* Kiem tra luc bien dich (khong dung _Static_assert de tuong thich ARMCC v5) */
#define WDG_STATIC_ASSERT(name, cond)  typedef char wdg_assert_##name[(cond) ? 1 : -1]

WDG_STATIC_ASSERT(reload_in_range,   WDG_RELOAD_VALUE <= 0x0FFFUL);          /* RLR 12 bit   */
WDG_STATIC_ASSERT(reload_not_zero,   WDG_RELOAD_VALUE >= 1UL);
WDG_STATIC_ASSERT(nominal_accuracy,  WDG_TIMEOUT_NOM_MS * 100UL >= WDG_TIMEOUT_TARGET_MS * 99UL); /* sai so <1% do lam tron */
WDG_STATIC_ASSERT(margin_over_cycle, WDG_TIMEOUT_MIN_MS >= 2UL * WDG_HEALTHY_CYCLE_MAX_MS);
WDG_STATIC_ASSERT(warn_before_reset, WDG_WARN_AFTER_MS  <  WDG_TIMEOUT_MIN_MS);
WDG_STATIC_ASSERT(task_in_cycle,     WDG_TASK_PERIOD_MS <  WDG_HEALTHY_CYCLE_MAX_MS);
WDG_STATIC_ASSERT(warn_after_cycle,  WDG_WARN_AFTER_MS  >  WDG_HEALTHY_CYCLE_MAX_MS);

/* Giai doan BOOT phai dai hon giai doan RUN va van bieu dien duoc bang IWDG */
WDG_STATIC_ASSERT(boot_longer,       WDG_BOOT_TIMEOUT_MS >= WDG_TIMEOUT_TARGET_MS);
WDG_STATIC_ASSERT(boot_representable,
                  ((WDG_BOOT_TIMEOUT_MS * WDG_LSI_NOMINAL_HZ) / (256UL * 1000UL)) <= 4096UL);
/* Chan tran 32 bit trong phep tinh runtime target_ms * f_LSI (f_LSI toi da 120 kHz) */
WDG_STATIC_ASSERT(no_overflow_run,   WDG_TIMEOUT_TARGET_MS <= 35000UL);
WDG_STATIC_ASSERT(no_overflow_boot,  WDG_BOOT_TIMEOUT_MS   <= 35000UL);

/* ===================== 4. DANH SACH TAC VU DUOC GIAM SAT =================== */

typedef enum
{
  WDG_TASK_HEARTBEAT = 0,   /* LED heartbeat                       */
  WDG_TASK_UART,            /* kenh log UART san sang / truyen duoc */
  WDG_TASK_SCHED,           /* bo lap lich / time base (SysTick)   */
  WDG_TASK_COUNT            /* luon de cuoi cung                   */
} WDG_TaskId;

#define WDG_ALL_TASKS_MASK  ((1UL << WDG_TASK_COUNT) - 1UL)

/* Giai doan hoat dong cua watchdog */
typedef enum
{
  WDG_PHASE_OFF = 0,   /* chua bat IWDG                                       */
  WDG_PHASE_BOOT,      /* dang khoi tao: timeout dai, nap bang WDG_BootKick() */
  WDG_PHASE_RUN        /* van hanh: chi WDG_Supervise() duoc nap              */
} WDG_Phase;

/* ===================== 5. API ============================================== */

/**
  * @brief  BAT IWDG voi timeout dai (WDG_BOOT_TIMEOUT_MS) de bao ve giai doan khoi tao.
  *         Goi NGAY SAU HAL_Init(), TRUOC SystemClock_Config(): IWDG chay bang LSI nen
  *         khong phu thuoc cau hinh clock he thong. IWDG mot khi da bat khong tat duoc.
  * @retval HAL_OK, hoac HAL_TIMEOUT neu thanh ghi IWDG khong cap nhat kip
  */
HAL_StatusTypeDef WDG_BootStart(void);

/**
  * @brief  Nap lai IWDG trong giai doan khoi tao (truoc WDG_Init). Dat xen giua cac
  *         buoc khoi tao dai. Sau khi WDG_Init() chay, ham nay KHONG con tac dung -
  *         nho vay khong the vo tinh pha chien luoc "chi nap khi moi tac vu khoe".
  */
void WDG_BootKick(void);

/**
  * @brief  Do tan so LSI thuc te bang RTC (RTC phai da khoi tao va chay bang LSI).
  *         Dem thoi gian giua hai nhip giay cua RTC bang HAL_GetTick() (goc HSE/PLL,
  *         chinh xac) => f_LSI = LSI_VALUE * 1000 / so_ms_do_duoc.
  *         Goi SAU MX_RTC_Init() va TRUOC WDG_Init().
  * @retval 1 neu do thanh cong (ket qua da duoc luu), 0 neu that bai (van dung danh dinh)
  */
uint8_t WDG_MeasureLsi(void);

/**
  * @brief  Chuyen sang giai doan RUN: tinh lai Prescaler/Reload theo f_LSI dang co
  *         (do duoc neu WDG_AUTO_TRIM_TO_LSI = 1, nguoc lai dung hang so bien dich),
  *         nap lai IWDG va khoa chien luoc "moi tac vu phai khoe".
  *         Goi 1 lan, sau cac MX_xxx_Init() va sau WDG_MeasureLsi().
  * @retval HAL_OK, hoac HAL_TIMEOUT neu thanh ghi IWDG khong cap nhat kip
  */
HAL_StatusTypeDef WDG_Init(void);

/**
  * @brief  Tac vu goi ham nay khi (va chi khi) no vua hoan thanh mot chu ky
  *         VA tu kiem tra thay minh khoe. An toan khi goi tu ngat.
  */
void WDG_Checkin(WDG_TaskId id);

/**
  * @brief  Goi trong vong lap chinh. Nap lai IWDG neu va chi neu moi tac vu
  *         da check-in tu lan refresh truoc; sau do xoa co de bat dau cua so moi.
  *         KHONG goi ham nay trong ngat.
  * @retval 1 neu da refresh, 0 neu chua (co tac vu chua khoe)
  */
uint8_t WDG_Supervise(void);

/* Mat na cac tac vu CHUA check-in trong cua so hien tai (bit = WDG_TaskId) */
uint32_t WDG_GetMissingMask(void);

/* So lan refresh da thuc hien va tick cua lan refresh gan nhat */
uint32_t WDG_GetRefreshCount(void);
uint32_t WDG_GetLastRefreshTick(void);

/* ---- Thong tin cau hinh dang chay (de in log / bao cao) ------------------- */
WDG_Phase WDG_GetPhase(void);
uint32_t  WDG_GetLsiHz(void);          /* f_LSI dang dung: do duoc hoac danh dinh */
uint8_t   WDG_IsLsiMeasured(void);     /* 1 neu gia tri tren la do thuc te        */
uint32_t  WDG_GetPrescalerDiv(void);   /* he so chia dang nap trong IWDG_PR       */
uint32_t  WDG_GetReload(void);         /* gia tri dang nap trong IWDG_RLR         */
uint32_t  WDG_GetTimeoutMs(void);      /* timeout thuc te voi cau hinh + f_LSI hien tai */
uint32_t  WDG_GetTimeoutMsAt(uint32_t lsi_hz);  /* timeout neu LSI = lsi_hz */

/**
  * @brief  Hook (weak) goi MOT LAN moi cua so khi qua WDG_WARN_AFTER_MS ma chua
  *         refresh duoc. Ung dung ghi de de in log / luu loi (SV3, SV4 mo rong).
  */
void WDG_OnUnhealthy(uint32_t missing_mask, uint32_t ms_since_refresh);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_MANAGER_H */
