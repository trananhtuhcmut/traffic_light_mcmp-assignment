#include "fsm_setting.h"
#include "global.h"

typedef struct
{
    const State_t *curr_state;          // Read-only
    Traffic_Light_Config_t *config;     // Read-Write
    Traffic_Light_Config_t temp_config; // Working copy while editing
    Traffic_Light_State *road_1;
    Traffic_Light_State *road_2;
    Display_Data_t *disp_data;
} fsm_setting_context_t;

static fsm_setting_context_t setting_ctx = {0};

static const State_t STATE_SETTING_EDIT_Y1;
static const State_t STATE_SETTING_EDIT_G1;
static const State_t STATE_SETTING_EDIT_Y2;
static const State_t STATE_SETTING_EDIT_G2;

static void setting_stateTrans(const State_t *next_state);
static void setting_edit_Y1_entry(void);
static void setting_edit_Y1_run(void);
static void setting_edit_G1_entry(void);
static void setting_edit_G1_run(void);
static void setting_edit_Y2_entry(void);
static void setting_edit_Y2_run(void);
static void setting_edit_G2_entry(void);
static void setting_edit_G2_run(void);

void fsm_setting_init(Traffic_Light_Config_t *cfg,
                      Traffic_Light_State *r1,
                      Traffic_Light_State *r2,
                      Display_Data_t *disp)
{
    // Reset internal FSM state and clear temporary configuration
    setting_ctx.curr_state = NULL;
    setting_ctx.temp_config = (Traffic_Light_Config_t){0};

    // Bind external configuration, road light outputs, and display data pointers
    setting_ctx.config = cfg;
    setting_ctx.road_1 = r1;
    setting_ctx.road_2 = r2;
    setting_ctx.disp_data = disp;
}

void fsm_setting_enter(void)
{
    // Notify the display module that the system is in Setting mode
    if (setting_ctx.disp_data != NULL)
    {
        setting_ctx.disp_data->mode = MODE_SETTING;
    }

    // Always start Setting mode by editing Road 1 Yellow (Y1)
    setting_stateTrans(&STATE_SETTING_EDIT_Y1);
}

void fsm_setting_run(void)
{
    // Dispatch execution to the active state's run handler
    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_run != NULL)
    {
        setting_ctx.curr_state->on_run();
    }
}

void fsm_setting_exit(void)
{
    // Run the active state's exit cleanup if defined
    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_exit != NULL)
    {
        setting_ctx.curr_state->on_exit();
    }

    // Clear active editing field and state pointer when leaving Setting mode
    if (setting_ctx.disp_data != NULL)
    {
        setting_ctx.disp_data->active_field = EDIT_NONE;
    }
    setting_ctx.curr_state = NULL;
}

static void setting_stateTrans(const State_t *next_state)
{
    // 1. Execute exit action of the outgoing state (if any)
    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_exit != NULL)
    {
        setting_ctx.curr_state->on_exit();
    }

    // 2. Update current state pointer to the new state
    setting_ctx.curr_state = next_state;

    // 3. Execute entry action of the incoming state (if any)
    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_entry != NULL)
    {
        setting_ctx.curr_state->on_entry();
    }
}

static void setting_edit_Y1_entry(void)
{
    // Turn on Road 1 Yellow and turn off Road 2
    *setting_ctx.road_1 = TL_AMBER;
    *setting_ctx.road_2 = TL_OFF;

    // Reload temporary config from current saved config (discards unconfirmed edits)
    setting_ctx.temp_config = *setting_ctx.config;

    // Update display with the active field (Y1) and its starting draft duration
    setting_ctx.disp_data->active_field = EDIT_Y1;
    setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.Y1_dur;
}

