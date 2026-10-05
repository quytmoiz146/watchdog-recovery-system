# Bản sửa phần sinh viên 2 — Đề tài Dễ 14

Đề tài: **Hệ thống giám sát tự phục hồi dùng Watchdog**. Đối chiếu dòng 16, ô I16 trong bảng phân công: sinh viên 2 mô phỏng lỗi bằng nút nhấn, gồm **treo vòng lặp, chờ cảm biến vô hạn và HardFault do truy cập địa chỉ sai**.

Toàn bộ bản sửa nằm trong `D:\stm32\sv2\sv2_da_sua`. Các tệp của bài đã làm ở thư mục cha được giữ riêng. Đây là chương trình chạy thử độc lập phần sinh viên 2, có IWDG và LED hỗ trợ quan sát sự phục hồi.

## 1. Tệp cần mở

| Mục đích | Tệp trong thư mục bản sửa |
| --- | --- |
| Xem/sửa cấu hình CubeMX | `sv2_da_sua.ioc` |
| Mở và biên dịch bằng Keil | `MDK-ARM/sv2_da_sua.uvprojx` |
| Mã chính phần sinh viên 2 | `Core/Src/fault_injection.c` và `Core/Inc/fault_injection.h` |
| Chạy thử, điều khiển LED, làm mới IWDG | `Core/Src/main.c` |
| Cấu hình timer 10 ms | `Core/Src/tim.c` |
| Cấu hình watchdog | `Core/Src/iwdg.c` |
| Chụp thông tin HardFault để xem khi debug | `Core/Src/stm32f1xx_it.c` |

Chọn đúng project có tên `sv2_da_sua` khi build/nạp. Mã nguồn dùng STM32F103C8T6, HAL STM32F1, Arm Compiler 6 và ST-LINK như project ban đầu.

**Kết quả kiểm tra bản sửa:** đã biên dịch/link trực tiếp bằng Arm Compiler 6.24 và tạo HEX thành công; mã C qua `-Wall -Wextra -Werror`. Assembler có một cảnh báo `A1950W` về công cụ `armasm` cũ, không phải lỗi mã C. Đã đối chiếu tham số `.ioc`/C, tham chiếu project và hash của 148 tệp gốc đều giữ nguyên. Kiểm tra mã máy cho thấy lệnh đọc địa chỉ sai vẫn còn sau tối ưu hóa. Chưa kiểm tra bằng giao diện CubeMX/uVision hoặc nạp chạy trên board.

Tệp nạp: `MDK-ARM/sv2_da_sua/sv2_da_sua.hex`; log: `MDK-ARM/sv2_da_sua/validation_build.log`. Có thể chạy lại kiểm tra bằng `python kiem_tra.py` trong thư mục bản sửa; nếu Arm Compiler ở vị trí khác, truyền đường dẫn thư mục `bin` làm đối số đầu tiên.

## 2. Điểm chưa ổn và cách sửa

