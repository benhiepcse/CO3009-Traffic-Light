# CLAUDE.md — CO3009 Traffic Light (STM32F103RB)

Tài liệu dùng chung cho mọi phiên Claude Code của nhóm. Mọi thông số dưới đây
được trích từ mã nguồn tại commit `87bb24e` (nhánh `main`). Mục nào chưa xác
minh được từ repo được đánh dấu **[CHƯA XÁC MINH]**.

---

## Phần 1. Tổng quan kiến trúc và thông số hệ thống

### 1.1 Phần cứng và xung nhịp

| Thông số | Giá trị | Nguồn |
|---|---|---|
| MCU | STM32F103RBTx, LQFP64, board NUCLEO-F103RB | `BTL.ioc` (`Mcu.UserName`, `board`) |
| Bộ nhớ | FLASH 128 KB @ `0x08000000`, RAM 20 KB @ `0x20000000` | `STM32F103RBTX_FLASH.ld` |
| Heap / Stack tối thiểu | `0x200` / `0x400` | `.ld`, `BTL.ioc` |
| Nguồn xung | HSI 8 MHz / 2 x PLL x16 = **64 MHz** SYSCLK/HCLK | `SystemClock_Config()` trong `Core/Src/main.c` |
| APB1 / APB1 timer | 32 MHz / 64 MHz (APB1 chia 2 nên timer clock x2) | `main.c`, `BTL.ioc` (`RCC.APB1TimFreq_Value`) |
| Flash latency | `FLASH_LATENCY_2` | `main.c` |
| HAL | STM32Cube FW_F1 V1.8.7, CubeMX 6.4.0 | `BTL.ioc` |
| NVIC | `NVIC_PRIORITYGROUP_4`; TIM2 và SysTick cùng mức ưu tiên 0 | `BTL.ioc` |

Ghi chú: `BTL.ioc` khai báo PD0/PD1 ở chế độ HSE External Clock nhưng
`SystemClock_Config()` dùng **HSI**. Độ chính xác của tick 10 ms vì vậy phụ
thuộc dung sai HSI; muốn khẳng định sai số thì phải đo độc lập (xem
`docs/co3009-18-task-schedule.md`).

### 1.2 Định thời

- `TIM2`: `Prescaler = 6399`, `Period = 99`, nên T = 6400 x 100 / 64 MHz = **10 ms**.
- Chuỗi gọi: `TIM2_IRQHandler` (`stm32f1xx_it.c`) gọi `HAL_TIM_IRQHandler(&htim2)`,
  rồi đến `HAL_TIM_PeriodElapsedCallback` (`main.c`, USER CODE 4), rồi `SCH_Update()`.
- Main loop gọi `SCH_Dispatch_Tasks()`.
- Một tick = 10 ms. Đổi ms sang tick bằng `SW_TIMER_MS_TO_TICKS(ms)`, **làm tròn lên**.
  Hậu tố tham số: `_ticks` và `_ms` (`docs/api-contract.md`).
- `timer_10ms_task` (`main.c`) được đăng ký `SCH_Add_Task(timer_10ms_task, 1, 1)` và gọi `sw_timer_tick()`.
- SysTick (1 ms) vẫn là timebase của HAL (`HAL_GetTick`). Scheduler **không** dùng SysTick.

### 1.3 Kiến trúc phân tầng

| Tầng | File thực tế | Phụ thuộc HAL |
|---|---|---|
| Phần cứng (CubeMX sinh) | `Core/Src/main.c`, `stm32f1xx_it.c`, `stm32f1xx_hal_msp.c`, `system_stm32f1xx.c`, `Core/Startup/`, `Drivers/` | Có |
| Dịch vụ định thời | `scheduler.c/.h` (delta queue tĩnh 16 slot, `SCH_Update` O(1)); `software_timer.c/.h` (`SW_TIMER_COUNT` timer tĩnh) | Chỉ `stm32f1xx.h` cho PRIMASK, tắt bằng `SCH_HOST_TEST` |
| Self-test và đo đạc | `sched_selftest.c/.h` (`SCH_SELFTEST`), khối `CO3009_18_MEASURE` và `CO3009_HW_BRINGUP` trong `main.c` | Có |
| Nghiệp vụ FSM | `global`, `fsm`, `fsm_automatic`, `fsm_manual`, `fsm_tuning`, cùng interface `button.h`, `traffic_light.h`, `lcd.h`, `dht20.h` | Không có (thiết kế qua interface) |
| Kiểm thử host | `tests/host/*.c`, `mock_modules.c/.h`, `scheduler_test_hooks.h`, `test.ps1` | Không |

