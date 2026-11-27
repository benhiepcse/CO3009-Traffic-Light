# CO3009-16/17 · DHT20 và LCD

Hai demo dùng I2C1 (100 kHz) và scheduler 10 ms hiện có. Mỗi demo có cấu
hình build riêng. Firmware mặc định, FSM và lịch tác vụ CO3009-18 giữ nguyên.

## CO3009-16 · Đọc nhiệt độ và độ ẩm

- Địa chỉ 7-bit của DHT20 là `0x38`. Driver truyền `0x70` cho STM32 HAL
  (`0x38 << 1`). HAL chọn bit đọc hoặc ghi.
- Gọi `dht20_init()` sau khi khởi tạo I2C1 rồi gọi `dht20_poll()` khoảng
  mỗi 10 ms. Lần đầu chờ 100 ms, sau đó gửi `AC 33 00` khoảng mỗi 2 giây.
- Sau lệnh đo, chờ ít nhất 80 ms bằng cách kiểm tra thời gian và trả về.
  Mỗi lần poll thực hiện tối đa một giao dịch I²C với timeout 3 ms.
  Driver đọc khung 7 byte gồm trạng thái, 5 byte dữ liệu và CRC.
- Driver bỏ khung còn bận, thử đọc lại ở tick kế tiếp và báo lỗi nếu
  lần đo đạt 200 ms mà chưa sẵn sàng. Giao dịch lỗi, trạng thái hiệu
  chuẩn/OTP sai hoặc CRC sai cũng kết thúc lần đo với lỗi. Driver thử
  lại ở chu kỳ kế tiếp. CRC-8 dùng giá trị đầu `0xFF`, đa thức `0x31`.
- `dht20_get_latest()` chỉ chép mẫu khi trả `DHT20_OK`. Hai trạng thái
  còn lại là `DHT20_NO_SAMPLE` và `DHT20_ERROR`.

Với `b[0]` là byte trạng thái, tách và quy đổi số đo như sau.

```text
RHraw = (b[1] << 12) | (b[2] << 4) | (b[3] >> 4)
Traw  = ((b[3] & 0x0F) << 16) | (b[4] << 8) | b[5]
RH    = RHraw * 100 / 2^20     (%RH)
T     = Traw  * 200 / 2^20 - 50 (°C)
```

API lưu kết quả theo phần mười và làm tròn về giá trị gần nhất. Khung
`18 80 00 06 00 00 23` cho 25,0 °C và 50,0 %RH. Trình tự và công thức
theo [datasheet DHT20 v1.3 của Aosong](https://www.aosong.com/userfiles/files/media/DHT20%E4%BA%A7%E5%93%81%E8%A7%84%E6%A0%BC%E4%B9%A6%28%E4%B8%AD%E6%96%87%E7%89%88%29%20A3-202409.pdf).

## CO3009-17 · API LCD và quyền hiển thị

Driver dùng giả định LCD 16×2 tương thích HD44780 với backpack PCF8574
hoặc PCF8574A. Sơ đồ chân backpack được giả định là P0=RS, P1=R/W,
P2=E, P3=đèn nền, P4–P7=D4–D7. Trên bus chỉ có LCD và DHT20, driver
thăm dò địa chỉ 7-bit `0x20–0x27` và `0x39–0x3F`, dành `0x38` cho DHT20.
Driver chỉ gửi dữ liệu khi đúng một địa chỉ trong dải trả ACK. Nếu dùng
chung bus với module khác, đặt `CO3009_LCD_ADDRESS` theo địa chỉ LCD đã đo.
Nếu module có bộ điều khiển hoặc sơ đồ khác, cần dùng đúng driver cho
module đó. Xem [PCF8574 của TI](https://www.ti.com/lit/ds/symlink/pcf8574.pdf)
và [HD44780U của Hitachi](https://cdn.sparkfun.com/assets/9/5/f/7/b/HD44780.pdf).

| API | Hành vi |
| --- | --- |
| `lcd_init()` | Khởi tạo ở startup sau I2C1 và trước khi chạy scheduler |
| `lcd_poll_ready()` | Tác vụ hiển thị gọi định kỳ. Mỗi lần thực hiện tối đa một bước phục hồi và trả `true` khi có thể ghi dòng |
| `lcd_clear()` | Xóa màn hình và đưa con trỏ về đầu |
| `lcd_set_cursor(row, column)` | Đặt vị trí, hàng và cột đều bắt đầu từ 0 |
| `lcd_print(text)`, `lcd_print_int(value)` | In chuỗi hoặc số nguyên có dấu từ con trỏ, tối đa hết hàng |
| `lcd_write_line(row, text)` | Ghi một hàng, cắt chuỗi dài và đệm khoảng trắng cho chuỗi ngắn |

Một dòng dùng một giao dịch 70 byte, khoảng 6,4 ms trên bus 100 kHz
chưa tính overhead. Demo chỉ ghi một dòng mỗi tick. Hàm trả `false` nếu
lỗi I²C. Sau lỗi khởi tạo hoặc truyền, driver chờ ít nhất 500 ms rồi
thăm dò và đồng bộ lại giao tiếp 4 bit qua các lần gọi `lcd_poll_ready()`.
Sau khi phục hồi, FSM ghi lại cả hai dòng, mỗi lần cập nhật một dòng.

Khi ghép hệ thống, FSM tiếp tục là nơi quyết định nội dung LCD.
`dht20_poll()` chỉ cập nhật mẫu và `lcd.c` chỉ ghi khi được gọi.
Nhánh Tuning giữ quyền hiển thị giá trị chỉnh. Nếu cần cập nhật cả hai
dòng, lúc tích hợp cần đo thời gian hoặc tách lần ghi theo tick.

## Chạy demo và nghiệm thu

Bật một symbol trong **STM32CubeIDE → Project Properties → C/C++ Build →
Settings → MCU GCC Compiler → Preprocessor**, build rồi nạp khi có board.
USART2 dùng 115200 baud, 8N1.

| Symbol | Hành vi | Trên board |
| --- | --- | --- |
| `CO3009_DHT20_DEMO` | In nhiệt độ và độ ẩm lên UART mỗi khoảng 2 giây. Xác nhận số đo và thử ngắt cảm biến để thấy báo lỗi. | Chờ board |
| `CO3009_LCD_DEMO` | Hiện nhiệt độ, độ ẩm giả, rồi màn hình `TUNE GREEN` với `60 seconds` và `5 s`. Quan sát `5 s` không còn ký tự của chuỗi cũ và thông số môi trường chỉ trở lại sau khi rời Tuning. | Chờ xác nhận địa chỉ và module LCD |

## Kiểm thử trên máy

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/test.ps1 -Target dht20_test
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/test.ps1 -Target lcd_test
```

```sh
gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -ICore/Inc -DDHT20_HOST_TEST Core/Src/dht20.c tests/host/dht20_test.c -o /tmp/dht20_test && /tmp/dht20_test
gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -ICore/Inc -DLCD_HOST_TEST -DCO3009_LCD_ADDRESS=0x3F Core/Src/lcd.c tests/host/lcd_test.c -o /tmp/lcd_test && /tmp/lcd_test
```

Hai test trên máy đã qua. Bản mặc định, hai demo mới và các cấu hình
firmware có sẵn đã build và link bằng toolchain ARM của STM32CubeIDE.
