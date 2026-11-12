# CO3009-18 - TIM2 integration and task schedule

## Section 3.2.3 - Timer configuration

TIM2 uses htim2 and the APB1 timer clock of 64 MHz. APB1 runs at
32 MHz with an APB1 divider of 2, so the TIM2 clock is twice PCLK1.
PSC = 6399 and ARR = 99 give (6399 + 1) * (99 + 1) / 64000000 s,
which equals 10 ms. TIM2 global interrupt is enabled.

Default startup initializes the scheduler and software timer, registers
the timer task once, clears the TIM2 update flag and starts TIM2 IRQ.
TIM2_IRQHandler calls HAL_TIM_IRQHandler, which invokes the period
elapsed callback. Only TIM2 calls SCH_Update. The main loop dispatches
callbacks. The default loop and the timer task contain no HAL_Delay.

## Task schedule

| Work | Callback or owner | Delay ticks/ms | Period ticks/ms | Status |
|---|---|---:|---:|---|
| Software timer | timer_10ms_task -> sw_timer_tick | 1 / 10 | 1 / 10 | Registered in default mode |
| Button sampling | button_poll in the 10 ms control task | 1 / 10 | 1 / 10 | Planned, driver pending |
| FSM | fsm_run in the 10 ms control task | 1 / 10 | 1 / 10 | Planned, FSM sources pending |
| DHT20 service | dht20_poll | TBD | TBD | Pending driver cadence |
| LCD | FSM owns display updates | Inherits FSM schedule | Inherits FSM schedule | Planned, no separate writer |

When button and FSM sources are available, extend the existing 10 ms
task to call sw_timer_tick, button_poll and fsm_run in that order.
This keeps the order even if a TIM2 IRQ occurs between calls. A timer
started by the FSM then first decrements on the next control cycle.
DHT20 polling cadence and sensor sampling cadence are separate
settings. Confirm them from the actual driver before registration.

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
| Software timer callback | 10 ms | Pending board |
| Button polling | 10 ms | Pending driver and board |
| FSM, DHT20 and LCD | Per integrated schedule | Pending integration |

Use SCH_SELFTEST for the separate CO3009-08 periodic and one-shot
demo. CO3009_18_MEASURE can also record IRQ timestamps in that mode.
Use CO3009_HW_BRINGUP alone to run the earlier hardware checks.