**Khoảng trống quan trọng (dữ kiện):** Commit `50b2d3f` (đưa project CubeIDE vào repo)
đã **xóa** toàn bộ `Core/Src/{global,fsm,fsm_automatic,fsm_manual,fsm_tuning}.c` và
các header `button.h`, `traffic_light.h`, `lcd.h`, `dht20.h`, `fsm*.h`, `global.h`.
Bản cuối của các file này nằm ở commit `8872e54`. Hệ quả:
- Firmware hiện tại **chưa có FSM, Safe Mode switching và LCD ownership**. Chỉ có scheduler và software timer.
- 5 trên 7 target host test **không biên dịch được** (`api_smoke`, `fsm_automatic_test`,
  `fsm_manual_test`, `fsm_tuning_test`, `fsm_modes_test`).
- Muốn khôi phục: `git checkout 8872e54 -- <file>`, rồi đối chiếu lại `scheduler.h` và
  `software_timer.h` mới (API đã đổi sau `8872e54`).

Thiết kế FSM tham chiếu (từ `8872e54` và `docs/sprint-03/co3009-13-15-fsm.md`):
- Mode: `SYSTEM_BOOT, SYSTEM_AUTOMATIC, SYSTEM_MANUAL, SYSTEM_TUNING, SYSTEM_SWITCH_AMBER, SYSTEM_SWITCH_RED, SYSTEM_FAULT`.
- "Safe Mode" của nhóm tương ứng `SYSTEM_FAULT`: yêu cầu cả hai đỏ và chờ reset.
- Mọi lần đổi cặp màu đi qua `fsm_set_phase()`: đặt đèn trước, nạp timer sau.
- LCD ownership: chỉ `fsm_run()` gọi `lcd_write_line()`, thông qua `update_screen()` có cache `last_screen` (ghi khi nội dung đổi).
- Thứ tự trong task 10 ms: `sw_timer_tick()`, rồi `button_poll()`, rồi `fsm_run()` (`docs/co3009-18-task-schedule.md`).

### 1.4 Lệnh build và test

**Host test (đã xác minh trên Linux, gcc).** Script chính thức là PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\host\test.ps1            # tất cả
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\host\test.ps1 -Target scheduler_test
```

Lệnh tương đương bằng bash (cờ lấy nguyên từ `test.ps1`), chạy từ gốc repo:

```bash
CFLAGS="-std=c11 -O2 -Wall -Wextra -Werror -pedantic -ICore/Inc"
gcc $CFLAGS -DSCH_HOST_TEST Core/Src/scheduler.c tests/host/scheduler_test.c -o /tmp/scheduler_test && /tmp/scheduler_test
gcc $CFLAGS -DSCH_HOST_TEST Core/Src/software_timer.c Core/Src/scheduler.c tests/host/software_timer_test.c -o /tmp/software_timer_test && /tmp/software_timer_test
# Target FSM (chỉ chạy được khi các file FSM đã được khôi phục):
gcc $CFLAGS -DMOCK_MODULES_USE_REAL_FSM Core/Src/global.c Core/Src/fsm.c Core/Src/fsm_automatic.c \
    Core/Src/fsm_manual.c Core/Src/fsm_tuning.c tests/host/fsm_modes_test.c tests/host/mock_modules.c -o /tmp/fsm_modes_test && /tmp/fsm_modes_test
```

Quy tắc của `test.ps1`: target có `Scheduler = $true` thì thêm `-DSCH_HOST_TEST` và
**không** link `mock_modules.c`. Target `Real = $true` thì thêm `-DMOCK_MODULES_USE_REAL_FSM`.

Kết quả tại `87bb24e`: `scheduler_test` PASS, `software_timer_test` PASS, 5 target còn lại lỗi biên dịch (thiếu file).

**Firmware:** STM32CubeIDE, Project > Build Project (cấu hình `Debug`, macro `DEBUG`,
`USE_HAL_DRIVER`, `STM32F103xB`). Lệnh headless sau **[CHƯA XÁC MINH]** vì môi trường
khảo sát không có `arm-none-eabi-gcc` hay CubeIDE:

```bash
stm32cubeide --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data /tmp/ws -import . -build BTL/Debug
```

Chế độ firmware chọn bằng macro tiền xử lý (Project Properties > MCU GCC Compiler > Preprocessor),
không sửa `.cproject` trong commit:

| Macro | Hành vi |
|---|---|
| (không có) | Scheduler + `timer_10ms_task` |
| `SCH_SELFTEST` | Self-test 500/1000/3000 ms (`sched_selftest.c`) |
| `CO3009_18_MEASURE` | Ghi 64 mốc `HAL_GetTick` của IRQ và task |
| `CO3009_HW_BRINGUP` | I2C scan, bảng chân lý đèn, đọc mức nút (có `HAL_Delay`, chỉ dùng để bring-up) |

`CO3009_HW_BRINGUP` loại trừ hai macro kia (`#error` trong `main.c`).

