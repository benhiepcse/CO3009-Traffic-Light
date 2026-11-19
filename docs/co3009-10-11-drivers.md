# CO3009-10/11 - Traffic-light and button drivers

## Scope

- CO3009-10: translate logical traffic colors to the two GPIO outputs of each
  module and guarantee a safe green handover.
- CO3009-11: poll D7-D10 and USER every 10 ms, debounce them, and publish
  `PRESS` and `LONG_PRESS` events through a FIFO queue.

## Traffic-light driver

Public API: `Core/Inc/traffic_light.h`.

`traffic_set_pair()` validates the complete requested pair before writing any
GPIO. Invalid colors and `GREEN/GREEN` are rejected without changing the
outputs. During a green handover, the old green is removed first and only then
is the new green enabled. The A/B pins belonging to one module are written in a
single GPIO BSRR transaction to avoid a two-write intermediate code.

The provisional GPIO mapping lives in `Core/Inc/hardware_profile.h`. Each
module has its own table so the measured encodings can differ.

| Logical color | Road 1 A B | Road 2 A B |
| --- | --- | --- |
| OFF | 00 | 00 |
| RED | 01 | 01 |
| AMBER | 11 | 11 |
| GREEN | 10 | 10 |

## Button driver

Public API: `Core/Inc/button.h`.

- Each 10 ms poll converts the GPIO level to pressed or released using that
  button's active level. A change becomes stable after three consecutive
  equal samples.
- The debounced press emits one `PRESS`. A button held at startup follows
  the same three-sample rule.
- The driver emits one `LONG_PRESS` after 100 further pressed samples
  (1 s of sampled hold). A short release bounce pauses the counter. A
  stable release resets the counter and rearms the next press.
- Events on the same poll enter the FIFO in D7, D8, D9, D10, USER order. If
  all eight slots are filled, the new event is dropped. A clear empties the
  queue while keeping the current press and debounce state.

The provisional active levels in `hardware_profile.h` are active high for
D7–D10 and active low for USER.

## Firmware demos

In STM32CubeIDE, open **Project Properties → C/C++ Build → Settings →
MCU GCC Compiler → Preprocessor**. Add one symbol, build, and flash when a
board is available. Use USART2 at 115200 baud, 8N1. Remove the symbol to
return to the normal scheduler build. Both demos use the production driver
APIs and run from the main loop.

| Build symbol | Behavior |
| --- | --- |
| `CO3009_TRAFFIC_DEMO` | Logs each numbered pair over USART2. Advances every 2 seconds, then stays red–red. Reset to repeat. |
| `CO3009_BUTTON_DEMO` | Polls every 10 ms using the HAL tick. Logs the button and event, for example `D7 PRESS` or `USER LONG_PRESS`. |

### Traffic transition checklist

After measuring CO3009-04, update the profile for each road. Verify the
colors at each step. Capture A/B for both modules with a logic analyzer
during handovers to check the intermediate codes. A visual inspection can
verify the settled colors.

| UART steps | Expected output | Board result |
| --- | --- | --- |
| 00 | Both modules red after initialization | Pending board |
| 01–03, 06 | Road 1 off, amber, green, red. Road 2 stays red on those numbered steps. | Pending board |
| 03 → 04 | Green/red → red/red → red/green. Road 1 changes first. | Pending board |
| 04 → 05 | Red/green → red/red → green/red. Road 2 changes first. | Pending board |
| 07–09, 11 | Road 2 off, amber, green, red. Road 1 stays red. | Pending board |
| 09 → 10 | Dual-green request rejected. Red/green is preserved. | Pending board |

### Button demo checklist

| Interaction | Expected UART output | Board result |
| --- | --- | --- |
| Tap each of the five buttons | One `PRESS` per stable tap | Pending board |
| Hold each button past the long-press threshold | One `PRESS`, then one `LONG_PRESS`. Further holding adds no events. | Pending board |
| Release, then press again | A new `PRESS` after a debounced release | Pending board |
| Hold USER during reset, then release | One `USER PRESS` after debounce. Holding past the threshold also emits one `USER LONG_PRESS`. | Pending board |

## Host verification

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/test.ps1 -Target traffic_light_test
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/test.ps1 -Target button_test
```

Both targets passed on Windows with GCC. Traffic testing covers all 225
transitions between valid pairs, checks the output after each simulated
GPIO write, and checks rejection of invalid colors and dual green. Button
testing covers bounce, three-sample debounce, hold boundaries, release
and repress, held-at-init, simultaneous inputs, queue overflow, wraparound
and clearing.

Physical verification is pending. Measure the two raw color tables and the
released/pressed levels from CO3009-04 before recording board results.
