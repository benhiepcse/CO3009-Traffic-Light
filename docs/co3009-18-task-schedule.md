# CO3009-18 - TIM2 integration and task schedule

## Section 3.2.3 - Timer configuration

TIM2 uses htim2 and the APB1 timer clock of 64 MHz. APB1 runs at
32 MHz with an APB1 divider of 2, so the TIM2 clock is twice PCLK1.
PSC = 6399 and ARR = 99 give (6399 + 1) * (99 + 1) / 64000000 s,
which equals 10 ms. TIM2 global interrupt is enabled.

Default startup initializes the scheduler, drivers and FSM, registers
the tasks once, clears the TIM2 update flag and starts TIM2 IRQ.
TIM2_IRQHandler calls HAL_TIM_IRQHandler, which invokes the period
elapsed callback. Only TIM2 calls SCH_Update. The main loop dispatches
callbacks. The default loop and the control task contain no HAL_Delay.

## Task schedule

| Work | Callback or owner | Delay ticks/ms | Period ticks/ms | Status |
|---|---|---:|---:|---|
| Software timer, buttons and FSM | `control_10ms_task` | 1 / 10 | 1 / 10 | Always registered |
| DHT20 service | `dht20_poll` | 1 / 10 | 1 / 10 | Sensor sampling interval is 2 s |
| LCD refresh | `fsm_display_run` | 1 / 10 | 5 / 50 | Registered if LCD initialization succeeds |

The control task calls `sw_timer_tick()`, `button_poll()` and `fsm_run()`
in that order.
This keeps the order even if a TIM2 IRQ occurs between calls. A timer
started by the FSM then first decrements on the next control cycle.
The LCD task writes at most one changed line per callback. Both I²C
drivers run in the cooperative scheduler, so measure control dispatch
intervals with the display and sensor active on the board.

## Board measurement

Build Debug with CO3009_18_MEASURE and run for at least one second
before suspending. Inspect irq_ms, irq_count, timer_task_ms and
timer_task_count in main.c. Once both counts reach 64, calculate the
63 adjacent timestamp differences for each buffer. Record min, max
and average in milliseconds, the commit and the ELF used for the run.

HAL_GetTick has 1 ms resolution. The results show the relationship
between TIM2 and the HAL timebase and the task dispatch timing.
Use an independent measurement if clock accuracy must be established.

| Signal | Configured or expected | Measured min / max / mean |
|---|---|---|
| TIM2 IRQ | 10 ms | Pending board |
| Software timer, buttons and FSM | 10 ms nominal | Pending board |
| DHT20 service and LCD refresh | Per task schedule | Pending board |

Use SCH_SELFTEST for the separate CO3009-08 periodic and one-shot
demo. CO3009_18_MEASURE can also record IRQ timestamps in that mode.
Use CO3009_HW_BRINGUP alone to run the earlier hardware checks.