---

## Phần 2. Ràng buộc với STM32CubeMX (`BTL.ioc`) và làm việc nhóm

1. **USER CODE.** `BTL.ioc` có `ProjectManager.KeepUserCode=true`. CubeMX chỉ giữ code nằm giữa
   `/* USER CODE BEGIN x */` và `/* USER CODE END x */`. Mọi chỉnh sửa trong `main.c`, `main.h`,
   `stm32f1xx_it.c`, `stm32f1xx_hal_msp.c`, `stm32f1xx_hal_conf.h` **bắt buộc** nằm trong cặp thẻ này.
   Không sửa thân `MX_*_Init()` hay `SystemClock_Config()` bằng tay; muốn đổi thì sửa `BTL.ioc` rồi generate lại.
2. **Module mới** (`button.c`, `lcd.c`, ...) đặt thành file riêng trong `Core/Src` và `Core/Inc`.
   CubeMX không chạm vào các file này. `main.c` chỉ include và gọi init, trong vùng USER CODE.
3. **Cô lập HAL.** Tầng nghiệp vụ (FSM, `global`) chỉ được include header interface
   (`button.h`, `traffic_light.h`, `lcd.h`, `dht20.h`, `software_timer.h`, `scheduler.h`),
   không include `main.h` hay `stm32f1xx_hal.h`, không gọi `HAL_*`. Code phụ thuộc phần cứng
   bị chặn bằng macro như mẫu `SCH_HOST_TEST` trong `scheduler.c`. Mọi module nghiệp vụ mới phải
   có target trong `tests/host/test.ps1`.
4. **Sau mỗi lần generate từ `BTL.ioc`:** chạy `git diff` để kiểm tra không mất USER CODE, build lại,
   chạy host test, và commit `BTL.ioc` cùng với code sinh ra trong **một** commit riêng.
5. **File cấu hình IDE.** Không commit thay đổi trong `.settings/`, `.cproject`, `.project`, `.mxproject`
   trừ khi cả nhóm thống nhất đổi cấu hình build. Macro thử nghiệm (`SCH_SELFTEST`, ...) chỉ bật cục bộ.
   Trước khi commit: `git diff --stat -- .settings .cproject .project .mxproject`.
6. **Quy trình nhánh** (`README.md` mục 7): `feature/<ma-task>-<ten-ngan>` tạo từ `dev`, PR vào `dev`,
   cần một người khác review; chỉ đưa vào `main` sau nghiệm thu. Commit theo dạng `module: mô tả`.

---

## Phần 3. Kỷ luật lập trình nhúng

1. **Bộ nhớ: không bắt buộc cấp phát tĩnh.** Project không bắt buộc dùng cấp phát tĩnh. Hiện tại
   code chưa dùng heap: bảng task (`SCH_MAX_TASKS = 16`) và timer (`SW_TIMER_COUNT`) là mảng tĩnh.
   Thêm timer mới thì thêm enum trước `SW_TIMER_COUNT`. Nếu dùng `malloc`, `calloc`, `realloc` hoặc `free`, phải tuân thủ:
   - Không gọi trong ISR hay trong callback ngắt, vì các hàm heap của thư viện C chuẩn không an toàn khi bị ngắt chen vào.
   - Luôn kiểm tra `NULL` và xử lý lỗi. Nếu lỗi xảy ra trong FSM thì chuyển về `SYSTEM_FAULT`.
   - Heap hiện chỉ có `0x200` (512 byte). Muốn tăng thì sửa `HeapSize` trong `BTL.ioc` rồi generate lại, không sửa tay file `.ld`.
   - Nên cấp phát một lần lúc khởi tạo, tránh cấp phát và giải phóng lặp lại trong vòng lặp chính để không bị phân mảnh heap.

   Hạn chế đệ quy, VLA, `snprintf` và `printf` vì stack chỉ 1 KB. Nếu dùng thì kiểm tra file `.su` sau khi build.