| Phân loại | Bản ban đầu | Bản sửa |
| --- | --- | --- |
| **Lỗi thời gian rõ ràng** | `TIM2` có Prescaler = 7199, Period = 65535 nhưng hàm quét được ghi là mỗi 10 ms. Thực tế mỗi lần cập nhật cách nhau 6,5536 giây; bấm ngắn thường không được nhận. | Period = **99** trong cả `.ioc` và `tim.c`, tạo chu kỳ 10 ms. |
| **Lỗi phục hồi khi giữ nút** | Sau reset, bộ đếm nút trở về 0; nút còn giữ tiếp tục kích hoạt lỗi rồi reset lặp lại. | Mỗi nút phải được thả ổn định trước khi được phép kích hoạt. Giữ nút qua reset không tự gây lỗi lần nữa. |
| **Lỗi chọn sự kiện khi nhiều nút cùng hợp lệ** | Vòng quét có thể ghi đè lỗi trước bằng nút xử lý sau, kể cả nút reset mềm. | Lỗi đầu tiên được chốt đến khi khởi động lại. Nếu nhiều nút đạt ngưỡng trong cùng lượt quét, ưu tiên PB12 → PB13 → PB14 → PB15. |
| **Thiếu xử lý cờ reset đầy đủ** | Chỉ xóa cờ khi thấy IWDG reset; các nguồn reset khác có thể còn cờ tích lũy. | Chụp `RCC->CSR` vào `g_reset_flags` ngay đầu `main`, giải mã `g_reset_cause`, rồi xóa cờ trong mọi lần khởi động. |
| **Cải tiến thử nghiệm HardFault** | Nhảy qua con trỏ hàm tới `0xFFFFFFFF`, khó giải thích trực tiếp theo yêu cầu truy cập địa chỉ sai. | Đọc `volatile` từ địa chỉ dành riêng `0xFFFFFFF0`, tắt xử lý BusFault riêng để lỗi này chuyển thành HardFault, chụp thanh ghi lỗi trong handler. Cách cũ không được kết luận là chắc chắn không hoạt động. |
| **Cải tiến kiểm tra tiến độ** | Làm mới watchdog liên tục trong vòng lặp chính. | Làm mới khoảng mỗi 100 ms, khi vòng chính còn chạy, bộ quét timer có tiến độ và chưa chốt lỗi. Không làm mới trong ngắt quét nút. |
| **Cải tiến khởi động timer** | Bỏ qua kết quả `HAL_TIM_Base_Start_IT`; có thể xử lý ngay cờ update sinh ra lúc khởi tạo. | Xóa cờ update/cờ ngắt chờ, đặt bộ đếm về 0, kiểm tra kết quả start. |
| **Cải tiến debug** | Luôn gọi macro đóng băng IWDG khi CPU bị debugger dừng. | Mặc định `SV2_FREEZE_IWDG_ON_DEBUG = 0`; có tùy chọn bật rõ ràng để xem lỗi tại breakpoint. |
| **Cải tiến tổ chức project** | `fault_injection.c` đặt trong `Core/Inc`. Keil vẫn có thể biên dịch vì project đã tham chiếu tệp này. | Chuyển module sang `Core/Src` và sửa tham chiếu trong project Keil. Đây là chỉnh cách tổ chức, không phải nguyên nhân lỗi biên dịch của bản cũ. |

`volatile` ở biến dùng chung ISR/main được giữ lại. Bản cũ đã đọc một lần biến lỗi trước khi `switch`; không quy lỗi cho việc đọc bị xé nhỏ. Vấn đề thực tế là sự kiện có thể bị ghi đè trong ISR.

## 3. Cấu hình và phép tính

| Thành phần | Giá trị |
| --- | --- |
| Vi điều khiển | STM32F103C8T6, Flash 64 KB, RAM 20 KB |
| HSE | Thạch anh ngoài 8 MHz, chế độ Crystal/Ceramic Resonator |
| PLL / SYSCLK / HCLK | HSE × 9 = 72 MHz |
| APB1 / clock TIM2 | APB1 chia 2 → PCLK1 = 36 MHz; clock timer = 72 MHz |
| TIM2 | Prescaler = 7199; Period/ARR = 99; ngắt cập nhật bật |
| Nút PB12–PB15 | GPIO Input, Pull-up nội; nhấn = mức thấp |
| LED PC13 | Output Push Pull, tốc độ thấp; mức ban đầu HIGH |
| IWDG | LSI; Prescaler = 32; Reload = 4095 |
| SWD | PA13 = SWDIO, PA14 = SWCLK |

Chu kỳ ngắt timer được tính từ cả prescaler và auto-reload:

```text
T_TIM2 = (Prescaler + 1) × (Period + 1) / f_TIM2

Bản cũ: (7199 + 1) × (65535 + 1) / 72.000.000 = 6,5536 s
Bản sửa: (7199 + 1) × (99 + 1) / 72.000.000 = 0,010 s = 10 ms
```

