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
    setting_ctx.curr_state = NULL;
    setting_ctx.temp_config = (Traffic_Light_Config_t){0};

    setting_ctx.config = cfg;
    setting_ctx.road_1 = r1;
    setting_ctx.road_2 = r2;
    setting_ctx.disp_data = disp;
}

void fsm_setting_enter(void)
{
    if (setting_ctx.disp_data != NULL)
    {
        setting_ctx.disp_data->mode = MODE_SETTING;
        setting_ctx.disp_data->invalid_ticks = 0;
    }

    if (setting_ctx.config != NULL)
    {
        setting_ctx.temp_config = *setting_ctx.config;
    }

    setting_stateTrans(&STATE_SETTING_EDIT_RED);
}

void fsm_setting_run(void)
{
    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_run != NULL)
    {
        setting_ctx.curr_state->on_run();
    }
}

void fsm_setting_exit(void)
{
    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_exit != NULL)
    {
        setting_ctx.curr_state->on_exit();
    }

    // Kiem tra R == Y + G: neu dung thi luu, neu sai thi bao INVALID va giu nguyen config cu
    if (setting_ctx.config != NULL)
    {
        if (setting_ctx.temp_config.R1_dur == (setting_ctx.temp_config.Y1_dur + setting_ctx.temp_config.G1_dur))
        {
            *setting_ctx.config = setting_ctx.temp_config;
        }
        else if (setting_ctx.disp_data != NULL)
        {
            setting_ctx.disp_data->invalid_ticks = 10U; // Hien "INVALID" trong 10 x 100ms = 1 giay
        }
    }
    setting_ctx.temp_config = (Traffic_Light_Config_t){0};

    if (setting_ctx.disp_data != NULL)
    {
        setting_ctx.disp_data->active_field = EDIT_NONE;
    }
    setting_ctx.curr_state = NULL;
}

static void setting_stateTrans(const State_t *next_state)
{
    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_exit != NULL)
    {
        setting_ctx.curr_state->on_exit();
    }

    setting_ctx.curr_state = next_state;

    if (setting_ctx.curr_state != NULL && setting_ctx.curr_state->on_entry != NULL)
    {
        setting_ctx.curr_state->on_entry();
    }
}

static void setting_enter_field(Traffic_Light_State color, Setting_Field field, uint16_t init_dur)
{
    *setting_ctx.road_1 = color;
    *setting_ctx.road_2 = color;

    if (setting_ctx.disp_data != NULL)
    {
        setting_ctx.disp_data->active_field = field;
        setting_ctx.disp_data->draft_duration = init_dur;
        setting_ctx.disp_data->set_ticks = 0;	//For set button
    }
}

static void setting_run_common(Traffic_Light_State color, uint16_t *dur_r1, uint16_t *dur_r2)
{
    if (event.TICK_250MS)
    {
        Traffic_Light_State next_led = (*setting_ctx.road_1 == color) ? TL_OFF : color;
        *setting_ctx.road_1 = next_led;
        *setting_ctx.road_2 = next_led;
    }

    if (event.INCREASE)
    {
        setting_ctx.disp_data->draft_duration = (setting_ctx.disp_data->draft_duration >= MAX_LIGHT_DUR)
                                                    ? MIN_LIGHT_DUR
                                                    : (setting_ctx.disp_data->draft_duration + 1U);
    }

    if (event.DECREASE)
    {
        setting_ctx.disp_data->draft_duration = (setting_ctx.disp_data->draft_duration <= MIN_LIGHT_DUR)
                                                    ? MAX_LIGHT_DUR
                                                    : (setting_ctx.disp_data->draft_duration - 1U);
    }

    if (event.CONFIRM)
    {
        *dur_r1 = setting_ctx.disp_data->draft_duration;
        *dur_r2 = setting_ctx.disp_data->draft_duration;

        if (setting_ctx.disp_data != NULL)
                {
                    setting_ctx.disp_data->set_ticks = 10U;
                }
    }
}

static void setting_edit_red_entry(void)
{
    setting_enter_field(TL_RED, EDIT_RED, setting_ctx.temp_config.R1_dur);
}

static void setting_edit_red_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_AMBER);
        return;
    }
    setting_run_common(TL_RED, &setting_ctx.temp_config.R1_dur, &setting_ctx.temp_config.R2_dur);
}

static const State_t STATE_SETTING_EDIT_RED = {
    .on_entry = setting_edit_red_entry,
    .on_run = setting_edit_red_run,
    .on_exit = NULL};

static void setting_edit_amber_entry(void)
{
    setting_enter_field(TL_AMBER, EDIT_AMBER, setting_ctx.temp_config.Y1_dur);
}

static void setting_edit_amber_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_GREEN);
        return;
    }
    setting_run_common(TL_AMBER, &setting_ctx.temp_config.Y1_dur, &setting_ctx.temp_config.Y2_dur);
}

static const State_t STATE_SETTING_EDIT_AMBER = {
    .on_entry = setting_edit_amber_entry,
    .on_run = setting_edit_amber_run,
    .on_exit = NULL};

static void setting_edit_green_entry(void)
{
    setting_enter_field(TL_GREEN, EDIT_GREEN, setting_ctx.temp_config.G1_dur);
}

static void setting_edit_green_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_RED);
        return;
    }
    setting_run_common(TL_GREEN, &setting_ctx.temp_config.G1_dur, &setting_ctx.temp_config.G2_dur);
}

static const State_t STATE_SETTING_EDIT_GREEN = {
    .on_entry = setting_edit_green_entry,
    .on_run = setting_edit_green_run,
    .on_exit = NULL};