2. **Kỷ luật ngắt.**
   - `HAL_TIM_PeriodElapsedCallback` chỉ gọi `SCH_Update()` (O(1)), cộng phần đo đạc bounded khi có `CO3009_18_MEASURE`.
   - Trong ISR và trong callback chạy ở ngữ cảnh ngắt: cấm `HAL_Delay`, vòng chờ cờ, I2C/LCD/UART,
     `printf`, và mọi vòng lặp phụ thuộc số task.
   - Callback trong scheduler chạy ở main context nhưng chung một luồng: không được chặn.
     Nếu một callback chạy lâu hơn 10 ms thì tick dồn vào `pending_ticks` và các task chu kỳ bị trễ pha
     (scheduler không phát lại chu kỳ bị lỡ; xem `docs/co3009-08-dispatch.md`).
   - `HAL_Delay` chỉ được phép trong nhánh `CO3009_HW_BRINGUP`.
3. **Đồng bộ ISR và main loop.**
   - Biến chia sẻ giữa ISR và main phải khai báo `volatile`. Biến nhiều từ nhớ, chuỗi đọc-sửa-ghi
     và cấu trúc nhiều trường thì phải truy cập trong vùng găng ngắn theo đúng mẫu `lock_scheduler()` /
     `unlock_scheduler()`: lưu PRIMASK, `__disable_irq()`, khôi phục PRIMASK. Không dùng `__enable_irq()`
     trần vì nó phá lock lồng nhau.
   - Ngoại lệ hiện có (dữ kiện): `tasks`, `head`, `pending_ticks` trong `scheduler.c` **không** khai báo
     `volatile`. Tính đúng đắn dựa vào việc mọi truy cập phía main đều nằm trong lock, và `__disable_irq()`
     có clobber `"memory"` (`cmsis_gcc.h:142`) nên đóng vai trò compiler barrier. Ngoại lệ này chỉ đúng
     khi **mọi** truy cập mới phía main cũng nằm trong lock. Không nhân bản mẫu này cho module khác.
   - `sw_timer_*` được thiết kế để **chỉ gọi từ main context** (comment trong `software_timer.h`). Không gọi từ ISR.
4. **Kiểu dữ liệu và FSM phòng thủ.**
   - Chỉ dùng `<stdint.h>` và `<stdbool.h>`; hằng số không dấu có hậu tố `U`.
   - Kiểm tra tràn trước khi cộng hoặc nhân thời lượng (ví dụ `seconds * TRAFFIC_TICKS_PER_SECOND`).
     Tham khảo `pending_ticks < UINT32_MAX` trong `SCH_Update`.
   - Kiểm tra con trỏ `NULL` và giới hạn chỉ số tại API công khai (mẫu `valid_id()`).
   - Mọi `switch` trên trạng thái hoặc mode của FSM phải có `default:` chuyển sang `SYSTEM_FAULT`
     (cả hai đỏ). Riêng hàm định dạng chuỗi được phép `default:` trả chuỗi lỗi.
   - Bất biến an toàn: **không bao giờ** ra lệnh cho hai đường cùng `TRAFFIC_GREEN`. Mọi lần chuyển
     mode phải qua vàng (nếu đang xanh) rồi cả hai đỏ `FSM_ALL_RED_TICKS`. Giá trị trả về `false` của
     `traffic_set_pair`, `sw_timer_start`, `lcd_write_line` phải được xử lý, không được bỏ qua.
   - Biên dịch sạch với `-Wall -Wextra -Werror -pedantic`.

---

## Phần 4. Hệ thống skill cố vấn kỹ thuật cho nhóm

### 4.0 Giao thức tương tác (áp dụng cho mọi câu trả lời)

- Phân tách rõ **Dữ kiện** (dẫn `file:dòng` hoặc commit), **Suy luận**, **Giả định chưa xác minh**.
- Không đồng ý chiều lòng. Nếu đề xuất vi phạm thời gian thực, an toàn bộ nhớ, bất biến đèn hoặc
  ranh giới USER CODE/HAL, nói thẳng là sai, nêu lý do ở mức phần cứng (chu kỳ, thanh ghi, PRIMASK,
  stack), rồi đưa phương án thay thế.
- Không tự bịa API: chỉ dùng các hàm có trong `Core/Inc` hoặc ở commit `8872e54`. Hàm nào chưa tồn tại thì nói rõ.
- Không đánh dấu hoàn thành khi chưa chạy host test. Khi báo cáo, nêu rõ phần nào cần nghiệm thu trên board.

