#include "fsm_manual.h"
#include "global.h"

typedef struct
{
    const State_t *curr_state;
    const Traffic_Light_Config_t *config;
    Traffic_Light_State *road_1;
    Traffic_Light_State *road_2;
    Display_Data_t *disp_data;
    uint16_t loc_tim;
} fsm_manual_context_t;

static fsm_manual_context_t manu_ctx = {0};

static const State_t STATE_MAN_R1_G2;
static const State_t STATE_MAN_R1_Y2_BUF;
static const State_t STATE_MAN_G1_R2;
static const State_t STATE_MAN_Y1_R2_BUF;

static void manu_stateTrans(const State_t *next_state);
static void manu_R1_G2_entry(void);
static void manu_R1_G2_run(void);
static void manu_R1_Y2_entry(void);
static void manu_R1_Y2_run(void);
static void manu_G1_R2_entry(void);
static void manu_G1_R2_run(void);
static void manu_Y1_R2_entry(void);
static void manu_Y1_R2_run(void);

void fsm_manual_init(const Traffic_Light_Config_t *cfg,
                     Traffic_Light_State *r1,
                     Traffic_Light_State *r2,
                     Display_Data_t *disp)
{
    manu_ctx.curr_state = NULL;
    manu_ctx.loc_tim = 0;

    manu_ctx.config = cfg;
    manu_ctx.road_1 = r1;
    manu_ctx.road_2 = r2;
    manu_ctx.disp_data = disp;
}

void fsm_manual_enter(void)
{
    if (manu_ctx.disp_data != NULL)
    {
        manu_ctx.disp_data->mode = MODE_MANUAL;
        manu_ctx.disp_data->countdown_road1 = 0;
        manu_ctx.disp_data->countdown_road2 = 0;
    }

    if (*manu_ctx.road_1 == TL_GREEN)
    {
        manu_stateTrans(&STATE_MAN_G1_R2);
    }
    else if (*manu_ctx.road_1 == TL_AMBER)
    {
        manu_stateTrans(&STATE_MAN_Y1_R2_BUF);
    }
    else if (*manu_ctx.road_2 == TL_AMBER)
    {
        manu_stateTrans(&STATE_MAN_R1_Y2_BUF);
    }
    else
    {
        manu_stateTrans(&STATE_MAN_R1_G2);
    }
}

void fsm_manual_run(void)
{
    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_run != NULL)
    {
        manu_ctx.curr_state->on_run();
    }
}

void fsm_manual_exit(void)
{
    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_exit != NULL)
    {
        manu_ctx.curr_state->on_exit();
    }

    manu_ctx.curr_state = NULL;
    manu_ctx.loc_tim = 0;
}

static void manu_stateTrans(const State_t *next_state)
{
    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_exit != NULL)
    {
        manu_ctx.curr_state->on_exit();
    }

    manu_ctx.curr_state = next_state;

    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_entry != NULL)
    {
        manu_ctx.curr_state->on_entry();
    }
}

static void manu_R1_G2_entry(void)
{
    *manu_ctx.road_1 = TL_RED;
    *manu_ctx.road_2 = TL_GREEN;
    if (manu_ctx.disp_data != NULL)
    {
        manu_ctx.disp_data->countdown_road1 = 0;
    }
}

static void manu_R1_G2_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        manu_stateTrans(&STATE_MAN_R1_Y2_BUF);
    }
}

static const State_t STATE_MAN_R1_G2 = {
    .on_entry = manu_R1_G2_entry,
    .on_run = manu_R1_G2_run,
    .on_exit = NULL};

static void manu_R1_Y2_entry(void)
{
    *manu_ctx.road_1 = TL_RED;
    *manu_ctx.road_2 = TL_AMBER;

    // Co dinh den vang sang 3 giay truoc khi tu dong chuyen sang G1_R2
    manu_ctx.loc_tim = MANUAL_AMBER_DUR;
    if (manu_ctx.disp_data != NULL)
    {
        manu_ctx.disp_data->countdown_road1 = manu_ctx.loc_tim;
    }
}

static void manu_R1_Y2_run(void)
{
    if (event.TICK_1S)
    {
        if (manu_ctx.loc_tim > 0 && --manu_ctx.loc_tim == 0)
        {
            manu_stateTrans(&STATE_MAN_G1_R2);
        }
        else if (manu_ctx.disp_data != NULL)
        {
            manu_ctx.disp_data->countdown_road1 = manu_ctx.loc_tim;
        }
    }
}

static const State_t STATE_MAN_R1_Y2_BUF = {
    .on_entry = manu_R1_Y2_entry,
    .on_run = manu_R1_Y2_run,
    .on_exit = NULL};

static void manu_G1_R2_entry(void)
{
    *manu_ctx.road_1 = TL_GREEN;
    *manu_ctx.road_2 = TL_RED;
    if (manu_ctx.disp_data != NULL)
    {
        manu_ctx.disp_data->countdown_road1 = 0;
    }
}

static void manu_G1_R2_run(void)
{
    if (event.SWITCH_LIGHT)
    {
        manu_stateTrans(&STATE_MAN_Y1_R2_BUF);
    }
}

static const State_t STATE_MAN_G1_R2 = {
    .on_entry = manu_G1_R2_entry,
    .on_run = manu_G1_R2_run,
    .on_exit = NULL};

static void manu_Y1_R2_entry(void)
{
    *manu_ctx.road_1 = TL_AMBER;
    *manu_ctx.road_2 = TL_RED;

    // Co dinh den vang sang 3 giay truoc khi tu dong chuyen sang R1_G2
    manu_ctx.loc_tim = MANUAL_AMBER_DUR;
    if (manu_ctx.disp_data != NULL)
    {
        manu_ctx.disp_data->countdown_road1 = manu_ctx.loc_tim;
    }
}

static void manu_Y1_R2_run(void)
{
    if (event.TICK_1S)
    {
        if (manu_ctx.loc_tim > 0 && --manu_ctx.loc_tim == 0)
        {
            manu_stateTrans(&STATE_MAN_R1_G2);
        }
        else if (manu_ctx.disp_data != NULL)
        {
            manu_ctx.disp_data->countdown_road1 = manu_ctx.loc_tim;
        }
    }
}

static const State_t STATE_MAN_Y1_R2_BUF = {
    .on_entry = manu_Y1_R2_entry,
    .on_run = manu_Y1_R2_run,
    .on_exit = NULL};
