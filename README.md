# Two-Road Traffic Light System on STM32 XNUCLEO-F103RB

Course project for **Microprocessors – Microcontrollers (CO3009)** at Ho Chi Minh City University of Technology (VNU-HCM) - Class **CC02**, Group **Triple Tu**.

An embedded two-road intersection traffic light controller built on the **XNUCLEO-F103RB** (`STM32F103RBT6`, ARM Cortex-M3) featuring a deterministic O(1) cooperative scheduler, hardware-independent Finite State Machines (FSMs), real-time **DHT20** temperature/humidity monitoring on a **16x2 I2C LCD**, and automatic I2C bus/hot-plug fault recovery.

---

## Key Features

* **O(1) Cooperative Scheduler (`scheduler.c/.h`)**: Delta-delay sorted static task queue (`SCH_MAX_TASKS = 10`) driven by a 10 ms `TIM2` interrupt, supporting both periodic and one-shot tasks with `PRIMASK` critical-section guards and `__WFI()` low-power sleep in the main loop.
* **Hardware-Independent FSM Architecture**: Three decoupled operating modes (`fsm_auto`, `fsm_manual`, `fsm_setting`) using a uniform 4-function lifecycle API (`_init`, `_enter`, `_run`, `_exit`) and pointer-injected shared state (`global.h`) with zero HAL/GPIO/I2C calls inside FSM modules.
  * **Automatic Mode**: Continuous 4-phase intersection cycle (`R1-G2 → R1-Y2 → G1-R2 → Y1-R2`) with per-road LCD countdowns (defaults: Red 5 s, Green 3 s, Amber 2 s).
  * **Manual Mode**: Preserves the active traffic phase on entry and enforces a timed Amber safety buffer (`Y1_dur` / `Y2_dur`) whenever switching right-of-way.
  * **Timing Adjustment Mode**: Independent live tuning of all six per-road durations (`R1`, `Y1`, `G1`, `R2`, `Y2`, `G2`, 1–99 s wrap-around) while blinking only the active road at 2 Hz (opposing road off), with `"SET!"` confirmation feedback (2 s) and cross-road validation (`R1 == G2 + Y2` and `R2 == G1 + Y1`) showing `"INVALID! R!=G+Y"` (2 s) on mismatch.
* **Non-Blocking Environmental Monitoring (`dht20.c`, `lcd.c`, `env_display.c`)**:
  * Polls the **DHT20** (`0x38`) every 2 s using a non-blocking trigger + 100 ms one-shot read task, CRC-8 verification, and integer fixed-point conversion (`0.1 °C`, `0.1 %RH`).
  * Drives the **16x2 HD44780 LCD** via PCF8574 (`0x21`) using differential shadow buffers (`lcd_want` vs. `lcd_shown`), flushing at most one changed row per 50 ms tick.
* **Safety & Fault Tolerance**:
  * Coordinator-level safety interlock prevents simultaneous Green lights outside Setting mode.
  * **I2C1 Bus Recovery (`i2c.c`)**: Automatic 9-clock `SCL` pulse + manual `STOP` + `RCC` reset (`I2C1_Bus_Recover`) when bus lockups (`HAL_BUSY`, `HAL_TIMEOUT`, `BERR`, `ARLO`) occur during hot-plugging.
  * **Non-Blocking LCD Hot-Plug Recovery & Proteus Soft-I2C Fallback (`lcd.c`)**: 4-state background re-initialization (`OFFLINE → INIT_1 → INIT_2 → READY`) with 200 ms connectivity probing on reconnect, plus automatic per-transaction bit-banged Soft-I2C fallback for Proteus simulation.

---

## Hardware Connections

| Peripheral | Arduino Header | STM32 Pin | Mode | User Label |
| :--- | :---: | :---: | :--- | :--- |
| **Traffic Light 1** | `D3`, `D4` | `PB3`, `PB5` | Output Push-Pull (No Pull) | `LED1_1` (Sig A), `LED1_2` (Sig B) |
| **Traffic Light 2** | `D5`, `D6` | `PB4`, `PB10` | Output Push-Pull (No Pull) | `LED2_1` (Sig A), `LED2_2` (Sig B) |
| **Buttons 1, 2** | `D7`, `D8` | `PA8`, `PA9` | Input (No Pull, ext. pull-up) | `Button1_1`, `Button1_2` |
| **Buttons 3, 4** | `D9`, `D10` | `PC7`, `PB6` | Input (No Pull, ext. pull-up) | `Button2_1`, `Button2_2` |
| **On-board LEDs** | - | `PA5`, `PC9` | Output Push-Pull (No Pull) | `Flashing`, `Built_IN_LED` |
| **USER Button** | - | `PC13` | EXTI Rising (Polled Active-Low) | `B1` |
| **DHT20 & LCD** | I2C Header | `PB8` (SCL), `PB9` (SDA) | AF Open-Drain (`I2C1` Remap, 100 kHz) | DHT20: `0x38`, LCD: `0x21` (7-bit) |

