#include "fsm_setting.h"
#include "global.h"

#define SETTING_MSG_TICKS 20U /* 20 x 100ms = 2 giay hien "SET!" / "INVALID" */

typedef struct
{
    const State_t *curr_state;          // Read-only
    Traffic_Light_Config_t *config;     // Read-Write (committed on exit if synced)
    Traffic_Light_Config_t temp_config; // Working copy confirmed per field
    Traffic_Light_State *road_1;
    Traffic_Light_State *road_2;
    Display_Data_t *disp_data;
} fsm_setting_context_t;

static fsm_setting_context_t setting_ctx = {0};

static const State_t STATE_SETTING_EDIT_R1;
static const State_t STATE_SETTING_EDIT_Y1;
static const State_t STATE_SETTING_EDIT_G1;
static const State_t STATE_SETTING_EDIT_R2;
static const State_t STATE_SETTING_EDIT_Y2;
static const State_t STATE_SETTING_EDIT_G2;

static void setting_stateTrans(const State_t *next_state);
static void setting_enter_field(uint8_t road_id, Traffic_Light_State color, Setting_Field field, uint16_t init_dur);
static void setting_run_common(uint8_t road_id, Traffic_Light_State color, uint16_t *target_dur);

static void setting_edit_r1_entry(void);
static void setting_edit_r1_run(void);
static void setting_edit_y1_entry(void);
static void setting_edit_y1_run(void);
static void setting_edit_g1_entry(void);
static void setting_edit_g1_run(void);
static void setting_edit_r2_entry(void);
static void setting_edit_r2_run(void);
static void setting_edit_y2_entry(void);
static void setting_edit_y2_run(void);
static void setting_edit_g2_entry(void);
static void setting_edit_g2_run(void);

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
        setting_ctx.disp_data->set_ticks = 0;
    }

    if (setting_ctx.config != NULL)
    {
        setting_ctx.temp_config = *setting_ctx.config;
    }

    setting_stateTrans(&STATE_SETTING_EDIT_R1);
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

    /* Kiem tra dong bo cheo: R1 == G2 + Y2 VA R2 == G1 + Y1 */
    if (setting_ctx.config != NULL)
    {
        bool r1_ok = (setting_ctx.temp_config.R1_dur ==
                      (setting_ctx.temp_config.G2_dur + setting_ctx.temp_config.Y2_dur));
        bool r2_ok = (setting_ctx.temp_config.R2_dur ==
                      (setting_ctx.temp_config.G1_dur + setting_ctx.temp_config.Y1_dur));

        if (r1_ok && r2_ok)
        {
            *setting_ctx.config = setting_ctx.temp_config;
        }
        else if (setting_ctx.disp_data != NULL)
        {
            setting_ctx.disp_data->invalid_ticks = SETTING_MSG_TICKS;
        }
    }
    setting_ctx.temp_config = (Traffic_Light_Config_t){0};

    if (setting_ctx.disp_data != NULL)
    {
        setting_ctx.disp_data->active_field = EDIT_NONE;
        setting_ctx.disp_data->set_ticks = 0;
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

static void setting_enter_field(uint8_t road_id, Traffic_Light_State color, Setting_Field field, uint16_t init_dur)
{
    /* Chi bat LED cua duong dang chinh, tat duong con lai de phan biet ro R1/R2 */
    if (road_id == 1U)
    {
        *setting_ctx.road_1 = color;
        *setting_ctx.road_2 = TL_OFF;
    }
    else
    {
        *setting_ctx.road_1 = TL_OFF;
        *setting_ctx.road_2 = color;
    }

    if (setting_ctx.disp_data != NULL)
    {
        setting_ctx.disp_data->active_field = field;
        setting_ctx.disp_data->draft_duration = init_dur;
        setting_ctx.disp_data->set_ticks = 0;
    }
}

static void setting_run_common(uint8_t road_id, Traffic_Light_State color, uint16_t *target_dur)
{
    if (event.TICK_250MS)
    {
        Traffic_Light_State *active_road = (road_id == 1U) ? setting_ctx.road_1 : setting_ctx.road_2;
        Traffic_Light_State *other_road  = (road_id == 1U) ? setting_ctx.road_2 : setting_ctx.road_1;

        *active_road = (*active_road == color) ? TL_OFF : color;
        *other_road  = TL_OFF;
    }

    if (setting_ctx.disp_data == NULL)
    {
        return;
    }

    if (event.INCREASE)
    {
        setting_ctx.disp_data->draft_duration = (setting_ctx.disp_data->draft_duration >= MAX_LIGHT_DUR)
                                                    ? MIN_LIGHT_DUR
                                                    : (setting_ctx.disp_data->draft_duration + 1U);
        setting_ctx.disp_data->set_ticks = 0;
    }

    if (event.DECREASE)
    {
        setting_ctx.disp_data->draft_duration = (setting_ctx.disp_data->draft_duration <= MIN_LIGHT_DUR)
                                                    ? MAX_LIGHT_DUR
                                                    : (setting_ctx.disp_data->draft_duration - 1U);
        setting_ctx.disp_data->set_ticks = 0;
    }

    if (event.CONFIRM)
    {
        *target_dur = setting_ctx.disp_data->draft_duration;
        setting_ctx.disp_data->set_ticks = SETTING_MSG_TICKS;
    }
}

/* --- ROAD 1: R1 -> Y1 -> G1 --- */
static void setting_edit_r1_entry(void)
{
    setting_enter_field(1U, TL_RED, EDIT_R1, setting_ctx.temp_config.R1_dur);
}

static void setting_edit_r1_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_Y1);
        return;
    }
    setting_run_common(1U, TL_RED, &setting_ctx.temp_config.R1_dur);
}