Bộ lọc nhận **3 mẫu liên tiếp** ở trạng thái nhấn hoặc thả. Với quét đều 10 ms, thời gian nhận thay đổi khoảng **20–30 ms** tính từ lúc tín hiệu đổi, tùy pha lấy mẫu; không phải cam kết tín hiệu luôn ổn định đủ ít nhất 30 ms. Nếu tiếp điểm dội, mẫu ngược mức làm khởi động lại bộ đếm tương ứng. Trên bản cũ, ba mẫu cách nhau 6,5536 s khiến thao tác nhấn có thể phải kéo dài khoảng 13,1–19,7 s.

Thời gian watchdog danh định sau lần nạp lại bộ đếm:

```text
T_IWDG = Prescaler × (Reload + 1) / f_LSI
        = 32 × 4096 / 40.000
        = 3,2768 s

Nếu f_LSI = 60 kHz: T ≈ 2,1845 s
Nếu f_LSI = 30 kHz: T ≈ 4,3691 s
```

LSI có giá trị min/typ/max **30/40/60 kHz** theo điều kiện trong bảng 25, trang 55 của [datasheet STM32F103x8/xB, DS5319](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf). Vì vậy khoảng **2,1845–4,3691 s** trên là phép tính theo dải LSI của tài liệu, không phải kết quả đã đo trên board.

Khoảng làm mới watchdog là 100 ms. Tính từ lúc lỗi được kích hoạt, bộ đếm có thể đã chạy một phần của khoảng 100 ms kể từ lần làm mới trước đó. Tính từ lúc tay bắt đầu nhấn còn cộng thời gian nhận nút; tính đến lúc LED báo khởi động còn cộng thời gian khởi động và lần bật LED đầu tiên. Không dùng thời gian bấm-nhìn-LED để khẳng định timeout phải đúng 3,2768 s. Clock timer/APB và cơ chế IWDG được mô tả trong [RM0008 của ST](https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101-103-105-107-stm32f100-series-armbased-32bit-mcus-stmicroelectronics.pdf).

## 4. Đấu nối và thao tác

Mỗi nút thường hở nối **một đầu vào chân PB tương ứng, đầu còn lại vào GND**. Pull-up nội đã bật nên trạng thái thả là HIGH, nhấn là LOW. Board và ST-LINK dùng chung GND.

| Nút | Tên trong mã | Hành vi khi nhấn |
| --- | --- | --- |
| PB12 | `BTN_LOOP` | Kẹt vòng lặp; LED đảo trạng thái khoảng mỗi 50 ms; IWDG không được làm mới. |
| PB13 | `BTN_SENSOR` | Chờ biến mô phỏng cảm biến mãi mãi; LED giữ trạng thái hiện tại; IWDG không được làm mới. |
| PB14 | `BTN_HARDFAULT` | Truy cập địa chỉ sai để vào HardFault; chờ IWDG reset. |
| PB15 | `BTN_RESET` | Reset mềm qua `NVIC_SystemReset()`; nút phụ để đối chiếu nguồn reset. |

Chương trình giả định LED PC13 của board sáng khi chân ở **LOW**: mức HIGH tắt LED. Nếu dùng LED rời, đấu để PC13 kéo dòng xuống qua LED và điện trở hạn dòng phù hợp. Board phải có thạch anh HSE 8 MHz tương ứng cấu hình; khi đổi loại board hoặc nguồn clock phải chỉnh lại cấu hình clock.

Sau khi bật nguồn/reset, thả các nút ít nhất 50 ms trước lần thử mới. Có thể chờ hết tín hiệu khởi động rồi giữ một nút khoảng 100 ms để thao tác dễ quan sát. Chỉ cần một lần nhấn được nhận; lỗi được chốt nên thả nút không hủy phép thử.

## 5. Tín hiệu LED và kết quả mong đợi

Khi chưa nhấn nút trong giai đoạn khởi động, LED chớp ngắn với thời gian mỗi trạng thái khoảng 100 ms:

| Nguồn reset được giải mã | Số chớp ngắn lúc khởi động |
| --- | --- |
| IWDG | 3 |
| Reset mềm PB15 | 2 |
| Cấp nguồn, nút NRST hoặc nguồn còn lại | 1 |

