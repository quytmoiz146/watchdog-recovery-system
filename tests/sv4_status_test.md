# SV4 – Test module `system_status` (LED heartbeat, UART log, WWDG)

## Chuẩn bị
- Nối USB-TTL: `PA9 (TX) → RX`, `PA10 (RX) → TX`, `GND → GND`
- Mở terminal (PuTTY / Tera Term / Hercules): **115200 8N1**, không flow control
- Nạp firmware bằng ST-Link → nhấn nút RESET

## Bảng test

| # | Thao tác | Kết quả mong đợi | Kết quả thực tế |
|---|---|---|---|
| T1 | Cấp nguồn lần đầu | Banner + `Nguyen nhan: POWER-ON`, LED PC13 nháy 1 Hz | |
| T2 | Nhấn nút NRST | `Nguyen nhan: NRST PIN`, số lần reset tăng 1 | |
| T3 | Gõ `h` | In danh sách lệnh | |
| T4 | Gõ `s` | In uptime, chế độ heartbeat, trạng thái WWDG | |
| T5 | Gõ `1` (treo vòng lặp) | LED sáng liên tục, ~1 s sau reset; log `IWDG` + LED nháy nhanh 10 Hz trong 5 s rồi về 1 Hz | |
| T6 | Gõ `2` (HardFault) | Reset sau ~1 s, log `IWDG` | |
| T7 | Gõ `3` (lỗi cảm biến) | Log `[FAULT] SENSOR ERROR`, `[HEALTH] WARNING`, LED nháy kép | |
| T8 | Gõ `r` | Log `SOFTWARE` sau reset | |
| T9 | Đặt `APP_ENABLE_WWDG 1`, nạp lại | Log `[WWDG] Da kich hoat`, hệ thống chạy ổn định, không bị reset | |
| T10 | (WWDG bật) gõ `e` | Reset ngay lập tức, log `WWDG` | |
| T11 | (WWDG bật) gõ `l` | Reset sau ≤ 58 ms, log `WWDG` | |
| T12 | (WWDG bật) gõ `1` | Reset sau ~58 ms (WWDG phát hiện trước IWDG) → log `WWDG` | |

## Đo thời gian (khuyến nghị dùng logic analyzer)
| Thông số | Lý thuyết | Đo được |
|---|---|---|
| Timeout IWDG (PR=64, RLR=625) | 1.0 s (0.67–1.33 s do LSI) | |
| Cửa sổ WWDG mở sau refresh | ~42.8 ms | |
| Timeout WWDG | ~58.3 ms | |

## So sánh IWDG vs WWDG (cho báo cáo)
| Tiêu chí | IWDG | WWDG |
|---|---|---|
| Nguồn clock | LSI ~40 kHz (độc lập) | PCLK1 36 MHz |
| Hoạt động khi clock chính hỏng | Có | Không |
| Phát hiện refresh **quá sớm** | Không | Có (cửa sổ) |
| Độ chính xác thời gian | Thấp (LSI lệch ±50%) | Cao (thạch anh) |
| Timeout trong project | ~1 s | ~58 ms |
| Ứng dụng | Chống treo tổng quát | Kiểm tra luồng chương trình chạy đúng nhịp |
