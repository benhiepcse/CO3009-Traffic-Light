# CO3009-23 Báo cáo triển khai phần cứng và module

## 3.1 Cấu hình phần cứng

Hệ thống đèn giao thông dùng kit XNUCLEO-F103RB với STM32F103RB. Project
`BTL.ioc` chọn board NUCLEO-F103RB trong STM32CubeIDE và giữ Serial Wire
để nạp, gỡ lỗi. Hai module đèn nhận mã A/B qua GPIO. Bốn nút trên board
mở rộng và nút USER được đọc qua GPIO. DHT20 và LCD dùng chung I²C1
ở 100 kHz trên PB9/PB8. UART kiểm thử dùng USART2 115200 baud, 8N1
qua cổng COM của ST-LINK.

| Đầu cắm | Chân STM32 | Nhãn hoặc chức năng | Cấu hình |
| --- | --- | --- | --- |
| D3, D4 | PB3, PB5 | `TL1_A`, `TL1_B` | Output push-pull |
| D5, D6 | PB4, PB10 | `TL2_A`, `TL2_B` | Output push-pull |
| D7, D8 | PA8, PA9 | `BTN_D7`, `BTN_D8` | Input, no pull |
| D9, D10 | PC7, PB6 | `BTN_D9`, `BTN_D10` | Input, no pull |
| USER | PC13 | `BTN_USER` | Input, no pull |
| D14, D15 | PB9, PB8 | `I2C1_SDA`, `I2C1_SCL` | Alternate function open-drain, remap I²C1 |
| D13 | PA5 | `LD2` | Output push-pull |

Cấu hình đầu ra đèn sau đây là phần trích từ `MX_GPIO_Init()`.
Serial Wire giữ PA13/PA14 cho debug và giải phóng các chân JTAG
được sử dụng làm GPIO trong project.

```c
GPIO_InitStruct.Pin = TL2_B_Pin | TL1_A_Pin | TL2_A_Pin | TL1_B_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
```

TIM2 nhận clock 64 MHz sau khi nhân đôi clock APB1 32 MHz. Với
`PSC = 6399` và `ARR = 99`, chu kỳ ngắt tính theo cấu hình là
`(6399 + 1) × (99 + 1) / 64 000 000 = 0,01 s`. Đây là chu kỳ thiết
kế để tiếp nối phần 3.2 của báo cáo.

## 3.3.2 Nút nhấn và chống dội

`button_poll()` lấy mẫu D7, D8, D9, D10 và USER mỗi 10 ms. Driver
đổi mức GPIO thành trạng thái nhấn theo cấu hình từng nút. Trạng thái
mới được chấp nhận sau ba mẫu liên tiếp bằng nhau. Khi chuyển sang
nhấn ổn định, driver thêm một `PRESS` vào hàng đợi. Sau 100 mẫu
nhấn tiếp theo, driver thêm tối đa một `LONG_PRESS`. Nhả ổn định
đặt lại bộ đếm để lần nhấn kế tiếp có sự kiện mới.

`button_init()` đặt lại trạng thái và hàng đợi. `button_take_event()`
lấy sự kiện theo thứ tự FIFO. Hàng đợi có tám chỗ và bỏ sự kiện mới
khi đầy. `button_clear_events()` chỉ xóa sự kiện chờ, giữ trạng thái
chống dội để lần đổi mode không tạo thêm một lần nhấn từ nút đang giữ.
Cấu hình tạm nhận D7–D10 ở mức cao và USER ở mức thấp.

## 3.3.3 Driver đèn hai đường

`traffic_init()` đặt hai đường đỏ. `traffic_set_pair(road1, road2)`
kiểm tra cả cặp màu trước khi ghi và từ chối giá trị sai hoặc hai
đường cùng xanh. Khi chuyển quyền đi, driver bỏ xanh ở đường đang đi
trước khi bật xanh đường đối diện. Hai chân A/B của một module trên
GPIOB đổi bằng một lần ghi BSRR để tránh mã trung gian do ghi từng
chân. FSM quản lý thời lượng vàng và khoảng hai đỏ.

