# CO3009-20 Nghiệm thu phần cứng và lỗi ngoại vi

Dùng phiếu này để đo riêng từng module rồi nghiệm thu đèn, nút, DHT20 và
LCD trên cùng một board. Các kết quả bên dưới đang chờ đo. Kiểm thử
giả lập trên máy và build firmware được ghi riêng.

## Phiên chạy

| Thông tin cần ghi | Giá trị |
| --- | --- |
| Ngày, người thực hiện và mã board/kit | Chờ chạy |
| Phiên bản CO3009-19, commit firmware và cấu hình build | Chờ chạy |
| Loại module đèn, loại LCD và địa chỉ I²C thực tế | Chờ xác nhận |
| Thiết bị ghi UART, timestamp hoặc phân tích logic | Chờ xác nhận |
| Thư mục ảnh, log và dữ liệu đo | Chờ ghi |

## Xác nhận dây và cấu hình

1. Tắt nguồn. Dùng [sơ đồ cắm dây](../sprint-05/co3009-28-32-demo.md#sơ-đồ-cắm-dây)
   để đối chiếu từng đầu cắm, chiều giắc, nguồn/GND và hai đường SDA/SCL.
2. Chạy `CO3009_HW_BRINGUP` để quét I²C 7 bit, đọc mức nút và lần lượt
   thử bốn mã đèn. Mở serial USART2 115200 baud, 8N1 trước khi reset.
3. So sánh số đo với `hardware_profile.h` và `CO3009_LCD_ADDRESS`.
   Ghi mọi sai khác trước khi nạp bản chạy đồng thời. DHT20 có địa chỉ
   theo datasheet là `0x38`. `0x27` chỉ là địa chỉ LCD tạm trong code.

### Bảng đo mã màu

| A B | Màu theo cấu hình tạm | Đường 1 quan sát | Đường 2 quan sát |
| --- | --- | --- | --- |
| 00 | Tắt | Chờ đo | Chờ đo |
| 01 | Đỏ | Chờ đo | Chờ đo |
| 10 | Xanh | Chờ đo | Chờ đo |
| 11 | Vàng | Chờ đo | Chờ đo |

### Bảng đo nút và I²C

| Nút | Chân MCU | Mức nhả | Mức nhấn |
| --- | --- | --- | --- |
| D7 | PA8 | Chờ đo | Chờ đo |
| D8 | PA9 | Chờ đo | Chờ đo |
| D9 | PC7 | Chờ đo | Chờ đo |
| D10 | PB6 | Chờ đo | Chờ đo |
| USER | PC13 | Chờ đo | Chờ đo |

| Thiết bị | Địa chỉ 7 bit dự kiến | Địa chỉ quét được |
| --- | --- | --- |
| DHT20 | `0x38` | Chờ đo |
| LCD | Theo backpack, code tạm `0x27` | Chờ đo |

## Kiểm thử riêng rồi chạy đồng thời

Chọn đúng một cấu hình demo ở mỗi lần build. Với chuyển xanh, quan sát
cả hai hướng. Nếu cần xác nhận xung thoáng qua, ghi đồng thời hai cặp
A/B bằng bộ phân tích logic rồi giải mã theo bảng màu thực đo.

| Ca | Cấu hình và thao tác | Kỳ vọng | Kết quả và bằng chứng |
| --- | --- | --- | --- |
| HW20-01 | `CO3009_HW_BRINGUP`. Quét bus, đo nút và đọc đủ bốn mã trên mỗi bộ đèn. | Ghi được pin/mức/các địa chỉ đầy đủ, mỗi module đèn có đủ bốn trạng thái. | Chờ board |
| HW20-02 | `CO3009_TRAFFIC_DEMO`. Xem bước 00–11, đặc biệt 03→04 và 04→05. | Đúng màu theo từng bước. Đường cũ mất xanh trước khi đường kia lên xanh. Bước 10 từ chối yêu cầu hai xanh. | Chờ board. Lưu log và trace nếu có. |
| HW20-03 | `CO3009_BUTTON_DEMO`. Nhấn từng nút, giữ, nhả và nhấn lại. | Một `PRESS` mỗi lần nhấn ổn định. Giữ đủ ngưỡng sinh tối đa một `LONG_PRESS`. Nhấn lại sau khi nhả được nhận đúng. | Chờ board. Lưu log UART. |
| HW20-04 | `CO3009_DHT20_DEMO`. Ghi ít nhất ba mẫu nối tiếp, timestamp và tình trạng thiết bị. | UART in nhiệt độ, độ ẩm và cập nhật khoảng 2 giây trong demo. Đọc lại khi sang bản tích hợp. | Chờ board. Lưu giá trị thực và thời điểm đọc. |
| HW20-05 | `CO3009_LCD_DEMO`. Xem màn hình dữ liệu giả và bước đổi `60 seconds` thành `5 s`. | Đúng dòng và nội dung. Phần cuối chuỗi cũ được xóa. | Chờ board. Dán ảnh trước/sau. |
| HW20-06 | Bản tích hợp CO3009-19. Chạy đủ một vòng Auto, thử nút trong Manual, vào Tuning khi DHT20 tiếp tục lấy mẫu. | Đèn đúng pha, một lần nhấn hợp lệ được nhận đúng. Auto/Manual cập nhật môi trường theo lịch tích hợp. LCD giữ nội dung chỉnh trong Tuning. | Chờ CO3009-19 và board. Lưu video/log. |
| HW20-07 | Bản tích hợp. Tắt nguồn, tháo riêng DHT20 rồi cấp nguồn lại. Giữ LCD và các module khác. | Lỗi cảm biến được ghi nhận. Nút và đèn tiếp tục xử lý. Ghi cách LCD biểu thị lỗi thực tế. | Chờ CO3009-19 và board. Lưu log và thao tác nút/đèn. |
| HW20-08 | Bản tích hợp. Tắt nguồn, nối lại DHT20 rồi tháo riêng LCD. Cấp nguồn và lặp thao tác nút/đèn. | Lỗi LCD được ghi nhận. DHT20, nút và đèn tiếp tục xử lý. Dùng UART hoặc debugger để xác nhận trạng thái. | Chờ CO3009-19 và board. Lưu log và thao tác nút/đèn. |
| HW20-09 | Tắt nguồn, nối lại đủ thiết bị rồi chạy lại HW20-06 trên cùng commit. | Hệ thống phục hồi sau khi lắp lại. Các chức năng đã kiểm tra trở lại đúng kỳ vọng. | Chờ CO3009-19 và board. |

Ghi cho mỗi ca ngày chạy, commit, kết quả **Đạt** hoặc **Không đạt**,
giá trị quan sát và đường dẫn log/ảnh. Nếu xuất hiện lỗi, ghi cách tái
hiện, file/driver nghi ngờ và hash bản sửa trước khi chạy lại cùng ca.
Ảnh cần lấy gồm toàn bộ hệ thống đã đấu nối, LCD ở môi trường và LCD
ở Tuning. Đưa kết quả khớp commit vào [báo cáo CO3009-23](co3009-23-report-hardware.md).

## Bằng chứng đã có trên máy

Bốn target `traffic_light_test`, `button_test`, `dht20_test` và
`lcd_test` đã qua tại commit `5faa024`, dùng GPIO, thời gian và I²C
giả lập. Tài liệu kỹ thuật lưu riêng phần
[đèn và nút](https://github.com/benhiepcse/CO3009-Traffic-Light/blob/e6ac768/docs/co3009-10-11-drivers.md)
và [DHT20 và LCD](https://github.com/benhiepcse/CO3009-Traffic-Light/blob/5faa024/docs/co3009-16-17-environment.md).