static void setting_edit_Y1_run(void)
{
    // Move to Edit G1 when the switch-light button is pressed
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_G1);
        return;
    }

    // Blink Road 1 Yellow LED at 2 Hz (toggle every 250 ms)
    if (event.TICK_250MS)
    {
        *setting_ctx.road_1 = (*setting_ctx.road_1 == TL_AMBER) ? TL_OFF : TL_AMBER;
    }

    // Increment temporary Y1 duration with wrap-around (99 -> 1)
    if (event.INCREASE)
    {
        setting_ctx.temp_config.Y1_dur = (setting_ctx.temp_config.Y1_dur >= MAX_LIGHT_DUR)
                                             ? MIN_LIGHT_DUR
                                             : (setting_ctx.temp_config.Y1_dur + 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.Y1_dur;
    }

    // Decrement temporary Y1 duration with wrap-around (1 -> 99)
    if (event.DECREASE)
    {
        setting_ctx.temp_config.Y1_dur = (setting_ctx.temp_config.Y1_dur <= MIN_LIGHT_DUR)
                                             ? MAX_LIGHT_DUR
                                             : (setting_ctx.temp_config.Y1_dur - 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.Y1_dur;
    }

    // Recalculate R2 duration (Y1 + G1) and commit temporary config on confirm
    if (event.CONFIRM)
    {
        setting_ctx.temp_config.R2_dur = setting_ctx.temp_config.Y1_dur + setting_ctx.temp_config.G1_dur;
        *setting_ctx.config = setting_ctx.temp_config;
    }
}

static const State_t STATE_SETTING_EDIT_Y1 = {.on_entry = setting_edit_Y1_entry,
                                              .on_run = setting_edit_Y1_run,
                                              .on_exit = NULL};

static void setting_edit_G1_entry(void)
{
    // Turn on Road 1 Green and turn off Road 2
    *setting_ctx.road_1 = TL_GREEN;
    *setting_ctx.road_2 = TL_OFF;

    // Reload temporary config from current saved config (discards unconfirmed edits)
    setting_ctx.temp_config = *setting_ctx.config;

    // Update display with the active field (G1) and its starting draft duration
    setting_ctx.disp_data->active_field = EDIT_G1;
    setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.G1_dur;
}

static void setting_edit_G1_run(void)
{
    // Move to Edit Y2 when the switch-light button is pressed
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_Y2);
        return;
    }

    // Blink Road 1 Green LED at 2 Hz (toggle every 250 ms)
    if (event.TICK_250MS)
    {
        *setting_ctx.road_1 = (*setting_ctx.road_1 == TL_GREEN) ? TL_OFF : TL_GREEN;
    }

    // Increment temporary G1 duration with wrap-around (99 -> 1)
    if (event.INCREASE)
    {
        setting_ctx.temp_config.G1_dur = (setting_ctx.temp_config.G1_dur >= MAX_LIGHT_DUR)
                                             ? MIN_LIGHT_DUR
                                             : (setting_ctx.temp_config.G1_dur + 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.G1_dur;
    }

    // Decrement temporary G1 duration with wrap-around (1 -> 99)
    if (event.DECREASE)
    {
        setting_ctx.temp_config.G1_dur = (setting_ctx.temp_config.G1_dur <= MIN_LIGHT_DUR)
                                             ? MAX_LIGHT_DUR
                                             : (setting_ctx.temp_config.G1_dur - 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.G1_dur;
    }

    // Recalculate R2 duration (Y1 + G1) and commit temporary config on confirm
    if (event.CONFIRM)
    {
        setting_ctx.temp_config.R2_dur = setting_ctx.temp_config.Y1_dur + setting_ctx.temp_config.G1_dur;
        *setting_ctx.config = setting_ctx.temp_config;
    }
}

static const State_t STATE_SETTING_EDIT_G1 = {.on_entry = setting_edit_G1_entry,
                                              .on_run = setting_edit_G1_run,
                                              .on_exit = NULL};

static void setting_edit_Y2_entry(void)
{
    // Turn off Road 1 and turn on Road 2 Yellow
    *setting_ctx.road_1 = TL_OFF;
    *setting_ctx.road_2 = TL_AMBER;

    // Reload temporary config from current saved config (discards unconfirmed edits)
    setting_ctx.temp_config = *setting_ctx.config;

    // Update display with the active field (Y2) and its starting draft duration
    setting_ctx.disp_data->active_field = EDIT_Y2;
    setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.Y2_dur;
}

static void setting_edit_Y2_run(void)
{
    // Move to Edit G2 when the switch-light button is pressed
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_G2);
        return;
    }

    // Blink Road 2 Yellow LED at 2 Hz (toggle every 250 ms)
    if (event.TICK_250MS)
    {
        *setting_ctx.road_2 = (*setting_ctx.road_2 == TL_AMBER) ? TL_OFF : TL_AMBER;
    }

    // Increment temporary Y2 duration with wrap-around (99 -> 1)
    if (event.INCREASE)
    {
        setting_ctx.temp_config.Y2_dur = (setting_ctx.temp_config.Y2_dur >= MAX_LIGHT_DUR)
                                             ? MIN_LIGHT_DUR
                                             : (setting_ctx.temp_config.Y2_dur + 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.Y2_dur;
    }

    // Decrement temporary Y2 duration with wrap-around (1 -> 99)
    if (event.DECREASE)
    {
        setting_ctx.temp_config.Y2_dur = (setting_ctx.temp_config.Y2_dur <= MIN_LIGHT_DUR)
                                             ? MAX_LIGHT_DUR
                                             : (setting_ctx.temp_config.Y2_dur - 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.Y2_dur;
    }

    // Recalculate R1 duration (Y2 + G2) and commit temporary config on confirm
    if (event.CONFIRM)
    {
        setting_ctx.temp_config.R1_dur = setting_ctx.temp_config.Y2_dur + setting_ctx.temp_config.G2_dur;
        *setting_ctx.config = setting_ctx.temp_config;
    }
}

static const State_t STATE_SETTING_EDIT_Y2 = {.on_entry = setting_edit_Y2_entry,
                                              .on_run = setting_edit_Y2_run,
                                              .on_exit = NULL};

static void setting_edit_G2_entry(void)
{
    // Turn off Road 1 and turn on Road 2 Green
    *setting_ctx.road_1 = TL_OFF;
    *setting_ctx.road_2 = TL_GREEN;

    // Reload temporary config from current saved config (discards unconfirmed edits)
    setting_ctx.temp_config = *setting_ctx.config;

    // Update display with the active field (G2) and its starting draft duration
    setting_ctx.disp_data->active_field = EDIT_G2;
    setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.G2_dur;
}

static void setting_edit_G2_run(void)
{
    // Wrap back to Edit Y1 when the switch-light button is pressed
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_Y1);
        return;
    }

    // Blink Road 2 Green LED at 2 Hz (toggle every 250 ms)
    if (event.TICK_250MS)
    {
        *setting_ctx.road_2 = (*setting_ctx.road_2 == TL_GREEN) ? TL_OFF : TL_GREEN;
    }

    // Increment temporary G2 duration with wrap-around (99 -> 1)
    if (event.INCREASE)
    {
        setting_ctx.temp_config.G2_dur = (setting_ctx.temp_config.G2_dur >= MAX_LIGHT_DUR)
                                             ? MIN_LIGHT_DUR
                                             : (setting_ctx.temp_config.G2_dur + 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.G2_dur;
    }

    // Decrement temporary G2 duration with wrap-around (1 -> 99)
    if (event.DECREASE)
    {
        setting_ctx.temp_config.G2_dur = (setting_ctx.temp_config.G2_dur <= MIN_LIGHT_DUR)
                                             ? MAX_LIGHT_DUR
                                             : (setting_ctx.temp_config.G2_dur - 1U);
        setting_ctx.disp_data->draft_duration = setting_ctx.temp_config.G2_dur;
    }

    // Recalculate R1 duration (Y2 + G2) and commit temporary config on confirm
    if (event.CONFIRM)
    {
        setting_ctx.temp_config.R1_dur = setting_ctx.temp_config.Y2_dur + setting_ctx.temp_config.G2_dur;
        *setting_ctx.config = setting_ctx.temp_config;
    }
}

static const State_t STATE_SETTING_EDIT_G2 = {.on_entry = setting_edit_G2_entry,
                                              .on_run = setting_edit_G2_run,
                                              .on_exit = NULL};
