# Traffic Light FSM Architecture & Documentation

## 1. Design Overview

All three mode FSMs (`fsm_auto`, `fsm_manual`, `fsm_setting`) share a hardware-independent, event-driven architecture defined in `Core/Inc/global.h`:

- **No direct peripheral calls**: FSMs never call GPIO, LCD, or button drivers directly. Instead, each FSM reads `event` (`System_Event_t`) and `config` (`Traffic_Light_Config_t`), and writes outputs through bound pointers (`road_1`, `road_2`, `disp_data`).
- **State descriptor table (`State_t`)**: Every sub-state is represented by a `static const State_t` struct with `.on_entry`, `.on_run`, and `.on_exit` function pointers.
- **Unified lifecycle API**: An outer mode coordinator controls transitions between modes (`MODE_AUTO`, `MODE_MANUAL`, `MODE_SETTING`) using the four lifecycle functions exposed by each FSM:
  - `fsm_<mode>_init(...)` — Binds external pointers (`cfg`, `r1`, `r2`, `disp`) and resets internal context. Call once at startup.
  - `fsm_<mode>_enter()` — Sets `disp_data->mode` and transitions into the mode's initial sub-state. Call once when entering the mode.
  - `fsm_<mode>_run()` — Dispatches execution to `curr_state->on_run()`. Call periodically in the main loop / scheduler task while the mode is active.
  - `fsm_<mode>_exit()` — Runs `curr_state->on_exit()` (if defined) and clears internal state. Call once when leaving the mode on `MODE_CHANGE`.

---

## 2. Shared Data & Events (`Core/Inc/global.h`)

- **Timing Configuration (`Traffic_Light_Config_t`)**:
  - Holds `R1_dur`, `R2_dur`, `Y1_dur`, `Y2_dur`, `G1_dur`, `G2_dur` (in seconds, valid range `MIN_LIGHT_DUR` = `1U` to `MAX_LIGHT_DUR` = `99U`).
- **Light Outputs (`Traffic_Light_State`)**:
  - `TL_OFF = 0`, `TL_AMBER = 1`, `TL_GREEN = 2`, `TL_RED = 3`.
- **Display View Model (`Display_Data_t`)**:
  - `mode`: `MODE_AUTO`, `MODE_MANUAL`, or `MODE_SETTING`.
  - `countdown_road1`, `countdown_road2`: Remaining seconds for Road 1 and Road 2 in `MODE_AUTO`.
  - `active_field`: `EDIT_NONE`, `EDIT_Y1`, `EDIT_G1`, `EDIT_Y2`, or `EDIT_G2` in `MODE_SETTING`.
  - `draft_duration`: Unconfirmed duration currently being edited in `MODE_SETTING`.
- **Events (`System_Event_t`, `extern System_Event_t event`)**:
  - One-shot boolean flags (`TICK_1S`, `TICK_250MS`, `MODE_CHANGE`, `SWITCH_LIGHT`, `INCREASE`, `DECREASE`, `CONFIRM`) set by timers/button handlers and cleared after each loop pass.

---

## 3. `fsm_auto` (`Core/Inc/fsm_auto.h`, `Core/Src/fsm_auto.c`)

Runs the normal automatic traffic-light cycle using the durations in `const Traffic_Light_Config_t *cfg`.

### States & Transitions
1. **`STATE_AUTO_R1_G2`** *(Initial state on `fsm_auto_enter()`)*
   - **Entry**: Sets `*road_1 = TL_RED`, `*road_2 = TL_GREEN`; loads `loc_tim = config->G2_dur`; initializes `countdown_road1 = config->R1_dur`, `countdown_road2 = config->G2_dur`.
   - **Run (`TICK_1S`)**: Decrements `loc_tim` and `countdown_road1`. When `loc_tim == 0`, transitions to `STATE_AUTO_R1_Y2`; otherwise updates `countdown_road2 = loc_tim`.
2. **`STATE_AUTO_R1_Y2`**
   - **Entry**: Sets `*road_2 = TL_AMBER`; loads `loc_tim = config->Y2_dur`; sets `countdown_road2 = config->Y2_dur`.
   - **Run (`TICK_1S`)**: Decrements `loc_tim` and `countdown_road1`. When `loc_tim == 0`, transitions to `STATE_AUTO_G1_R2`; otherwise updates `countdown_road2 = loc_tim`.
3. **`STATE_AUTO_G1_R2`**
   - **Entry**: Sets `*road_1 = TL_GREEN`, `*road_2 = TL_RED`; loads `loc_tim = config->G1_dur`; initializes `countdown_road1 = config->G1_dur`, `countdown_road2 = config->R2_dur`.
   - **Run (`TICK_1S`)**: Decrements `loc_tim` and `countdown_road2`. When `loc_tim == 0`, transitions to `STATE_AUTO_Y1_R2`; otherwise updates `countdown_road1 = loc_tim`.
4. **`STATE_AUTO_Y1_R2`**
   - **Entry**: Sets `*road_1 = TL_AMBER`; loads `loc_tim = config->Y1_dur`; sets `countdown_road1 = config->Y1_dur`.
   - **Run (`TICK_1S`)**: Decrements `loc_tim`. When `loc_tim == 0`, wraps back to `STATE_AUTO_R1_G2`; otherwise updates `countdown_road1 = loc_tim` and decrements `countdown_road2`.

