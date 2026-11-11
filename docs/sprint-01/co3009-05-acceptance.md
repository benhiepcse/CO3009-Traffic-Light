# CO3009-05 Ma trận yêu cầu và kịch bản nghiệm thu

Phạm vi là BTL đèn giao thông hai đường trên XNUCLEO-F103RB. Ma trận nối **yêu cầu → module → ca kiểm thử → mục báo cáo**. Mỗi ca hiện ở trạng thái **Chưa chạy**. Cột mục báo cáo dùng số mục của mẫu Word.

Nguồn đối chiếu gồm `Project_Announcement_CO3009.pdf` (A2–A5, Step 2–7), `Template_Report_BTL_CO3009.docx`, bảng phân công `PROGRESS` và [hợp đồng API CO3009-02](https://github.com/benhiepcse/CO3009-Traffic-Light/blob/389a5ef905e79953719ea4065947de556686b79c/docs/api-contract.md). Ca kiểm thử có số cụ thể dùng cấu hình đề xuất của `CO3009-06`, sẽ cập nhật theo phương án nhóm duyệt.

## Ma trận yêu cầu

| Mã | Điều kiện nghiệm thu | Module hoặc sản phẩm | Ca kiểm thử | Mục Word |
|---|---|---|---|---|
| R01 | Project chọn đúng F103RB, Serial Wire, GPIO/I2C1 và timer tick 10 ms. Xác nhận bảng chân, mã màu, mức nút và địa chỉ I2C trên bộ kit. | `.ioc`, cấu hình board, driver đèn/nút/I2C | TC-HW-01, TC-HW-02 | 3.1, 3.2.3 |
| R02 | Automatic tự lặp đúng thứ tự pha và đúng thời lượng đã cấu hình. Reset đưa hệ thống về trạng thái khởi động đã thiết kế. | `fsm_automatic`, software timer, traffic light | TC-AUTO-01, TC-AUTO-02 | 2.1, 3.4.1, 3.5 |
| R03 | Manual đổi pha theo thao tác nút đã công bố. Một lần nhấn hợp lệ tạo một thao tác, kể cả khi nút dội hoặc bị giữ. | `fsm_manual`, button, software timer | TC-MAN-01, TC-MAN-02 | 2.1, 3.3.2, 3.4.2, 3.5 |
| R04 | Tuning chọn và chỉnh đỏ/xanh/vàng trên LCD. Giá trị sai bị từ chối, bộ hợp lệ được áp dụng khi về Automatic. | `fsm_tuning`, cấu hình dùng chung, LCD | TC-TUNE-01, TC-TUNE-02 | 2.3.3, 3.4.3, 3.5 |
| R05 | Chuyển đúng ba chế độ. Mỗi lần chuyển xác định màu hai đường, timer/task, bộ đếm, sự kiện nút và LCD. Hai đường không cùng xanh. | FSM chuyển mode, ba FSM, `global`, traffic light | TC-AUTO-01, TC-MAN-01, TC-MODE-01, TC-MODE-02, TC-MODE-03 | 2.2, 2.3, 3.4, 3.5 |
| R06 | `SCH_Update` được gọi từ ngắt timer và có số bước bị chặn bởi hằng số, không phụ thuộc số task. | `scheduler`, timer IRQ | TC-SCH-01 | 3.2.1–3.2.3 |
| R07 | Scheduler hỗ trợ Init/Add/Delete/Update/Dispatch. Task định kỳ chạy lại, one-shot chạy một lần, thêm/xóa không làm sai hạn của task còn lại. | `scheduler`, software timer | TC-SCH-02, TC-SCH-03 | 3.2.2, 3.2.3 |
| R08 | Đọc DHT20 định kỳ và hiển thị nhiệt độ, độ ẩm lên LCD theo chế độ. | DHT20, LCD, task môi trường | TC-ENV-01 | 1.2, 3.3.4 |
| R09 | FSM dùng API module. Ngắt cập nhật scheduler, vòng chính dispatch task. Nút, đèn, timer mềm và LCD có module riêng. | `fsm_*.c`, `main.c`, module hỗ trợ | TC-ARCH-01 | 1.3, 3.2.3, 3.3, 3.5 |
| R10 | Firmware chạy ổn định trên board và demo được đủ chức năng từ phiên bản nộp. | Firmware tích hợp, board, checklist demo | TC-REL-01 | 4.1, 5.2, 5.3 |
| R11 | Báo cáo theo mẫu, đúng thông tin đóng góp và commit/tag. Repo truy cập được, có hướng dẫn build/flash/vận hành. | PDF, repo GitHub, README | TC-REL-02 | Bìa, 4.3, 5 |

## Kịch bản chạy thử

`Mock` là cấp sự kiện và thời gian giả rồi ghi lại thứ tự lời gọi. Smoke test của CO3009-02 chỉ kiểm tra giao diện module. Khi hiện thực FSM, cần dùng FSM thật với mock có thể ghi cặp đèn, timer, LCD và phát nhiều sự kiện nút. `Board` là chạy firmware thật, ghi commit và phần cứng dùng để test. Các ca cần board vẫn để **Chưa chạy** nếu mới có kết quả mock.

| Mã ca | Môi trường và thao tác | Kỳ vọng và bằng chứng cần lưu |
|---|---|---|
| TC-HW-01 | **Board.** Kiểm tra `.ioc`, đọc clock timer, tính PSC/ARR. Nạp blink PA5, nạp lại rồi đo tick bằng tín hiệu kiểm tra hoặc trace. | Nạp lại được. Tick danh định 10 ms và phép đo phù hợp với cấu hình. Lưu ảnh `.ioc`, phép tính và số đo. |
| TC-HW-02 | **Board.** Thử `00`, `01`, `10`, `11` cho từng module đèn. Đọc mức khi nhấn/nhả bốn nút ngoài và USER/PC13, quét I2C. | Có bảng D3–D10 → chân MCU, bảng mã riêng từng đèn, mức của cả năm nút và địa chỉ hai thiết bị. Giữ bảng quan sát và log scan. |
| TC-AUTO-01 | **Mock + Board.** Chạy từ sau pha khởi động với `G=5 s, Y=2 s, R=9 s`. Ghi tối thiểu hai vòng. | Mỗi vòng: `G/R 5 s → Y/R 2 s → R/R 1 s → R/G 5 s → R/Y 2 s → R/R 1 s`. Chu kỳ danh định 16 s. Đối chiếu timestamp, màu và độ lệch theo tick/chu kỳ gọi FSM đã xác nhận. |
| TC-AUTO-02 | **Mock + Board.** Reset ở giữa pha xanh hoặc vàng. | Sau khi phần mềm điều khiển GPIO, xuất `R/R`, giữ khởi động 1 s rồi vào `G/R` với thời lượng mặc định đầy đủ. Ghi thời điểm các lệnh đèn và timer đầu tiên. |
| TC-MAN-01 | **Mock + Board.** Vào Manual. Nhấn B2 để cho đường 1 đi, rồi nhấn B2 yêu cầu đổi đường. Lặp chiều ngược lại. | Từ `R/R` sang `G/R`. Khi đổi đường đi qua vàng của đường đang đi, rồi `R/R` đủ thời lượng trước khi bật xanh đường kia. Ghi log cặp màu. |
| TC-MAN-02 | **Mock + Board.** Cấp chuỗi dội hoặc giữ B2, nhấn thêm B2 trong pha vàng/khoảng đỏ, thử B1 và B2 cùng lượt xử lý. | Một thao tác cho mỗi lần nhấn đã chống dội. B2 trong pha chuyển bị bỏ qua. B1 được ưu tiên, B2 không chạy bù sau khi vào chế độ mới. Lưu log sự kiện nút. |
| TC-TUNE-01 | **Mock + Board.** Vào Tuning, nhấn B2 để đi qua `R → G → Y → R`. Dùng B3 tăng đỏ từ 9 lên 11 s rồi nhấn B1 xác nhận. | LCD hiển thị trường/giá trị đang chỉnh. Trong Tuning là `R/R`. Khi quay lại Auto, bộ mới là `G=5, Y=2, R=11`, hai khoảng `R/R` là 2 s, chu kỳ danh định 18 s. Lưu LCD và trace một vòng. |
| TC-TUNE-02 | **Mock + Board.** Với `G=5, Y=2`, giảm R xuống 8 rồi xác nhận. Thử giảm/tăng ở biên của từng trường. Sau phần thử biên, đặt lại đầy đủ `G=5, Y=2, R=9` rồi xác nhận. | R=8 bị từ chối vì `R < G+Y+2`. LCD báo lỗi, giữ Tuning và cấu hình chạy cũ. Tăng/giảm không vượt biên. Bộ `G=5, Y=2, R=9` hợp lệ, trở về Auto sau khoảng đỏ. |
| TC-MODE-01 | **Mock + Board.** Yêu cầu Auto → Manual trong từng loại pha xanh, vàng, cả hai đỏ, thử cho cả hai đường. | Nếu đang xanh hoặc vàng, đường đang hoạt động giữ vàng trọn `Y_active`, rồi cả hai đỏ tối thiểu 1 s. Nếu đang cả hai đỏ, bắt đầu khoảng đỏ 1 s. Manual vào `WAIT` với `R/R`. Lưu log đèn và timer cũ/mới. |
| TC-MODE-02 | **Mock + Board.** Yêu cầu Manual → Tuning khi Manual đang xanh, vàng, hoặc cả hai đỏ. Nhấn B1/B2 thêm trong lúc chuyển và giữ B1 đến sau khi vào mode mới. | Đèn đi qua đúng pha chuyển. Chỉ có một lần đổi mode. Vào Tuning ở `R/R`, LCD hiện trường chỉnh, nút giữ và timeout cũ không đổi trạng thái mới. Lưu kết quả xóa hàng đợi và kiểm tra sự kiện `PRESS` mới sau khi nhả. |
| TC-MODE-03 | **Mock + Board.** Đưa B1 và timeout của mode cũ vào cùng lượt `fsm_run()`. Xác nhận Tuning hợp lệ để về Auto và theo dõi lượt nạp timer khi `sw_timer_tick()` cùng đến hạn. Quan sát thứ tự ghi đèn khi cấp xanh. | B1 ưu tiên trước timeout cũ. Timer cũ được hủy, timer mới chỉ được nạp sau khi áp dụng màu và lần giảm đầu tiên là ở tick kế tiếp. `R/R` trước khi Auto vào `G/R`. Đường đối diện đã đỏ trước khi cấp xanh. Lưu trace mock và quan sát GPIO đủ độ phân giải nếu cần kiểm tra xung thoáng qua. |
| TC-SCH-01 | **Review + Board.** Soát đường gọi từ timer ISR vào `SCH_Update`. Nếu đo, so sánh khi ít/nhiều task đang đăng ký. | Chỉ xử lý phần tử đầu và các thao tác có giới hạn cố định. Không có duyệt danh sách trong đường cập nhật. Lưu đoạn code làm bằng chứng O(1) và trace đo nếu có. |
| TC-SCH-02 | **Mock + Board.** Tạo hai task chu kỳ 50/100 tick và one-shot sau 300 tick ở tick 10 ms. Ghi tick dispatch. | Khoảng cách các lần chạy định kỳ danh định là 0,5 s và 1 s. One-shot chạy đúng một lần sau 3 s, được xóa, các task định kỳ tiếp tục. Lưu trace tick và số lần chạy. |
| TC-SCH-03 | **Mock.** Chèn task đầu, giữa, cuối và hai task cùng hạn. Xóa task chờ ở đầu/giữa/cuối rồi chạy tiếp. | Task còn lại giữ thời điểm dự kiến, task cùng hạn đều được dispatch, task đã xóa không chạy. Lưu bảng hạn dự kiến/thực tế và thứ tự dispatch theo quy ước scheduler. |
| TC-ENV-01 | **Board.** Chạy Auto và Manual qua nhiều lần đọc DHT20, sau đó vào Tuning rồi quay lại Auto. | Nhiệt độ/độ ẩm cập nhật theo chu kỳ của task môi trường. LCD giữ màn hình chỉnh trong Tuning và hiện lại dữ liệu môi trường khi về Auto. Lưu ảnh LCD và log đọc có timestamp. |
| TC-ARCH-01 | **Review + Mock.** Kiểm tra `fsm_*.c`, nơi gọi update/dispatch, vị trí gọi HAL trong toàn bộ mã ứng dụng và biên dịch logic với driver giả. | FSM gọi API module. Lời gọi HAL chỉ nằm trong module hỗ trợ và `main.c`. Sau khi dùng scheduler, chương trình chính và callback task không dùng `HAL_Delay`. Timer ISR gọi `SCH_Update()`, vòng chính dispatch `sw_timer_tick()`, `button_poll()` và `fsm_run()` theo lịch tác vụ. Lưu kết quả build mock và vị trí code. |
| TC-REL-01 | **Board.** Chạy liên tục theo thời lượng đã xác nhận, diễn tập Auto → Manual → Tuning → Auto và hiển thị DHT20/LCD. | Không treo, pha và điều kiện an toàn khớp thiết kế. Lưu thời lượng thực chạy, commit, log, ảnh hoặc video demo. |
| TC-REL-02 | **Review + Board.** Clone sạch, kiểm tra quyền xem repo theo quy định, làm theo README. Mở PDF, đối chiếu trang bìa, mục lục, bảng đóng góp và commit/tag. | Build/flash theo hướng dẫn được. Báo cáo khớp firmware, có bằng chứng thật, đã xóa đoạn `Guide`, tên PDF đúng LMS và hồ sơ nộp đúng bản. Lưu kết quả clone/build/flash và xác nhận nộp khi thực hiện. |

### Mẫu ghi khi thực hiện một ca

| Mã ca | Môi trường và commit | Thời điểm | Quan sát thực tế | Kết quả | Minh chứng hoặc mã lỗi |
|---|---|---|---|---|---|
| TC-AUTO-01 | Điền mock hoặc board, hash và cấu hình | Điền khi chạy | Ghi timestamp từng lần đổi pha | Chưa chạy | Điền đường dẫn log/ảnh |

Ghi **Đạt** khi quan sát khớp kỳ vọng trên môi trường được yêu cầu. Ghi **Không đạt** kèm bước tái hiện nếu sai. Ghi **Chưa chạy** cho ca chưa có dữ liệu. Với ca `Mock + Board`, tạo hai dòng kết quả riêng để theo dõi.

## Khung báo cáo để bắt đầu điền

| Mục | Nội dung cần chuẩn bị |
|---|---|
| Bìa | Thành viên, phần việc và tỷ lệ đóng góp, link GitHub, branch/tag/hash sau khi chốt bản nộp. |
| 1 | Mô tả bài toán, chức năng, sơ đồ khối và ảnh phần cứng. Chèn ảnh sau khi lắp board. |
| 2 | Lấy bảng nút, sơ đồ chế độ và bảng chuyển mode từ CO3009-06. Giải thích màu đèn, timer/task, bộ đếm, LCD và quy tắc không cùng xanh. |
| 3 | Bảng nối chân và mã màu, cấu trúc scheduler và danh sách task, API module, ba sơ đồ FSM và phần hiện thực. Nhân chuẩn bị 3.4 và 3.5, nhận số liệu và nội dung module của các thành viên. |
| 4 | Kết quả từng nhóm ca test có dẫn chứng, hạn chế còn lại và phần đóng góp cá nhân. |
| 5 | Cấu trúc repo, cách build, flash và thao tác đủ ba chế độ. |

## Điểm cần đối chiếu với nhóm và LMS

- **Tên PDF.** A4 đặt theo MSSV, Step 7 đặt theo mã nhóm. Điền quy định LMS vào `TC-REL-02` khi đã xác nhận.
- **Thời lượng chạy ổn định.** Step 7 còn để `[..] phút`. Điền số phút chính thức vào `TC-REL-01` trước khi chốt ca.
- **Đặc tả bổ sung.** Đề dẫn `Project_Group.docx`. Khi có tài liệu, rà lại yêu cầu về nút, thời gian, cách chuyển mode và cập nhật các ca liên quan.
- **Lịch tác vụ.** Đối chiếu thứ tự chạy `sw_timer_tick()`, `button_poll()` và `fsm_run()` với CO3009-18 trước khi chốt kỳ vọng theo tick trên board. Khi timer pha vừa được nạp, lần giảm đầu tiên phải thuộc tick kế tiếp.
