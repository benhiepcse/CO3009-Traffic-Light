# Hợp đồng giao diện firmware CO3009

Trạng thái: Bản nháp để Nghĩa Hiệp, Thế Thông và Nhân Nguyễn review.

## 1. Đơn vị thời gian

Một tick của hệ thống tương ứng 10 ms. Timer phần cứng tạo ngắt mỗi
10 ms và ngắt gọi `SCH_Update()`. Vòng lặp chính gọi
`SCH_Dispatch_Tasks()` để chạy các tác vụ đến hạn.

- `SCH_Add_Task()` nhận thời gian chờ và chu kỳ theo tick.
- Bộ định thời phần mềm cũng nhận thời lượng theo tick.
- Tham số nhận tick dùng hậu tố `_ticks`. Giá trị tính bằng mili giây
  dùng hậu tố `_ms`.
- Khi đổi thời lượng từ ms sang tick, làm tròn lên nếu phép chia có dư.
  Ví dụ 25 ms được đổi thành 3 tick, tương ứng 30 ms.
- Giá trị chu kỳ bằng 0 trong scheduler biểu thị tác vụ chạy một lần
  (one-shot).

| Thời gian | Số tick |
|---|---:|
| 10 ms | 1 |
| 500 ms | 50 |
| 1 giây | 100 |
| 3 giây | 300 |

Hiệp dùng quy ước này khi viết scheduler và bộ định thời phần mềm.
Thông đối chiếu chu kỳ ngắt 10 ms với cấu hình timer trên board.
Nhân đổi thời lượng các pha đèn sang tick trước khi đặt timer cho FSM.

## 2. Bộ lập lịch Scheduler

- Gọi `SCH_Init()` trước khi đăng ký tác vụ và trước khi bật ngắt timer.
- `SCH_Add_Task(task, delay_ticks, period_ticks)` đăng ký một callback.
  Hàm trả ID khác 0 nếu thành công và trả 0 nếu thất bại,
  ví dụ khi callback là NULL hoặc hàng đợi đã đầy.
- `delay_ticks` là số tick chờ trước lần chạy đầu tiên.
  Nếu bằng 0, tác vụ đủ điều kiện chạy ngay và callback được gọi khi dispatch.
- `period_ticks` là chu kỳ chạy lại. Nếu bằng 0, tác vụ chạy một lần
  rồi tự được loại khỏi scheduler.
- ID của tác vụ định kỳ giữ nguyên giữa các lần chạy.
- `SCH_Delete_Task(task_id)` trả 1 nếu xóa thành công và trả 0 nếu ID
  không hợp lệ hoặc tác vụ đã hết hiệu lực.
- Ngắt timer gọi `SCH_Update()`. Hàm chỉ xử lý đầu hàng đợi để có
  độ phức tạp O(1).
- Vòng lặp chính gọi `SCH_Dispatch_Tasks()` để thực thi callback đến hạn.
  Thao tác thêm và xóa tác vụ được gọi từ luồng chính.

## 3. Bộ định thời phần mềm

- Gọi `sw_timer_init()` trước khi sử dụng timer.
- Scheduler gọi `sw_timer_tick()` qua một tác vụ định kỳ mỗi 1 tick (10 ms).
  Mỗi lần gọi cập nhật các bộ đếm đang chạy thêm một tick.
- `SW_TIMER_TRAFFIC_PHASE` là ID của timer dùng để đếm thời lượng pha đèn.
- `sw_timer_start(id, duration_ticks)` đặt hoặc đặt lại timer và xóa cờ
  hết hạn cũ. Thời lượng phải lớn hơn 0.
- Khi bộ đếm về 0, timer dừng và đặt cờ hết hạn.
- `sw_timer_take_timeout(id)` trả `true` và xóa cờ nếu timer đã hết hạn.
  Nếu chưa có cờ, hàm trả `false`. FSM dùng kết quả này để chuyển pha.
- `sw_timer_cancel(id)` dừng timer và xóa cờ hết hạn, kể cả khi timer đã
  dừng. FSM có thể gọi hàm này khi rời một chế độ.
- Với ID không hợp lệ, các hàm trả `false` và giữ nguyên trạng thái.
  `sw_timer_start()` cũng trả `false` nếu thời lượng bằng 0.