---

## 4. `fsm_manual` (`Core/Inc/fsm_manual.h`, `Core/Src/fsm_manual.c`)

Allows manual switching between roads while inserting a timed Amber safety buffer before granting Green to the other road.

### States & Transitions
1. **`STATE_MAN_R1_G2`** *(Initial state on `fsm_manual_enter()`)*
   - **Entry**: Sets `*road_1 = TL_RED`, `*road_2 = TL_GREEN`.
   - **Run (`SWITCH_LIGHT`)**: Transitions to `STATE_MAN_R1_Y2_BUF`.
2. **`STATE_MAN_R1_Y2_BUF`** *(Amber safety buffer for Road 2)*
   - **Entry**: Sets `*road_1 = TL_RED`, `*road_2 = TL_AMBER`; loads `loc_tim = config->Y2_dur`.
   - **Run (`TICK_1S`)**: Decrements `loc_tim`; when `loc_tim == 0`, transitions to `STATE_MAN_G1_R2`.
3. **`STATE_MAN_G1_R2`**
   - **Entry**: Sets `*road_1 = TL_GREEN`, `*road_2 = TL_RED`.
   - **Run (`SWITCH_LIGHT`)**: Transitions to `STATE_MAN_Y1_R2_BUF`.
4. **`STATE_MAN_Y1_R2_BUF`** *(Amber safety buffer for Road 1)*
   - **Entry**: Sets `*road_1 = TL_AMBER`, `*road_2 = TL_RED`; loads `loc_tim = config->Y1_dur`.
   - **Run (`TICK_1S`)**: Decrements `loc_tim`; when `loc_tim == 0`, wraps back to `STATE_MAN_R1_G2`.

---

## 5. `fsm_setting` (`Core/Inc/fsm_setting.h`, `Core/Src/fsm_setting.c`)

Allows tuning `Y1_dur`, `G1_dur`, `Y2_dur`, and `G2_dur` in a working copy (`temp_config`) while blinking the active LED at 2 Hz (`TICK_250MS`). Red durations (`R1_dur`, `R2_dur`) are automatically derived on `CONFIRM` so the two roads stay synchronized.

### States & Transitions
1. **`STATE_SETTING_EDIT_Y1`** *(Initial state on `fsm_setting_enter()`)*
   - **Entry**: Sets `*road_1 = TL_AMBER`, `*road_2 = TL_OFF`, `temp_config = *config`, `active_field = EDIT_Y1`, `draft_duration = temp_config.Y1_dur`.
   - **Run**:
     - `SWITCH_LIGHT` $\rightarrow$ `STATE_SETTING_EDIT_G1` (discards unconfirmed edits on next entry).
     - `TICK_250MS` $\rightarrow$ Toggles `*road_1` between `TL_AMBER` and `TL_OFF`.
     - `INCREASE` / `DECREASE` $\rightarrow$ Adjusts `temp_config.Y1_dur` with wrap-around (`1` $\leftrightarrow$ `99`) and updates `draft_duration`.
     - `CONFIRM` $\rightarrow$ Computes `temp_config.R2_dur = temp_config.Y1_dur + temp_config.G1_dur` and commits `*config = temp_config`.
2. **`STATE_SETTING_EDIT_G1`**
   - **Entry**: Sets `*road_1 = TL_GREEN`, `*road_2 = TL_OFF`, `temp_config = *config`, `active_field = EDIT_G1`, `draft_duration = temp_config.G1_dur`.
   - **Run**: Same behavior for `G1_dur`; `SWITCH_LIGHT` $\rightarrow$ `STATE_SETTING_EDIT_Y2`; `CONFIRM` updates `R2_dur = Y1_dur + G1_dur` and saves `*config`.
3. **`STATE_SETTING_EDIT_Y2`**
   - **Entry**: Sets `*road_1 = TL_OFF`, `*road_2 = TL_AMBER`, `temp_config = *config`, `active_field = EDIT_Y2`, `draft_duration = temp_config.Y2_dur`.
   - **Run**: Same behavior for `Y2_dur`; `SWITCH_LIGHT` $\rightarrow$ `STATE_SETTING_EDIT_G2`; `CONFIRM` updates `R1_dur = Y2_dur + G2_dur` and saves `*config`.
4. **`STATE_SETTING_EDIT_G2`**
   - **Entry**: Sets `*road_1 = TL_OFF`, `*road_2 = TL_GREEN`, `temp_config = *config`, `active_field = EDIT_G2`, `draft_duration = temp_config.G2_dur`.
   - **Run**: Same behavior for `G2_dur`; `SWITCH_LIGHT` $\rightarrow$ `STATE_SETTING_EDIT_Y1`; `CONFIRM` updates `R1_dur = Y2_dur + G2_dur` and saves `*config`.

### Exit Behavior (`fsm_setting_exit()`)
- Resets `disp_data->active_field = EDIT_NONE` and clears `curr_state = NULL`.