Sau đó LED đảo trạng thái mỗi 500 ms, tức chu kỳ sáng-tắt hoàn chỉnh khoảng 1 giây. Nếu nhấn nút gây lỗi trong lúc báo khởi động, chuỗi báo này có thể bị ngắt bởi phép thử.

PB12, PB13 và PB14 đều có kết quả phục hồi mong đợi là **IWDG reset**, sau đó ba chớp ngắn và chạy bình thường. Riêng HardFault là lỗi CPU xảy ra trước; watchdog mới là nguồn gây reset. RCC không có một cờ reset riêng tên “HardFault”. Chỉ nhìn thấy IWDG reset chưa đủ chứng minh PB14 đã thực sự vào `HardFault_Handler`, vì vòng lặp dự phòng không làm mới watchdog cũng gây cùng kiểu reset.

`g_reset_flags` giữ ảnh chụp các cờ của lần khởi động hiện tại; `g_reset_cause` giữ kết quả giải mã. Các biến này không phải bộ đếm hoặc nhật ký reset được lưu lâu dài.

## 6. Xác nhận HardFault và lưu ý debugger

Địa chỉ `0xFFFFFFF0` nằm trong vùng dành riêng trên bản đồ bộ nhớ STM32F103x8/xB. Mã tắt `BUSFAULTENA` rồi dùng các lệnh đồng bộ trước khi đọc; lỗi bus không có handler riêng được phép chuyển thành HardFault theo [PM0056, mục 2.4.2](https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf). Đọc vùng dành riêng vẫn cần kiểm tra trên phần cứng thực tế; trình mô phỏng hoặc chip khác có thể xử lý khác.

Để xác nhận PB14, đặt breakpoint trong `HardFault_Handler` sau ba lệnh chụp thanh ghi, kiểm tra đã vào đúng handler và xem:

- `g_hardfault_hfsr`: bit `FORCED` cho biết lỗi được chuyển thành HardFault.
- `g_hardfault_cfsr`: kiểm tra nguyên nhân lỗi bus; với đọc địa chỉ không hợp lệ thường có `PRECISERR`.
- `g_hardfault_bfar`: chỉ dùng địa chỉ này khi bit `BFARVALID` trong CFSR được đặt; giá trị mong đợi của phép đọc là `0xFFFFFFF0`.

Ba biến chụp lỗi nằm trong RAM thông thường và được khởi tạo lại khi reset. Chúng hỗ trợ xem lỗi **trước reset**, không lưu lịch sử xuyên reset.

Mặc định `SV2_FREEZE_IWDG_ON_DEBUG = 0`: watchdog vẫn chạy khi CPU bị dừng ở breakpoint, nên có thể reset khi đang xem thanh ghi. Để quan sát thuận tiện, tạm đặt macro bằng 1, build/nạp lại rồi thử PB14. Khi CPU bị debugger halt, IWDG được đóng băng; khi Run trở lại, watchdog tiếp tục đếm. Sau khi xác nhận handler, đưa macro về 0 và thử phục hồi khi chương trình chạy liên tục. Không dùng thời gian đo lúc halt/step làm số liệu timeout.

## 7. Danh sách kiểm thử cần thực hiện

Các mục dưới đây là tiêu chí chạy thử, không phải tuyên bố đã kiểm chứng phần cứng.

