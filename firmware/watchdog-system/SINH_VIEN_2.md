# Sinh viên 2 — Mô phỏng lỗi bằng nút

## Trạng thái

Đã tích hợp module vào `main.c` và danh sách nguồn của project Keil. Build toàn bộ project bằng Keil uVision / Arm Compiler 6.24: **0 lỗi, 0 cảnh báo**, đã tạo AXF và HEX. Chưa kiểm thử trên bo mạch thật; cần thực hiện các bước bên dưới để xác nhận reset và HardFault trên MCU.

## Mở project

- Máy hiện tại có Arm Compiler 6: mở `MDK-ARM/watchdog-system-sv2-ac6.uvprojx`, chọn target `watchdog-system`, nhấn Rebuild (F7).
- HEX: `MDK-ARM/sv2-ac6/watchdog-system.hex`.
- Project gốc `MDK-ARM/watchdog-system.uvprojx` vẫn giữ lựa chọn Compiler 5 và đã thêm nguồn `fault_injection.c`. Dùng project này trên máy có Compiler 5. Máy hiện tại chưa cài Compiler 5 nên dùng project AC6 riêng phía trên.
- Hai project dùng chung mã nguồn, chân GPIO và cấu hình ngoại vi; không phải hai bản firmware có chức năng khác nhau.

## Cách bấm nút

Dùng đúng hai chân đã được nhóm cấu hình GPIO input pull-up. Khi bấm, nút nối chân xuống GND.

| Thao tác | Lỗi mô phỏng |
|---|---|
| Bấm rồi thả BTN_FAULT1 / PA0 | Treo vòng lặp chính vô hạn |
| Bấm rồi thả BTN_FAULT2 / PA1 | Chờ cờ cảm biến `sensor_ready` vô hạn |
| Giữ đồng thời cả hai nút | Đọc địa chỉ sai `0xFFFFFFF0` để gây HardFault |

Chống dội 30 ms. Nên giữ/thả ít nhất 100 ms khi thử. Nút đơn kích hoạt khi **thả**, để có thời gian bấm nút thứ hai cho tổ hợp HardFault. Có thể bấm nút thứ nhất trước rồi giữ và bấm thêm nút thứ hai.

Sau khởi động/reset, thả cả hai nút ít nhất 30 ms trước khi thực hiện lỗi mới. Nếu vẫn giữ nút lúc reset, module chưa kích hoạt lỗi tiếp; tránh lặp reset do nút chưa được thả.

## Phần đã thêm và ghép

- `Core/Inc/fault_injection.h`: enum ba lỗi và API `Fault_Init`, `Fault_Process`, `Fault_GetActive`.
- `Core/Src/fault_injection.c`: đọc nút đã cấu hình sẵn, chống dội, xử lý tổ hợp, thực thi ba lỗi. Không khởi tạo GPIO, không điều khiển LED/LCD/UART, không refresh watchdog.
- `Core/Src/main.c`: chỉ thêm trong vùng `USER CODE`: include, trạng thái/hàm hỗ trợ, gọi `Fault_Init()` sau các hàm khởi tạo và `Fault_Process()` trong vòng lặp chính.
- `MDK-ARM/watchdog-system.uvprojx`: chỉ bổ sung `fault_injection.c` vào nhóm `Application/User/Core`.
- `MDK-ARM/watchdog-system-sv2-ac6.uvprojx`: project bổ sung chọn Compiler 6 và thư mục output riêng, dùng được trên máy hiện tại.

Không sửa `.ioc`, thư viện HAL/CMSIS, startup, handler ngắt, các hàm cấu hình clock/GPIO/ngoại vi, hoặc lựa chọn compiler của project gốc. Bản sao các file trước tích hợp nằm ở `D:/stm32/sv2/sv2_reclone_20261009/integration/backup`.

## Watchdog khi chạy thử

Vòng lặp trong bản clone còn trống, trong khi cả IWDG và WWDG đã được khởi tạo. Vì vậy cần service trong vòng lặp chính để MCU không reset ngay cả khi chưa chọn lỗi.