| Màu logic | Mã A/B tạm đường 1 | Mã A/B tạm đường 2 |
| --- | --- | --- |
| Tắt | 00 | 00 |
| Đỏ | 01 | 01 |
| Vàng | 11 | 11 |
| Xanh | 10 | 10 |

`hardware_profile.h` lưu bảng mã riêng cho từng đường. Test GPIO
giả lập kiểm tra 225 chuyển đổi giữa các cặp màu hợp lệ, cả hai
chiều chuyển xanh và việc từ chối cặp hai xanh.

## 3.3.4 Cảm biến DHT20 và màn hình LCD

DHT20 dùng địa chỉ 7 bit `0x38`, tương ứng `0x70` khi truyền vào
STM32 HAL. Sau bước khởi tạo, driver gửi `AC 33 00` khoảng mỗi
2 giây. Driver chia việc đo thành các lần gọi `dht20_poll()`, chờ
80 ms bằng mốc thời gian giữa các lần gọi và thực hiện tối đa một
giao dịch I²C mỗi lần. Khung 7 byte chứa trạng thái, dữ liệu và CRC.
Driver kiểm tra cờ bận, trạng thái hiệu chuẩn/OTP và CRC-8 trước khi
lưu kết quả. Khi quá 200 ms hoặc giao dịch gặp lỗi, driver báo
`DHT20_ERROR` và thử lại ở chu kỳ sau.

Với `b[0]` là byte trạng thái, hai giá trị 20 bit được tách và
quy đổi theo datasheet Aosong.

```text
RHraw = (b[1] << 12) | (b[2] << 4) | (b[3] >> 4)
Traw  = ((b[3] & 0x0F) << 16) | (b[4] << 8) | b[5]
RH    = 100 × RHraw / 2^20       (%RH)
T     = 200 × Traw / 2^20 - 50  (°C)
```

`dht20_get_latest()` đọc mẫu đã lưu dạng phần mười. Khung kiểm thử
`18 80 00 06 00 00 23` cho 25,0 °C và 50,0 %RH.

Driver LCD hướng tới màn hình 16×2, bộ điều khiển HD44780 và backpack
PCF8574(A). Firmware tự dò địa chỉ trên bus LCD/DHT20. Khởi tạo và
xóa thông qua `lcd_init()` và `lcd_clear()`. Các hàm
`lcd_set_cursor()`, `lcd_print()` và `lcd_print_int()` cung cấp
thao tác con trỏ, in chuỗi và số. `lcd_write_line()` cắt nội dung dài
và đệm khoảng trắng cho chuỗi ngắn để xóa ký tự cũ. Khi tích hợp,
FSM chọn nội dung LCD. Auto/Manual dùng mẫu DHT20 đã lưu và Tuning
giữ phần hiển thị thời lượng đang chỉnh.

## Dữ liệu cần điền trước khi ghép Word

- Đối chiếu kết quả CO3009-20, thay bảng mã màu và mức nhấn tạm bằng
  số đo của từng module. Ghi địa chỉ I²C đã quét, loại LCD thực tế,
  commit đo, chu kỳ cập nhật và kết quả ca lỗi ngoại vi.
- Chèn ảnh board đã đấu đủ dây và ảnh LCD khi chạy firmware tích hợp.
  Bổ sung chú thích chỉ rõ vị trí module và commit đã dùng để chụp.
- Chép bốn mục vào `Template_Report_BTL_CO3009.docx` của nhóm.
  Mục 3.2, 3.3.1 và phần FSM dùng nội dung bàn giao của các thành viên
  tương ứng.

### Nguồn đối chiếu

- [BTL.ioc và cấu hình chân](https://github.com/benhiepcse/CO3009-Traffic-Light/tree/e6ac768)
- [Driver nút và đèn](https://github.com/benhiepcse/CO3009-Traffic-Light/blob/e6ac768/docs/co3009-10-11-drivers.md)
- [Driver DHT20 và LCD](https://github.com/benhiepcse/CO3009-Traffic-Light/blob/5faa024/docs/co3009-16-17-environment.md)
- [Bảng nghiệm thu CO3009-20](co3009-20-hardware-acceptance.md)
