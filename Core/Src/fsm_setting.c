#include "fsm_setting.h"
#include "global.h"

typedef struct
{
    const State_t *curr_state;          // Read-only
    Traffic_Light_Config_t *config;     // Read-Write (committed on exit if synced)
    Traffic_Light_Config_t temp_config; // Working copy confirmed per color
    Traffic_Light_State *road_1;
    Traffic_Light_State *road_2;
    Display_Data_t *disp_data;
} fsm_setting_context_t;

static fsm_setting_context_t setting_ctx = {0};

static const State_t STATE_SETTING_EDIT_RED;
static const State_t STATE_SETTING_EDIT_AMBER;
static const State_t STATE_SETTING_EDIT_GREEN;

static void setting_stateTrans(const State_t *next_state);
static void setting_enter_field(Traffic_Light_State color, Setting_Field field, uint16_t init_dur);
static void setting_run_common(Traffic_Light_State color, uint16_t *dur_r1, uint16_t *dur_r2);
static void setting_edit_red_entry(void);
static void setting_edit_red_run(void);
static void setting_edit_amber_entry(void);
static void setting_edit_amber_run(void);
static void setting_edit_green_entry(void);
static void setting_edit_green_run(void);

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

    // Load a fresh working copy of the saved config when entering Setting mode
    if (setting_ctx.config != NULL)
    {
        setting_ctx.temp_config = *setting_ctx.config;
    }

    // Start Setting mode by editing the Red pair
    setting_stateTrans(&STATE_SETTING_EDIT_RED);
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

    // Commit temp_config only if Red == Amber + Green; otherwise discard it
    if (setting_ctx.config != NULL &&
        setting_ctx.temp_config.R1_dur == (setting_ctx.temp_config.Y1_dur + setting_ctx.temp_config.G1_dur))
    {
        *setting_ctx.config = setting_ctx.temp_config;
    }
    setting_ctx.temp_config = (Traffic_Light_Config_t){0};

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

static void setting_enter_field(Traffic_Light_State color, Setting_Field field, uint16_t init_dur)
{
    // Turn both roads to the active color being edited
    *setting_ctx.road_1 = color;
    *setting_ctx.road_2 = color;

    // Load the active field and its confirmed-so-far duration (drops unconfirmed draft edits)
    setting_ctx.disp_data->active_field = field;
    setting_ctx.disp_data->draft_duration = init_dur;
}

static void setting_run_common(Traffic_Light_State color, uint16_t *dur_r1, uint16_t *dur_r2)
{
    // Blink both roads' active LEDs at 2 Hz (toggle every 250 ms)
    if (event.TICK_250MS)
    {
        Traffic_Light_State next_led = (*setting_ctx.road_1 == color) ? TL_OFF : color;
        *setting_ctx.road_1 = next_led;
        *setting_ctx.road_2 = next_led;
    }

    // Increment draft duration with wrap-around (99 -> 1)
    if (event.INCREASE)
    {
        setting_ctx.disp_data->draft_duration = (setting_ctx.disp_data->draft_duration >= MAX_LIGHT_DUR)
                                                    ? MIN_LIGHT_DUR
                                                    : (setting_ctx.disp_data->draft_duration + 1U);
    }

    // Decrement draft duration with wrap-around (1 -> 99)
    if (event.DECREASE)
    {
        setting_ctx.disp_data->draft_duration = (setting_ctx.disp_data->draft_duration <= MIN_LIGHT_DUR)
                                                    ? MAX_LIGHT_DUR
                                                    : (setting_ctx.disp_data->draft_duration - 1U);
    }

    // Save the draft duration into temp_config for both roads (committed on mode exit if synced)
    if (event.CONFIRM)
    {
        *dur_r1 = setting_ctx.disp_data->draft_duration;
        *dur_r2 = setting_ctx.disp_data->draft_duration;
    }
}

static void setting_edit_red_entry(void)
{
    setting_enter_field(TL_RED, EDIT_RED, setting_ctx.temp_config.R1_dur);
}

static void setting_edit_red_run(void)
{
    // Cycle to Edit Amber when the switch-light button is pressed
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_AMBER);
        return;
    }

    setting_run_common(TL_RED, &setting_ctx.temp_config.R1_dur, &setting_ctx.temp_config.R2_dur);
}

static const State_t STATE_SETTING_EDIT_RED = {.on_entry = setting_edit_red_entry,
                                               .on_run = setting_edit_red_run,
                                               .on_exit = NULL};

static void setting_edit_amber_entry(void)
{
    setting_enter_field(TL_AMBER, EDIT_AMBER, setting_ctx.temp_config.Y1_dur);
}

static void setting_edit_amber_run(void)
{
    // Cycle to Edit Green when the switch-light button is pressed
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_GREEN);
        return;
    }

    setting_run_common(TL_AMBER, &setting_ctx.temp_config.Y1_dur, &setting_ctx.temp_config.Y2_dur);
}

static const State_t STATE_SETTING_EDIT_AMBER = {.on_entry = setting_edit_amber_entry,
                                                 .on_run = setting_edit_amber_run,
                                                 .on_exit = NULL};

static void setting_edit_green_entry(void)
{
    setting_enter_field(TL_GREEN, EDIT_GREEN, setting_ctx.temp_config.G1_dur);
}

static void setting_edit_green_run(void)
{
    // Wrap back to Edit Red when the switch-light button is pressed
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_RED);
        return;
    }

    setting_run_common(TL_GREEN, &setting_ctx.temp_config.G1_dur, &setting_ctx.temp_config.G2_dur);
}

static const State_t STATE_SETTING_EDIT_GREEN = {.on_entry = setting_edit_green_entry,
                                                 .on_run = setting_edit_green_run,
                                                 .on_exit = NULL};