Hàm `SV2_DemoWatchdogService()` trong `USER CODE` chỉ phục vụ bản thử SV2 hiện tại:

- IWDG: refresh sau mỗi 100 ms khi vòng lặp chính còn hoạt động; refresh một lần lúc hoàn tất khởi tạo.
- WWDG: đọc bộ đếm thực, chỉ refresh khi `0x3F < counter < hwwdg.Init.Window`; giữ nguyên prescaler/window/counter đã cấu hình.
- Gọi service **sau** `Fault_Process()`. Khi một trong ba lỗi làm treo luồng chính/đi vào HardFault, service không được chạy tiếp, watchdog sẽ hết hạn.

Với cấu hình hiện tại, WWDG có thời gian ngắn hơn IWDG nên dự kiến gây reset trước. Ba lỗi kiểm tra đường phục hồi của cấu hình hiện có; không tách riêng phép thử IWDG và WWDG, không thay đổi cấu hình để chọn watchdog khác.

Khi ghép supervisor của sinh viên 1, thay lời gọi `SV2_DemoWatchdogService()` bằng cơ chế kiểm tra sức khỏe chung của nhóm. Giữ `Fault_Process()` trong main, gọi thường xuyên (khuyến nghị không quá 10 ms giữa hai lần gọi). Không đưa refresh vào ISR/SysTick/HardFault hoặc các vòng lặp lỗi. Nếu thêm tác vụ chặn lâu, cần đánh giá lại lịch chạy theo cửa sổ WWDG hiện tại.

## Kiểm thử trên bo mạch

1. Build project AC6, nạp HEX. Khi không bấm nút, MCU cần chạy ổn định qua nhiều chu kỳ watchdog. Có thể đặt breakpoint ở đầu `main()` để kiểm tra reset ngoài ý muốn, sau đó chạy tự do; breakpoint/debug có thể ảnh hưởng phép đo thời gian watchdog.
2. Thả hai nút sau boot. Bấm/thả PA0: mã đi vào vòng lặp vô hạn; sau đó MCU cần khởi động lại. Xác nhận `RCC_FLAG_WWDGRST`/`RCC_FLAG_IWDGRST` trước khi phần khác xóa reset flags.
3. Thả nút sau reset. Bấm/thả PA1: mã chờ `sensor_ready == 0` vô hạn; xác nhận MCU tự reset.
4. Thả hai nút sau reset. Giữ cả hai nút: kiểm tra vào **`HardFault_Handler`** bằng debugger. Nếu cần xem nguyên nhân trước reset, xem `SCB->HFSR` và `SCB->CFSR`. Sau đó bỏ breakpoint, chạy tự do để xác nhận watchdog reset.
5. Giữ nút qua một lần reset rồi thả: không được tái kích hoạt lỗi ngay sau boot; thử thêm một thao tác mới.
6. Thử bấm hai nút lệch nhau và bấm nhanh dưới 30 ms: tổ hợp hợp lệ phải chọn HardFault; xung quá ngắn không được chọn lỗi.

Phép đọc địa chỉ sai phải kiểm tra trên STM32 thật. Một số mô phỏng không tạo bus fault tại địa chỉ chưa ánh xạ; vòng lặp dự phòng vẫn làm watchdog reset nhưng **chỉ reset chưa đủ chứng minh đã vào HardFault**. Chưa có kết quả thử phần cứng trong lần tích hợp này.

## Kết quả kiểm tra trên máy

- Đã build cả main, module SV2, startup và toàn bộ các nguồn được project Keil liệt kê; linker thành công và tạo HEX.
- Keil AC6: Code 6668 byte, RO-data 312 byte, RW-data 12 byte, ZI-data 1868 byte; nằm trong giới hạn Flash/RAM cấu hình hiện tại.
- Kiểm tra C nghiêm ngặt bổ sung với `-Wall -Wextra -Werror`: tất cả nguồn C biên dịch thành công; assembly dùng armasm có thông báo công cụ đã deprecated, không phải lỗi mã nguồn.
- Nhật ký build Keil: `D:/stm32/sv2/sv2_reclone_20261009/integration/keil-build.log`.
