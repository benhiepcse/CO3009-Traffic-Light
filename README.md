# CO3009 — Hệ thống đèn giao thông hai đường

Bài tập lớn môn **Vi xử lý – Vi điều khiển (CO3009)**, học kỳ 1 năm học 2026–2027, Trường Đại học Bách khoa – ĐHQG TP.HCM.

Dự án xây dựng hệ thống điều khiển đèn giao thông tại giao lộ hai đường trên board **XNUCLEO-F103RB**, sử dụng vi điều khiển **STM32F103RB**. Firmware được phát triển bằng **STM32CubeIDE** và tổ chức theo các lớp phần cứng, module hỗ trợ, bộ lập lịch và máy trạng thái.

## 1. Chức năng dự kiến

| Chức năng | Mô tả |
|---|---|
| Tự động (Automatic) | Hai đường luân phiên được cấp quyền đi theo chu kỳ đèn đỏ, xanh và vàng đã cấu hình. |
| Thủ công (Manual) | Người dùng điều khiển chuyển trạng thái đèn bằng nút nhấn theo quy tắc của chế độ thủ công. |
| Chỉnh thời lượng (Timing adjustment) | Người dùng chọn màu đèn, thay đổi thời lượng và xác nhận qua nút nhấn. LCD hiển thị giá trị đang chỉnh. |
| Hiển thị môi trường | Đọc nhiệt độ và độ ẩm từ DHT20 theo chu kỳ và hiển thị lên LCD. |
| Lập lịch tác vụ | Hỗ trợ tác vụ định kỳ và tác vụ chạy một lần (one-shot). Hàm `SCH_Update` có độ phức tạp O(1). |

**Yêu cầu an toàn:** Hai đường không được cùng bật đèn xanh, kể cả khi chuyển chế độ. Bộ thời lượng mới phải qua kiểm tra tính hợp lệ trước khi áp dụng vào chu kỳ tự động.

## 2. Thành viên và phân công