- [ ] Mở project Keil bản sửa, Rebuild và kiểm tra log biên dịch; nạp đúng chương trình lên board.
- [ ] Cấp nguồn, thả tất cả nút: một chớp ngắn rồi LED chạy bình thường; để chạy ít nhất 30 giây không tự reset.
- [ ] PB12: LED nháy nhanh, sau đó IWDG reset và khôi phục nháy bình thường.
- [ ] PB13: LED đứng yên, sau đó IWDG reset và khôi phục.
- [ ] PB14: xác nhận vào `HardFault_Handler` và xem các thanh ghi lỗi; sau đó chạy thử IWDG tự phục hồi không halt.
- [ ] PB15: reset mềm và hai chớp ngắn lúc khởi động; phân biệt với ba chớp sau IWDG.
- [ ] Giữ mỗi nút qua lần reset: khi khởi động lại, hệ thống tiếp tục chạy bình thường, không reset lặp. Thả ổn định rồi nhấn lại mới tạo lần thử tiếp theo.
- [ ] Giữ nút ngay từ lúc cấp nguồn: không kích hoạt lỗi cho đến khi đã thả ổn định rồi nhấn lại.
- [ ] Bấm nhanh/dội không đủ ba mẫu liên tiếp: không kích hoạt. Thử cả trạng thái thả bị dội.
- [ ] Nhấn nhiều nút cùng lúc: khi cùng đạt ngưỡng một lượt quét, kết quả theo ưu tiên PB12 → PB13 → PB14 → PB15; nút sau không ghi đè lỗi đã chốt.
- [ ] Kiểm tra tăng của `scan_count` hoặc `Fault_GetScanCount()`: khoảng 100 lượt mỗi giây khi chạy bình thường, không tính thời gian debugger halt.
- [ ] Nếu thử dừng ngắt TIM2 có chủ đích: watchdog phải ngừng được làm mới khi không có lượt quét mới và cuối cùng reset.
- [ ] Ghi thời gian phục hồi thực tế, điều kiện nguồn, cách đo, trạng thái debugger; đối chiếu sai số LSI thay vì yêu cầu đúng tuyệt đối 3,2768 s.

## 8. Ghép với bài của nhóm và sinh mã lại

Phần sinh viên 2 bàn giao module nút/gây lỗi. Gọi `Fault_Init()` trước khi bật timer quét; gọi `Fault_Scan_10ms()` từ callback timer 10 ms; gọi `Fault_Process()` từ vòng lặp chính. ISR chỉ lấy mẫu và chốt cờ, không chạy vòng treo, không reset mềm và không làm mới IWDG. Có thể dùng `Fault_GetActive()` cùng `Fault_GetScanCount()` để cung cấp trạng thái cho bộ giám sát.

Phạm vi chương trình demo được giới hạn theo phân công:

| Thành viên | Phần cần ghép trong bản hoàn chỉnh của nhóm |
| --- | --- |
| Sinh viên 1 | Chiến lược IWDG, kiểm tra sức khỏe/tiến độ các tác vụ và quyết định làm mới watchdog. Điều kiện kiểm tra main + quét nút của demo này chỉ là hỗ trợ chạy độc lập. |
| Sinh viên 2 | Ba lỗi bắt buộc qua nút: vòng lặp treo, chờ cảm biến vô hạn, HardFault do truy cập địa chỉ sai. PB15 là phép thử phụ. |
| Sinh viên 3 | Phân loại/đếm reset, lưu bằng backup register và hiển thị LCD. Các ảnh chụp RAM và LED của demo chưa thay thế phần này. |
| Sinh viên 4 | Cảm biến thật, LED/UART và phần WWDG theo phân công. Demo này chưa tích hợp các chức năng đó. |

Khi ghép, chỉ để bộ giám sát của sinh viên 1 làm mới IWDG sau khi đủ điều kiện của toàn nhóm; loại bỏ hoặc thay thế đường làm mới dùng riêng cho demo trong `main.c`. Ghép nội dung vào callback timer chung nếu nhóm đã có callback, tránh định nghĩa trùng. Sinh viên 3 nên dùng một điểm duy nhất chụp rồi xóa cờ reset; nếu một module xóa cờ trước, module khác phải dùng ảnh chụp thay vì đọc lại phần cứng.

Mã tùy chỉnh trong các tệp do CubeMX tạo được đặt trong vùng `USER CODE`; cấu hình TIM2 và IWDG trong `.ioc` đã được đồng bộ với mã C. CubeMX bật Keep User Code và backup, nhưng sau mỗi lần Generate Code vẫn cần soát `main.c`, callback timer, handler lỗi và các tham chiếu trong Keil. Nếu module tự viết không còn trong danh sách build, thêm lại `Core/Src/fault_injection.c` vào nhóm Application/User/Core. Chỉ mở và sinh mã từ `.ioc` nằm trong thư mục bản sửa.
