# SV4 – Test ứng dụng nền (đo nhiệt, LED heartbeat, UART log) + so sánh WWDG

## Phần cứng
| Kết nối | Chân STM32 |
|---|---|
| DHT11/DHT22 – DATA | **PA8** (cần trở kéo lên 4.7–10 kΩ; module 3 chân đã có sẵn) |
| DHT – VCC / GND | 3.3 V / GND |
| USB-TTL RX / TX / GND | PA9 / PA10 / GND |
| LED heartbeat | PC13 (onboard) |

Terminal: **115200 8N1**. Nên bật timestamp của terminal (Tera Term: *Setup → Additional settings → Log → Timestamp*), hoặc dùng logic analyzer để đo thời gian reset.

## A. Ứng dụng nền
| # | Thao tác | Kết quả mong đợi | Thực tế |
|---|---|---|---|
| A1 | Cấp nguồn | Banner, `Nguyen nhan: POWER-ON`, LED nháy 1 Hz | |
| A2 | Chờ 2 s | `[TEMP] Nhiet do: xx.x C \| Do am: yy.y %` mỗi 2 s | |
| A3 | Hơ nóng / chạm tay vào DHT | Nhiệt độ tăng dần | |
| A4 | Rút dây DATA của DHT | `Loi doc cam bien: KHONG PHAN HOI`; sau 3 lần → `[HEALTH] WARNING`, LED nháy kép | |
| A5 | Cắm lại DHT | Đọc OK → `[HEALTH] OK`, LED về 1 Hz | |
| A6 | Nhấn NRST | `NRST PIN`, số lần reset tăng | |

## B. So sánh IWDG vs WWDG
Sửa 2 macro trong `main.c` → build → nạp → quan sát log khi tới mốc t = 10 s.

| Kịch bản (`WDG_TEST_SCENARIO`) | `APP_ENABLE_WWDG 0` (chỉ IWDG) | `APP_ENABLE_WWDG 1` (IWDG + WWDG) |
|---|---|---|
| **1** – Treo cứng `while(1)` | Mong đợi: reset sau ~1 s, log `IWDG` → Thực tế: | Mong đợi: reset sau ~58 ms, log `WWDG` → Thực tế: |
| **2** – Task chậm (100 ms/vòng) | Mong đợi: **không reset** (vẫn đọc nhiệt) → Thực tế: | Mong đợi: reset, log `WWDG` (refresh trễ) → Thực tế: |
| **3** – Vòng lặp chạy loạn, feed liên tục | Mong đợi: **không reset**, LED sáng đứng → Thực tế: | Mong đợi: reset ngay, log `WWDG` (refresh sớm) → Thực tế: |

## C. Bảng thông số
| Thông số | Lý thuyết | Đo được |
|---|---|---|
| Timeout IWDG (PR=64, RLR=625) | 1.0 s (0.67–1.33 s do LSI) | |
| Cửa sổ WWDG mở sau refresh | ~42.8 ms | |
| Timeout WWDG | ~58.3 ms | |

## D. Kết luận cho báo cáo
| Tiêu chí | IWDG | WWDG |
|---|---|---|
| Nguồn clock | LSI ~40 kHz (độc lập) | PCLK1 36 MHz |
| Vẫn chạy khi clock chính hỏng | Có | Không |
| Phát hiện treo cứng | Có (chậm, ~1 s) | Có (nhanh, ~58 ms) |
| Phát hiện task chạy **chậm** hơn dự kiến | Chỉ khi chậm hơn ~1 s | Có |
| Phát hiện refresh **quá sớm** (chạy loạn) | **Không** | Có (nhờ cửa sổ) |
| Độ chính xác thời gian | Thấp (LSI lệch) | Cao (thạch anh) |
| Phù hợp | Lưới an toàn cuối cùng | Giám sát luồng chương trình chạy đúng nhịp |
