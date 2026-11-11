# CO3009-03/04 — Hardware bring-up

## Pin mapping

| Arduino pin | STM32 pin | GPIO mode | User label |
|---|---|---|---|
| D3 | PB3 | Output Push-Pull | `TL1_A` |
| D4 | PB5 | Output Push-Pull | `TL1_B` |
| D5 | PB4 | Output Push-Pull | `TL2_A` |
| D6 | PB10 | Output Push-Pull | `TL2_B` |
| D7 | PA8 | Input | `BTN_D7` |
| D8 | PA9 | Input | `BTN_D8` |
| D9 | PC7 | Input | `BTN_D9` |
| D10 | PB6 | Input | `BTN_D10` |
| USER | PC13 | Input | `BTN_USER` |
| D14 | PB9 | I2C1 SDA | I2C1 |
| D15 | PB8 | I2C1 SCL | I2C1 |
| D13 | PA5 | Output Push-Pull | `LD2` |

The project is created for `NUCLEO-F103RB` with SYS Debug set to
`Serial Wire`. I2C1 runs in standard mode at 100 kHz. USART2 uses 115200,
8-N-1 for bring-up logs through the ST-LINK virtual COM port.

## Scheduler tick

The clock tree uses HSI through the PLL:

- SYSCLK/HCLK: 64 MHz
- APB1 peripheral clock: 32 MHz
- APB1 timer clock: 64 MHz

TIM2 is configured with `PSC = 6399` and `ARR = 99`:

```text
T = (PSC + 1) * (ARR + 1) / f_TIM
  = 6400 * 100 / 64,000,000
  = 0.01 s = 10 ms
```

The TIM2 interrupt increments `tick_10ms`. The project builds with zero
errors and zero warnings.

## Temporary test firmware

The bring-up code in `Core/Src/main.c` performs these checks:

1. scans 7-bit I2C addresses and prints detected devices over USART2;
2. applies `00`, `01`, `10`, and `11` to each traffic-light module for two
   seconds per combination;
3. prints the raw levels of D7-D10 and USER every 500 ms;
4. toggles LD2 on PA5 every 500 ms.

## Results pending physical hardware

### Traffic-light truth table

| Module | A | B | Observed state |
|---|---:|---:|---|
| Road 1 | 0 | 0 | Pending |
| Road 1 | 0 | 1 | Pending |
| Road 1 | 1 | 0 | Pending |
| Road 1 | 1 | 1 | Pending |
| Road 2 | 0 | 0 | Pending |
| Road 2 | 0 | 1 | Pending |
| Road 2 | 1 | 0 | Pending |
| Road 2 | 1 | 1 | Pending |

### Button levels

| Button | Released | Pressed | Active level |
|---|---:|---:|---|
| D7 | Pending | Pending | Pending |
| D8 | Pending | Pending | Pending |
| D9 | Pending | Pending | Pending |
| D10 | Pending | Pending | Pending |
| USER | Pending | Pending | Pending |

### I2C addresses

| Device | 7-bit address |
|---|---|
| DHT20 | Pending |
| LCD | Pending |

The pending fields must be filled from measurements on the assigned board.
