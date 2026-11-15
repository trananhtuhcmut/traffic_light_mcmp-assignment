# Integration & Fix Notes for Peripheral Modules

This note lists conflicts found between the current peripheral modules and the updated FSM architecture (`Core/Inc/global.h`, `fsm_auto`, `fsm_manual`, `fsm_setting`).

---

## 1. `Core/Src/env_display.c` & `Core/Inc/env_display.h`

### Removed `global.h` Symbols (Compile Errors)
`display_task()` in `Core/Src/env_display.c` (lines 47–54) references symbols from an older `global.h` that no longer exist:
- `IS_TUNING(status)`
- `IS_AUTO(status)`
- `status`
- `time_red`, `time_green`, `time_amber`

### Missing `MODE_SETTING` LCD Rendering
`Core/Src/env_display.c` (lines 47–48) skips drawing when in tuning/setting mode (`if (IS_TUNING(status)) return;`), assuming the FSM draws the LCD itself.
However, `fsm_auto`, `fsm_manual`, and `fsm_setting` do **not** call `lcd_put_line()` directly; they only update `curr_disp_data` (`Display_Data_t` in `Core/Inc/global.h`):
- `mode` (`MODE_AUTO`, `MODE_MANUAL`, `MODE_SETTING`)
- `countdown_road1`, `countdown_road2` (used in `MODE_AUTO`)
- `active_field` (`EDIT_NONE`, `EDIT_Y1`, `EDIT_G1`, `EDIT_Y2`, `EDIT_G2`) and `draft_duration` (used in `MODE_SETTING`)

**What to fix:**
1. Update `display_task()` to switch on `curr_disp_data.mode` (`MODE_AUTO`, `MODE_MANUAL`, `MODE_SETTING`).
2. Read durations from `curr_disp_data.countdown_road1` / `countdown_road2` (or `curr_light_config`) in `MODE_AUTO`, and render `curr_disp_data.active_field` + `curr_disp_data.draft_duration` in `MODE_SETTING`.
3. Optionally sync `env_temp_x10`, `env_hum_x10`, and `env_valid` into `curr_disp_data` (`temp_x10`, `hum_x10`, `env_valid`) if other modules read from `Display_Data_t`.

---

## 2. `Core/Inc/traffic_light.h` & `Core/Src/traffic_light.c`

### Type Name Mismatch (`TrafficLightState` vs. `Traffic_Light_State`)
- `Core/Inc/traffic_light.h` (lines 14–15) and `Core/Src/traffic_light.c` (lines 24, 32, 51) use `TrafficLightState`, without `#include "global.h"` or defining `TrafficLightState`.
- `Core/Inc/global.h` (lines 38–44) defines the enum as `Traffic_Light_State` with values `TL_OFF = 0`, `TL_AMBER = 1`, `TL_GREEN = 2`, `TL_RED = 3`.

**What to fix:**
- Include `"global.h"` in `Core/Inc/traffic_light.h` and rename `TrafficLightState` to `Traffic_Light_State` in both `traffic_light.h` and `traffic_light.c`.

---

## 3. `Core/Inc/button.h` & `Core/Src/button.c`

### Macro Redefinition (`NUM_BUTTONS`)
- `Core/Inc/global.h` (line 17) defines `#define NUM_BUTTONS 4U`.
- `Core/Inc/button.h` (line 20) defines `#define NUM_BUTTONS 5`.
- Including both headers in the same `.c` file will trigger a macro redefinition warning/error and array-size mismatch.

### Pin Overlap with `traffic_light.c`
According to the pin mapping comments in both files:
- `Core/Src/traffic_light.c` (lines 19–20): `LED1_1 (PC7)`, `LED1_2 (PB6)`, `LED2_1 (PA8)`, `LED2_2 (PA9)`
- `Core/Src/button.c` (lines 19–23): `Button1_1 (PA8)`, `Button1_2 (PA9)`, `Button2_1 (PC7)`, `Button2_2 (PB6)`, `B1 (PC13)`

**What to fix:**
1. Align `NUM_BUTTONS` (and the button port/pin arrays) between `button.h`, `button.c`, and `global.h`.
2. Verify in `.ioc` / `main.h` that the button pins and traffic light LED pins do not share the same GPIO pins (`PA8`, `PA9`, `PC7`, `PB6`).
3. Add a mapping/event-generation step (using `is_button_pressed()` / `is_button_long_pressed()`) to set the one-shot flags in `event` (`System_Event_t` in `global.h`): `MODE_CHANGE`, `SWITCH_LIGHT`, `INCREASE`, `DECREASE`, and `CONFIRM` depending on `curr_mode`.

---

## 4. `Core/Inc/scheduler.h` & `Core/Src/scheduler.c`

### `SCH_REPORT_ERRORS` Overwrites `GPIOB->ODR`
- `Core/Inc/scheduler.h` (lines 25, 39–41) enables `#define SCH_REPORT_ERRORS` and maps `#define Error_port GPIOB->ODR`.
- `Core/Src/scheduler.c` (line 121) executes:
  ```c
  Error_port = (Error_port & 0xFF00) | (uint8_t)(255 - Error_code_G);
  ```
- Whenever `SCH_Update()` runs `SCH_Report_Status()`, any scheduler error overwrites pins `PB0`–`PB7` on `GPIOB`, which can corrupt `PB6` (`LED1_2` / `Button2_2`) or I2C/other `GPIOB` peripherals.

**What to fix:**
- Comment out or remove `#define SCH_REPORT_ERRORS` in `Core/Inc/scheduler.h` (or remove the `GPIOB->ODR` write from `SCH_Report_Status()`).