### Cách gọi skill

Ghi tên skill ở đầu prompt, kèm phạm vi:

```text
[SKILL: ARCH-GUARD] Core/Src/main.c
[SKILL: TIMING-CONCURRENCY] thêm button_poll vào timer_10ms_task
[SKILL: FSM-SAFETY] Core/Src/fsm.c
[SKILL: HOST-TDD] button debounce
[SKILL: PR-PREFLIGHT] feature/co3009-10-button -> dev
```

Mỗi skill trả về: (1) bảng phát hiện gồm mức độ, `file:dòng`, loại (Dữ kiện/Suy luận), nguyên nhân gốc,
cách sửa; (2) danh sách mục chưa xác minh; (3) kết luận ĐẠT hoặc KHÔNG ĐẠT.

### 4.1 `[SKILL: ARCH-GUARD]` — Kiến trúc và ranh giới CubeMX/HAL

Kích hoạt khi thêm module, hoặc sửa `Core/`, `Drivers/`, `BTL.ioc`.

1. `git diff` các file CubeMX sinh: mọi dòng thêm hoặc sửa phải nằm giữa một cặp `USER CODE BEGIN/END`
   cùng tên. Dòng nào nằm ngoài thì đánh dấu CHẶN.
2. `Drivers/` không được sửa. Nếu có diff ở đó thì CHẶN.
3. Module nghiệp vụ: `grep -n "HAL_\|main.h\|stm32f1xx" <file>` phải rỗng, hoặc chỉ nằm trong `#ifndef SCH_HOST_TEST`.
4. Có target tương ứng trong `tests/host/test.ps1` và biên dịch được bằng gcc host.
5. Đổi `BTL.ioc`: đối chiếu PSC/ARR/clock với mục 1.1 và 1.2. Nếu tick khác 10 ms thì CHẶN, trừ khi cập nhật toàn bộ hằng tick.
6. Header mới có include guard và chỉ include những gì cần.

### 4.2 `[SKILL: TIMING-CONCURRENCY]` — TIM2, ngắt 10 ms, LCD ownership

Kích hoạt khi sửa scheduler, software timer, callback ngắt, task định kỳ, LCD/I2C.

1. Liệt kê mọi hàm có thể chạy trong ISR (theo chuỗi từ `TIM2_IRQHandler`). Mỗi hàm phải O(1), không có vòng
   lặp phụ thuộc dữ liệu, không gọi HAL chặn hay I2C.
2. Ước lượng thời gian thực thi xấu nhất của từng callback scheduler, tính theo chu kỳ máy ở 64 MHz.
   Ví dụ 10 ms = 640 000 chu kỳ. Một giao dịch I2C 100 kHz ghi một dòng LCD 16 ký tự có thể tốn vài ms
   **[SUY LUẬN: phụ thuộc driver LCD, chưa có]**. Tổng thời gian trong một tick phải nhỏ hơn 10 ms, nếu không sẽ có drift.
3. Tìm biến dùng chung ISR/main: kiểm tra `volatile` hoặc lock theo Phần 3.3. Đặc biệt soi chuỗi đọc-sửa-ghi
   và biến 64-bit.
4. Vùng găng phải ngắn và không gọi callback người dùng bên trong (mẫu `SCH_Dispatch_Tasks` mở lock trước `callback()`).
5. LCD ownership: chỉ một nơi gọi `lcd_write_line` (theo thiết kế là `fsm_run` qua `update_screen`).
   Grep toàn repo. Nếu thấy nơi gọi thứ hai, hoặc gọi trong ISR, thì CHẶN. DHT20 chỉ lưu mẫu; FSM là bên đọc và hiển thị.
6. Thứ tự task trong cùng tick: `sw_timer_tick`, `button_poll`, `fsm_run`. Đổi thứ tự là đổi ngữ nghĩa thời lượng pha.
7. Bằng chứng trên board: build với `CO3009_18_MEASURE` và ghi min/max/mean vào `docs/co3009-18-task-schedule.md`.

### 4.3 `[SKILL: FSM-SAFETY]` — Máy trạng thái và safe mode switching

Kích hoạt khi sửa `fsm*.c`, `global.c`, logic nút hoặc chuyển mode.

1. Lập bảng chuyển trạng thái đầy đủ (Mermaid `stateDiagram-v2`) từ code thật, cho mọi cặp
   (trạng thái, sự kiện) với sự kiện thuộc {timeout, D7, D8, D9, D10, lỗi driver}.
   Ô nào không xác định thì là lỗi.