- Các hàm của module được gọi từ luồng chính. Ngắt timer phần cứng
  gọi `SCH_Update()`, còn `sw_timer_tick()` chạy qua scheduler dispatch.

## 4. Nút nhấn

- `button_init()` khởi tạo trạng thái chống dội và làm rỗng hàng đợi sự kiện.
- Scheduler gọi `button_poll()` mỗi 1 tick (10 ms) để lấy mẫu và chống dội.
- ID nút được đặt theo vị trí nối dây: D7, D8, D9, D10 và USER trên PC13.
  Mức logic khi nhấn được xác định từ kết quả đo ở CO3009-04.
- Khi nút chuyển sang trạng thái nhấn ổn định, driver đưa một sự kiện
  `BUTTON_EVENT_PRESS` vào hàng đợi. Giữ nút không phát thêm PRESS.
  Sau khi nút được thả ổn định, lần nhấn tiếp theo có thể phát PRESS mới.
- Nếu thiết kế FSM dùng nhấn giữ, driver phát thêm đúng một
  `BUTTON_EVENT_LONG_PRESS` khi vượt ngưỡng giữ. Khi đó cùng một lượt nhấn
  có thể tạo PRESS trước và LONG_PRESS sau. Ngưỡng giữ và cách mode xử lý
  hai sự kiện này do nhóm chốt khi review.
- `button_take_event(out)` lấy và xóa sự kiện cũ nhất, ghi vào `*out`
  rồi trả `true`. Nếu hàng đợi rỗng hoặc `out` là NULL, trả `false`
  và giữ nguyên trạng thái.
- Khi hàng đợi đầy, bỏ sự kiện mới và giữ các sự kiện đang chờ.
- FSM cấp cao là nơi lấy sự kiện và chuyển cho mode đang hoạt động.
  Khi cần bỏ các sự kiện còn chờ lúc chuyển mode, gọi `button_clear_events()`.
  Hàm này chỉ làm rỗng hàng đợi, giữ trạng thái chống dội và trạng thái giữ nút.
- Các hàm trên chạy từ luồng chính thông qua scheduler hoặc FSM.

## 5. Đèn giao thông

- Đường 1 dùng module nối D3–D4. Đường 2 dùng module nối D5–D6.
- Màu đèn được biểu diễn bằng `TRAFFIC_OFF`, `TRAFFIC_RED`,
  `TRAFFIC_AMBER` và `TRAFFIC_GREEN`. Driver dùng bảng đo ở CO3009-04
  để chuyển từng màu thành tổ hợp tín hiệu GPIO.
- Gọi `traffic_init()` sau khi GPIO đã được cấu hình và trước khi
  khởi chạy FSM. Trạng thái đầu ra sau khi gọi là đỏ–đỏ.
- `traffic_set_pair(road1, road2)` nhận màu đường 1 trước,
  màu đường 2 sau. Hàm trả `true` khi đã áp dụng cặp màu.
- Nếu có màu ngoài tập giá trị hợp lệ hoặc cả hai đường được yêu cầu
  bật xanh, hàm trả `false` trước khi ghi GPIO và giữ nguyên cặp màu cũ.
- Khi chuyển quyền đi giữa hai đường, driver phải tắt xanh ở đường
  đang xanh trước khi bật xanh ở đường còn lại. Cần kiểm tra cả
  tổ hợp tín hiệu trung gian nếu hai chân GPIO được ghi lần lượt.
- Các hàm điều khiển đèn chạy từ luồng chính. FSM gọi API này
  để điều khiển đèn trong các chế độ.

## 6. Cảm biến DHT20

- Gọi `dht20_init()` sau khi I²C đã được cấu hình. Trạng thái ban đầu
  là `DHT20_NO_SAMPLE`.
- Scheduler gọi `dht20_poll()` từ luồng chính để xử lý quá trình đọc.
  Driver quản lý chu kỳ đo và thời gian chờ giữa các bước, tránh chờ
  dài trong callback. Chu kỳ đo sẽ được chốt khi triển khai CO3009-16.
- `dht20_get_latest(out)` đọc trạng thái và mẫu đã lưu. Hàm không
  thực hiện giao dịch I²C.
