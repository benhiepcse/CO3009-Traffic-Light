# CO3009-08 - Dispatch and board self-test

The main loop calls SCH_Dispatch_Tasks. Readiness is represented by
a zero delay at the queue head. Each dispatch normalizes elapsed ticks
before removing a ready task. A periodic task is reinserted in the
same slot and with the same ID under the same interrupt lock.
A one-shot slot is released before its callback begins.

Callbacks run with the previous PRIMASK restored. They can add or
delete tasks, including deleting their own periodic task. Calls to
dispatch from a callback return immediately. Each outer dispatch
runs at most 16 callbacks and the next main-loop call can continue.

A periodic task's next deadline is the dispatch tick plus its period.
There is no replay of missed periods. When A (due 3, period 4), B
(due 5) and C (due 9) wait until tick 6, A and B run in that order.
C stays due at 9, and A moves to 10.

## Host verification

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\host\test.ps1 -Target scheduler_test

Both the CO3009-07 and CO3009-08 PASS lines should appear.

## XNUCLEO-F103RB self-test

1. Import BTL in STM32CubeIDE. In the active Debug configuration,
   open Project Properties > C/C++ Build > Settings > MCU GCC Compiler
   > Preprocessor and add SCH_SELFTEST to the defined symbols.
2. Clean and build. Confirm the compiler command contains
   -DSCH_SELFTEST for main.c and sched_selftest.c. Record the commit
   from git rev-parse HEAD and the ELF selected in the debug launcher.
3. Flash, run for over 6 seconds, then suspend once. Read the file-scope
   variables in sched_selftest.c (Debug with optimization disabled).
   Inspect trace[0..trace_count-1], count_500, count_1000, count_3000,
   and one_shot_delete_result. Do not single-step during timing capture.
4. Check timestamp differences between successive entries for each
   periodic task. The first timestamp is relative to setup, so it
   includes setup and any start phase offset. Record actual values.
5. Remove SCH_SELFTEST and clean/build to return to normal operation.

| Check | Expected | Measured |
|---|---|---|
| Consecutive 500-ms callbacks | Approximately 500 ms | Pending board |
| Consecutive 1000-ms callbacks | Approximately 1000 ms | Pending board |
| One-shot | Around 3000 ms, count stays at 1, delete result 0 | Pending board |
| PA5 / LD2 | Toggle edges 500 ms apart, rising edges 1000 ms apart | Pending board |

The trace stops recording at 32 entries. Counters keep running.
Host tests also fill the queue and verify that freed one-shot slots
can be allocated again.