2. Mọi trạng thái có lối ra. Mọi `switch` có `default:` sang `SYSTEM_FAULT`.
3. Chứng minh bất biến:
   - I1: không bao giờ có (G, G), và mock không ghi nhận `mock_traffic_dual_green_requests() > 0`.
   - I2: từ xanh sang đỏ luôn qua vàng đủ thời lượng.
   - I3: giữa hai lần cấp xanh cho hai đường khác nhau luôn có cả hai đỏ.
   - I4: chuyển mode luôn đi qua `SYSTEM_SWITCH_AMBER` (nếu cần) rồi `SYSTEM_SWITCH_RED`.
   - I5: thời lượng chỉ được áp dụng khi `traffic_timing_valid()` trả về true.
4. Điều kiện biên: nhấn trong lúc BOOT hoặc chuyển mode; nhiều nút cùng tick (ưu tiên D7 > D8 > D9 > D10);
   timeout và D7 cùng tick; giá trị Tuning ở biên min/max; dispatch trễ nhiều tick (mỗi lần chỉ chuyển một pha).
5. Lỗi driver (`traffic_set_pair`, `sw_timer_start` trả false) phải đưa về `SYSTEM_FAULT`. Kiểm bằng `mock_fail_next_*`.

### 4.4 `[SKILL: HOST-TDD]` — Unit test trong `tests/host/`

Kích hoạt trước khi viết hoặc sửa logic nghiệp vụ.

1. Viết test trước, chạy để thấy FAIL, rồi mới hiện thực.
2. Mỗi target mới: thêm vào `$targets` trong `test.ps1` (đúng cờ `Scheduler` hoặc `Real`) và thêm dòng bash tương đương vào mục 1.4.
3. Bao phủ bắt buộc:
   - Luồng chuẩn: ít nhất 3 chu kỳ đầy đủ, kiểm từng tick.
   - Biên thời gian: duration 0, 1, `UINT32_MAX`; ID quay vòng; hàng đợi đầy.
   - Tiêm lỗi: `mock_fail_next_timer_start`, `mock_fail_next_traffic_pair`, `mock_fail_next_lcd_write`, DHT20 lỗi.
   - Bất biến I1–I5 kiểm sau **mỗi** tick.
4. Mock mới đặt trong `mock_modules.c/.h`, không chứa logic nghiệp vụ. Hook nội bộ đặt sau macro `*_HOST_TEST`
   (mẫu `scheduler_test_hooks.h`).
5. Test là tất định: không dùng thời gian thực, không random nếu không cố định seed.
6. Biên dịch với `-Werror`. In `PASS:` theo từng ca, trả exit code khác 0 khi fail.

### 4.5 `[SKILL: PR-PREFLIGHT]` — Rà soát trước khi merge

Kích hoạt trước khi mở PR hoặc merge vào `dev` hay `main`.

1. `git fetch && git diff --stat origin/dev...HEAD`: phạm vi khớp mã task, không có file rác (`Debug/`, `*.o`, `.metadata/`).
2. `git diff origin/dev...HEAD -- .settings .cproject .project .mxproject Drivers` phải rỗng, hoặc có lý do được nhóm duyệt.
3. Chạy ARCH-GUARD trên các file `Core/` thay đổi.
4. Nếu chạm scheduler, timer hay ISR: chạy TIMING-CONCURRENCY. Nếu chạm FSM: chạy FSM-SAFETY.
5. Toàn bộ host test PASS (mục 1.4). Ghi rõ target nào bị bỏ qua và lý do.
6. Firmware build 0 error, 0 warning trong CubeIDE cấu hình `Debug`. Ghi rõ nếu chưa build được.
7. `grep -rn "HAL_Delay" Core/Src` chỉ được phép trả về các dòng trong khối `CO3009_HW_BRINGUP`.
   `grep -rn "malloc\|calloc\|realloc\|free(" Core/Src`: mỗi chỗ gọi phải có kiểm tra `NULL` và không nằm trong ngữ cảnh ngắt (Phần 3.1).
8. `docs/` và `README.md` được cập nhật nếu API, bảng nút, thời lượng hoặc lệnh build thay đổi. Kết quả đo board ghi
   "Pending board" nếu chưa đo, không điền số giả.
9. Mô tả PR có: mã task, thay đổi chính, cách kiểm tra, các mục còn chưa xác minh trên board.