static const State_t STATE_SETTING_EDIT_R1 = {
    .on_entry = setting_edit_r1_entry,
    .on_run   = setting_edit_r1_run,
    .on_exit  = NULL};

static void setting_edit_y1_entry(void)
{
    setting_enter_field(1U, TL_AMBER, EDIT_Y1, setting_ctx.temp_config.Y1_dur);
}

static void setting_edit_y1_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_G1);
        return;
    }
    setting_run_common(1U, TL_AMBER, &setting_ctx.temp_config.Y1_dur);
}

static const State_t STATE_SETTING_EDIT_Y1 = {
    .on_entry = setting_edit_y1_entry,
    .on_run   = setting_edit_y1_run,
    .on_exit  = NULL};

static void setting_edit_g1_entry(void)
{
    setting_enter_field(1U, TL_GREEN, EDIT_G1, setting_ctx.temp_config.G1_dur);
}

static void setting_edit_g1_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_R2);
        return;
    }
    setting_run_common(1U, TL_GREEN, &setting_ctx.temp_config.G1_dur);
}

static const State_t STATE_SETTING_EDIT_G1 = {
    .on_entry = setting_edit_g1_entry,
    .on_run   = setting_edit_g1_run,
    .on_exit  = NULL};

/* --- ROAD 2: R2 -> Y2 -> G2 --- */
static void setting_edit_r2_entry(void)
{
    setting_enter_field(2U, TL_RED, EDIT_R2, setting_ctx.temp_config.R2_dur);
}

static void setting_edit_r2_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_Y2);
        return;
    }
    setting_run_common(2U, TL_RED, &setting_ctx.temp_config.R2_dur);
}

static const State_t STATE_SETTING_EDIT_R2 = {
    .on_entry = setting_edit_r2_entry,
    .on_run   = setting_edit_r2_run,
    .on_exit  = NULL};

static void setting_edit_y2_entry(void)
{
    setting_enter_field(2U, TL_AMBER, EDIT_Y2, setting_ctx.temp_config.Y2_dur);
}

static void setting_edit_y2_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_G2);
        return;
    }
    setting_run_common(2U, TL_AMBER, &setting_ctx.temp_config.Y2_dur);
}

static const State_t STATE_SETTING_EDIT_Y2 = {
    .on_entry = setting_edit_y2_entry,
    .on_run   = setting_edit_y2_run,
    .on_exit  = NULL};

static void setting_edit_g2_entry(void)
{
    setting_enter_field(2U, TL_GREEN, EDIT_G2, setting_ctx.temp_config.G2_dur);
}

static void setting_edit_g2_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        setting_stateTrans(&STATE_SETTING_EDIT_R1);
        return;
    }
    setting_run_common(2U, TL_GREEN, &setting_ctx.temp_config.G2_dur);
}

static const State_t STATE_SETTING_EDIT_G2 = {
    .on_entry = setting_edit_g2_entry,
    .on_run   = setting_edit_g2_run,
    .on_exit  = NULL};