| Thành viên | Tài khoản GitHub | Phụ trách |
|---|---|---|
| Nghĩa Hiệp | [benhiepcse](https://github.com/benhiepcse) | Thiết kế giao diện giữa các module, bộ lập lịch, bộ định thời phần mềm, tích hợp firmware và chốt phiên bản mã nguồn nộp. |
| Thế Thông | [thth0ng](https://github.com/thth0ng) | Khởi tạo project STM32CubeIDE, cấu hình phần cứng, triển khai module đèn, nút nhấn, DHT20 và LCD, kiểm thử trên board. |
| Nhân Nguyễn | [NguyenTrongNhan2006](https://github.com/NguyenTrongNhan2006) | Thiết kế và triển khai các máy trạng thái, xây dựng tiêu chí nghiệm thu, kiểm thử chuyển chế độ và tổng hợp báo cáo. |

Cả nhóm cùng review thay đổi, kiểm thử tích hợp và diễn tập demo. Mã công việc và trạng thái được theo dõi trong sheet `PROGRESS` của `BTL VXL.xlsx`.

## 3. Phần cứng

| Thiết bị | Kết nối theo đề |
|---|---|
| Board XNUCLEO-F103RB | Vi điều khiển STM32F103RB |
| Module đèn đường 1 | D3, D4 |
| Module đèn đường 2 | D5, D6 |
| Module nút nhấn 1 | D7, D8 |
| Module nút nhấn 2 | D9, D10 |
| Cảm biến DHT20 | Khe I²C trên board mở rộng |
| Màn hình LCD | Khe I²C trên board mở rộng |
| Nút USER trên board | PC13 |

Bảng ánh xạ chân Arduino sang chân STM32, mức logic khi nhấn, tổ hợp tín hiệu ứng với từng màu đèn và địa chỉ I²C sẽ được cập nhật theo cấu hình `.ioc` và kết quả kiểm thử phần cứng ở **CO3009-03** và **CO3009-04**.

## 4. Kiến trúc firmware

- **Cấu hình phần cứng** thiết lập GPIO, I²C và timer trong project STM32CubeIDE.
- **Module hỗ trợ** cung cấp API cho nút nhấn, đèn giao thông, bộ định thời phần mềm, DHT20 và LCD.
- **Bộ lập lịch** nhận tick từ ngắt timer qua `SCH_Update`. Vòng lặp chính gọi `SCH_Dispatch_Tasks` để thực thi các tác vụ đến hạn. Tác vụ one-shot có `Period = 0` và chỉ chạy một lần.
- **Máy trạng thái hữu hạn (FSM)** quản lý ba chế độ và việc chuyển chế độ. FSM gọi API của các module hỗ trợ để đọc nút, điều khiển đèn và cập nhật LCD.

[CO3009-12 — Chế độ Automatic](docs/sprint-02/co3009-12-automatic.md) mô tả các pha đèn, thời lượng và cách chạy kiểm thử trên máy tính.

Cấu trúc dự kiến sau khi tích hợp project STM32CubeIDE:

```text
CO3009-Traffic-Light/
├── Core/
│   ├── Inc/                # Header và giao diện các module
│   └── Src/                # Mã nguồn ứng dụng và module
├── Drivers/                # Thư viện STM32 HAL và CMSIS
├── <ten-project>.ioc       # Cấu hình STM32CubeMX
├── .gitignore
└── README.md
```

## 5. Tải mã nguồn, build và nạp chương trình

### Tải mã nguồn

Cài Git và STM32CubeIDE. Sau đó tải repo về máy:

```bash
git clone https://github.com/USERNAME_CUA_BAN/CO3009-Traffic-Light.git
cd CO3009-Traffic-Light
```

Nếu repo ở chế độ riêng tư, đăng nhập bằng tài khoản GitHub đã được cấp quyền. Khi nhóm công bố tag nộp, sử dụng tag đó để build đúng phiên bản dùng cho báo cáo và demo.

### Build và nạp sau khi project nền được bàn giao

1. Trong STM32CubeIDE, chọn **File → Open Projects from File System** và mở thư mục chứa project.
2. Kiểm tra cấu hình project dùng board **NUCLEO-F103RB** trong Board Selector và chọn **Serial Wire** ở **System Core → SYS → Debug**.
3. Chọn **Project → Build Project** và kiểm tra kết quả biên dịch.
4. Kết nối board với máy tính qua cổng **ST-LINK** và chọn **Run** để nạp firmware.
5. Kiểm tra hệ thống khởi động và phản hồi theo bảng thao tác của phiên bản đang chạy.

Hướng dẫn này sẽ được cập nhật với tên project thực tế và kiểm tra lại từ bản clone sạch ở **CO3009-27**.

## 6. Vận hành và kiểm thử

Bảng thao tác chính thức sẽ được điền sau khi nhóm chốt thiết kế FSM và kiểm tra trên board.

Xem [CO3009-06 — Thiết kế FSM Sprint 1](docs/sprint-01/co3009-06-fsm-design.md) để review phương án nút nhấn, các pha đèn và quy tắc chuyển chế độ.

| Nội dung | Thông tin cần cập nhật |
|---|---|
| Khởi động hoặc reset | Chế độ khởi động và màu đèn của cả hai đường |
| Chuyển chế độ | Nút hoặc sự kiện kích hoạt từng lần chuyển |
| Điều khiển thủ công | Chức năng của từng nút trong chế độ Manual |
| Chỉnh thời lượng | Nút chọn màu, tăng hoặc giảm, xác nhận và cách LCD báo giá trị sai |
| Hiển thị môi trường | Chu kỳ đọc DHT20 và cách LCD hiển thị nhiệt độ, độ ẩm |

Các nhóm ca kiểm thử dự kiến:

- Kiểm tra màu đèn, mức nhấn và địa chỉ I²C trên board thật.
- Kiểm tra task định kỳ, task one-shot và hành vi khi thêm hoặc xóa task.
- Kiểm tra đủ ba chế độ, bao gồm chuyển chế độ khi một đường đang xanh hoặc vàng.
- Kiểm tra bộ thời lượng hợp lệ và không hợp lệ, nút nhấn liên tiếp và hiển thị LCD.
- Kiểm tra chạy ổn định và xác nhận hai đường không cùng bật đèn xanh.

## 7. Quy ước làm việc trên GitHub

| Nhánh | Mục đích |
|---|---|
| `main` | Lưu phiên bản ổn định. Phiên bản nộp được đánh dấu bằng tag sau nghiệm thu. |
| `dev` | Tích hợp và kiểm tra thay đổi của cả nhóm. |
| `feature/<ma-task>-<ten-ngan>` | Nhánh riêng cho một nhiệm vụ, tạo từ `dev`. Ví dụ `feature/co3009-07-scheduler`. |

1. Cập nhật từ `dev` trước khi tạo nhánh cho nhiệm vụ mới.
2. Mỗi commit tập trung vào một thay đổi và mô tả rõ nội dung, ví dụ `scheduler: thêm hỗ trợ tác vụ one-shot`.
3. Mỗi thành viên commit bằng danh tính Git của mình, dùng email đã liên kết với tài khoản GitHub cá nhân.
4. Mở **pull request** vào `dev`. Trong mô tả, ghi mã công việc, phần đã sửa và cách kiểm tra.
5. Một thành viên khác review trước khi merge. Khi phiên bản trên `dev` đã qua nghiệm thu, nhóm tích hợp vào `main` và chốt tag nộp.
6. Giữ mã nguồn, header và cấu hình project trong repo. Các file build sinh ra được loại khỏi Git bằng `.gitignore`.

## 8. Phiên bản nộp và tài liệu

| Mục | Trạng thái |
|---|---|
| Nhánh nộp dự kiến | `main` |
| Tag và mã commit nộp | Cập nhật sau khi nghiệm thu bản chạy trên board |
| Báo cáo PDF | Soạn theo mẫu `Template_Report_BTL_CO3009.docx` |
| Tài liệu yêu cầu | `Project_Announcement_CO3009.pdf` |
| Nghiệm thu Sprint 1 | [CO3009-05 — Ma trận yêu cầu và kịch bản nghiệm thu](docs/sprint-01/co3009-05-acceptance.md) |
