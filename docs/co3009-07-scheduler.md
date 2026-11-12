# CO3009-07 - Scheduler queue

The scheduler uses a static array of 16 slots and links active
tasks by slot index. The array bounds memory usage, while the links
let insertion and deletion update the queue without shifting entries.

The head stores the delay until its deadline. Each later task stores
the difference from the previous deadline. Insertion subtracts the
inserted delay from the successor. Deletion adds the removed delay
to the successor. Tasks with equal deadlines keep insertion order.

When the head is already due, SCH_Update increments pending_ticks.
Queue operations apply pending ticks before editing the queue.

## Complexity

N is the pool capacity. n is the number of active tasks.

| Function | Time | Reason |
|---|---|---|
| SCH_Init | O(N) | Clears the pool. |
| SCH_Add_Task | O(N + n) without ID collisions, O(N + n^2) worst case | Scans for a free slot and insertion point. After ID wraparound, each candidate ID can require another queue scan. |
| SCH_Delete_Task | O(n) | Applies pending ticks and searches for the task ID. |
| SCH_Update | O(1) | Checks the head and updates one counter. No queue traversal. |

The static pool uses O(N) storage.

## Host verification

From the repository root, run:

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\host\test.ps1 -Target scheduler_test

The tests cover insertion, deletion, equal deadlines, zero delay,
full capacity, invalid IDs, slot reuse, ID wraparound, pending ticks
and maximum delay.