### 2-Bit Traffic Light Signal Encoding

| Signal 1 (MSB) | Signal 2 (LSB) | Enum (`Traffic_Light_State`) | Light Output |
| :---: | :---: | :--- | :--- |
| `0` | `0` | `TL_OFF` | All LEDs Off |
| `0` | `1` | `TL_AMBER` | Amber LED On |
| `1` | `0` | `TL_GREEN` | Green LED On |
| `1` | `1` | `TL_RED` | Red LED On |

---

## Operating Modes & Controls

| Button | Automatic Mode | Manual Mode | Timing Adjustment Mode |
| :--- | :--- | :--- | :--- |
| **Button 1 (`D7 / PA8`)** | Switch to Manual (`MODE_CHANGE`) | Switch to Setting (`MODE_CHANGE`) | Validate (`R1 == G2+Y2 && R2 == G1+Y1`) & return to Automatic |
| **Button 2 (`D8 / PA9`)** | No effect | Short press: Switch road via Amber buffer (`SWITCH_LIGHT`) | Short press: Cycle `R1 → Y1 → G1 → R2 → Y2 → G2` (`SWITCH_LIGHT`) |
| **Button 3 (`D9 / PC7`)** | No effect | No effect | Short press : `+1 s` (wrap `99 → 1`) & clear `SET!` (`INCREASE`) |
| **Button 4 (`D10 / PB6`)** | No effect | No effect | Short press : `-1 s` (wrap `1 → 99`) & clear `SET!` (`DECREASE`) |
| **USER `B1` (`PC13`)** | No effect | No effect | Short press: Confirm draft into `temp_config` & show `"SET!"` for 2 s (`CONFIRM`) |

---

## Scheduler Task Table (`TIM2` = 10 ms Tick)

| Task | Initial Delay | Period | Description |
| :--- | :---: | :---: | :--- |
| `button_reading` | 0 ms (0 ticks) | 10 ms (1 tick) | Polls all 5 buttons with 2-sample debounce, falling-edge & 1 s long-press detection |
| `system_fsm_task` | 0 ms (0 ticks) | 10 ms (1 tick) | Generates 250 ms & 1 s flags, maps button events, runs mode coordinator & active FSM, enforces safety interlock, updates GPIOs |
| `display_task` | 50 ms (5 ticks) | 100 ms (10 ticks) | Formats LCD line 0 (mode / countdown / `SET!` / `INVALID!`) and line 1 (DHT20 readings) into shadow buffer |
| `lcd_flush_task` | 60 ms (6 ticks) | 50 ms (5 ticks) | Flushes at most 1 modified LCD row per call over I2C and handles non-blocking hot-plug recovery |
| `env_sample_task` | 100 ms (10 ticks) | 2000 ms (200 ticks) | Triggers DHT20 measurement (`0xAC 0x33 0x00`) and schedules one-shot `env_read_task` |
| `env_read_task` | 100 ms (10 ticks) | One-shot (`0`) | Reads 7 bytes from DHT20 after ≥ 80 ms conversion, verifies busy bit & CRC-8, updates `curr_disp_data` |

---

## Project Structure

```text
traffic_light_mcmp-assignment/
├── Core/
│   ├── Inc/        # button.h, dht20.h, env_display.h, fsm_auto.h, fsm_manual.h,
│   │               # fsm_setting.h, global.h, gpio.h, i2c.h, lcd.h, main.h,
│   │               # scheduler.h, stm32f1xx_hal_conf.h, stm32f1xx_it.h, tim.h, traffic_light.h
│   ├── Src/        # button.c, dht20.c, env_display.c, fsm_auto.c, fsm_manual.c,
│   │               # fsm_setting.c, global.c, gpio.c, i2c.c, lcd.c, main.c,
│   │               # scheduler.c, stm32f1xx_hal_msp.c, stm32f1xx_it.c, tim.c, traffic_light.c
│   └── Startup/    # startup_stm32f103rbtx.s
├── Drafts/         # fsm_documentation.md, teammate_fixes_note.md, traffic_fsm.drawio
├── Drivers/        # CMSIS/, STM32F1xx_HAL_Driver/
├── Demo.pdsprj     # Proteus simulation schematic
├── traffic_light.ioc
└── README.md