- Trả `DHT20_OK` nếu lần đọc hoàn tất gần nhất thành công. Khi đó
  hàm sao chép mẫu vào `*out`.
- Trả `DHT20_NO_SAMPLE` nếu chưa có lần đọc nào hoàn tất.
  Trả `DHT20_ERROR` nếu lần đọc hoàn tất gần nhất thất bại hoặc
  `out` là NULL. Trong cả hai trường hợp, hàm không ghi vào `*out`.
- Nhiệt độ và độ ẩm được lưu theo đơn vị một phần mười.
  Ví dụ `temperature_x10 = 253` là 25,3 °C và
  `humidity_x10 = 654` là 65,4 %RH.

## 7. Màn hình LCD

- Gọi `lcd_init()` sau khi I²C đã được cấu hình. Hàm trả `true`
  nếu khởi tạo thành công và `false` nếu khởi tạo thất bại.
- `lcd_write_line(row, text)` ghi một dòng. Dòng đầu có chỉ số 0.
  Số dòng, số cột và địa chỉ I²C lấy từ LCD thực tế của nhóm.
- `text` là chuỗi C kết thúc bằng ký tự null. Nếu chuỗi ngắn hơn
  chiều rộng LCD, driver đệm dấu cách. Nếu dài hơn, driver chỉ ghi
  phần vừa với chiều rộng. Chuỗi rỗng dùng để xóa một dòng.
- Hàm trả `false` nếu chưa khởi tạo thành công, chỉ số dòng không
  hợp lệ hoặc `text` là NULL. Với các trường hợp này, driver
  kiểm tra đầu vào trước khi ghi. Nếu giao dịch I²C lỗi trong lúc
  ghi, hàm cũng trả `false` và màn hình có thể đã thay đổi một phần.
- Các hàm LCD chạy từ luồng chính. Lớp ứng dụng quyết định nội dung
  hiển thị. Trong chế độ chỉnh thời lượng, tác vụ hiển thị môi
  trường không ghi đè nội dung chỉnh đang hiển thị.

  ## 8. Điểm gọi máy trạng thái

- Gọi `fsm_init()` sau khi khởi tạo các module mà FSM sử dụng
  và trước khi đăng ký tác vụ `fsm_run()`.
- Scheduler gọi `fsm_run()` định kỳ từ vòng lặp chính.
  Chu kỳ cụ thể sẽ được chốt trong bảng tác vụ khi tích hợp.
- FSM cấp cao lấy sự kiện nút và chuyển cho chế độ đang hoạt động.
  Khi đổi chế độ, FSM xử lý sự kiện chờ, timer và quyền hiển thị
  theo thiết kế chuyển chế độ.
- FSM dùng API module để đặt cặp màu đèn, đọc timeout và cập nhật LCD.
- Trạng thái khởi động, bảng chức năng nút, các pha đèn và điều kiện
  hợp lệ của thời lượng được xác định trong CO3009-06.

## 9. Kiểm tra giao diện trên máy tính

- `tests/host/mock_modules.c` cung cấp bản giả lập tối thiểu cho smoke test.
- `tests/host/api_smoke.c` kiểm tra cách gọi các API trong bảy header.
- Mock dùng cấu hình kiểm thử 2 dòng LCD, mỗi dòng 16 ký tự,
  mẫu DHT20 có thể thay đổi và hàng đợi tối đa 8 sự kiện nút.
- FSM trong mock chỉ minh họa luồng D7 -> đặt cặp màu và được scheduler
  gọi làm one-shot. Thiết kế chế độ thật được bàn giao ở các task FSM.
- Khi build firmware STM32, chỉ liên kết phần triển khai thật.
  Thư mục `tests/host/` dành cho chương trình kiểm tra trên máy tính.
- Ba thành viên review giao diện trước khi merge vào `dev`.

Sau khi tích hợp mã 13–15, chạy `tests/host/test.ps1` để kiểm tra toàn bộ
FSM thật và smoke test API. Các build dùng `MOCK_MODULES_USE_REAL_FSM` bỏ
hai hàm FSM giả trong `mock_modules.c` và liên kết các file thật trong
`Core/Src`. Xem [tài liệu Sprint 3](sprint-03/co3009-13-15-fsm.md) cho cấu
hình thời lượng dùng chung, thao tác nút và luồng chuyển mode.
