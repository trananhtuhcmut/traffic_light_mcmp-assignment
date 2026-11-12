/**
 * Implementation for fsm_manual public functions
 * and implementation for fsm_manual private elements
 */

#include "fsm_manual.h"
#include "global.h"

// Private context encapsulating manual FSM state and bound external pointers
typedef struct
{
    const State_t *curr_state;            // Active sub-state descriptor (read-only)
    const Traffic_Light_Config_t *config; // Shared timing config (read-only, used for Y1_dur / Y2_dur)
    Traffic_Light_State *road_1;          // Output pointer for Road 1 light state
    Traffic_Light_State *road_2;          // Output pointer for Road 2 light state
    Display_Data_t *disp_data;            // Output pointer for LCD view model
    uint16_t loc_tim;                     // Amber buffer countdown timer (in seconds)
} fsm_manual_context_t;

static fsm_manual_context_t manu_ctx = {0};

// Forward declarations of sub-state descriptors
static const State_t STATE_MAN_R1_G2;
static const State_t STATE_MAN_R1_Y2_BUF;
static const State_t STATE_MAN_G1_R2;
static const State_t STATE_MAN_Y1_R2_BUF;

// Forward declarations of private state handlers
static void manu_stateTrans(const State_t *next_state);
static void manu_R1_G2_entry(void);
static void manu_R1_G2_run(void);
static void manu_R1_Y2_entry(void);
static void manu_R1_Y2_run(void);
static void manu_G1_R2_entry(void);
static void manu_G1_R2_run(void);
static void manu_Y1_R2_entry(void);
static void manu_Y1_R2_run(void);

// public, callable in main to initialize state machine context before entering the main loop
void fsm_manual_init(const Traffic_Light_Config_t *cfg,
                     Traffic_Light_State *r1,
                     Traffic_Light_State *r2,
                     Display_Data_t *disp)
{
    // Clear active state and timer; state is entered later via fsm_manual_enter()
    manu_ctx.curr_state = NULL;
    manu_ctx.loc_tim = 0;

    // Bind external shared data pointers so state handlers can read config and write outputs
    manu_ctx.config = cfg;
    manu_ctx.road_1 = r1;
    manu_ctx.road_2 = r2;
    manu_ctx.disp_data = disp;
}

// public, scheduled once when entering MODE_MANUAL
void fsm_manual_enter(void)
{
    // Notify the display data struct that the system is now in Manual mode
    if (manu_ctx.disp_data != NULL)
    {
        manu_ctx.disp_data->mode = MODE_MANUAL;
    }

    // Enter the initial sub-state (R1 Red, R2 Green) and trigger its on_entry action
    manu_stateTrans(&STATE_MAN_R1_G2);
}

// public, periodic task when in MODE_MANUAL
void fsm_manual_run(void)
{
    // Delegate execution to the current sub-state's periodic run handler
    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_run != NULL)
    {
        manu_ctx.curr_state->on_run();
    }
}

// public, called once when exiting MODE_MANUAL
void fsm_manual_exit(void)
{
    // Run the active sub-state's exit cleanup if one is registered
    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_exit != NULL)
    {
        manu_ctx.curr_state->on_exit();
    }

    // Detach state pointer and reset countdown so re-entry starts clean
    manu_ctx.curr_state = NULL;
    manu_ctx.loc_tim = 0;
}

// private, state transition helper
static void manu_stateTrans(const State_t *next_state)
{
    // 1. Execute exit action of the outgoing state (if defined)
    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_exit != NULL)
    {
        manu_ctx.curr_state->on_exit();
    }

    // 2. Switch active state pointer to the new state descriptor
    manu_ctx.curr_state = next_state;

    // 3. Execute entry action of the incoming state once to set outputs/timers
    if (manu_ctx.curr_state != NULL && manu_ctx.curr_state->on_entry != NULL)
    {
        manu_ctx.curr_state->on_entry();
    }
}

// private, entry action for R1_G2
static void manu_R1_G2_entry(void)
{
    // Output steady state: Road 1 Red, Road 2 Green
    *manu_ctx.road_1 = TL_RED;
    *manu_ctx.road_2 = TL_GREEN;
}

// private, normal run action for R1_G2
static void manu_R1_G2_run(void)
{
    // On switch button press, move to R1_Y2 buffer instead of switching directly to Green
    if (event.SWITCH_LIGHT)
    {
        manu_stateTrans(&STATE_MAN_R1_Y2_BUF);
    }
}

static const State_t STATE_MAN_R1_G2 = {
    .on_entry = manu_R1_G2_entry,
    .on_run = manu_R1_G2_run,
    .on_exit = NULL};

// private, entry action for R1_Y2_BUF
static void manu_R1_Y2_entry(void)
{
    // Keep Road 1 Red, switch Road 2 from Green to Amber
    *manu_ctx.road_1 = TL_RED;
    *manu_ctx.road_2 = TL_AMBER;

    // Load Road 2's configured Amber duration (in seconds) into local countdown
    manu_ctx.loc_tim = manu_ctx.config->Y2_dur;
}

// private, normal run action for R1_Y2_BUF
static void manu_R1_Y2_run(void)
{
    // Decrement countdown every 1s tick; once it reaches 0, complete switch to G1_R2
    if (event.TICK_1S)
    {
        if (manu_ctx.loc_tim > 0 && --manu_ctx.loc_tim == 0)
        {
            manu_stateTrans(&STATE_MAN_G1_R2);
        }
    }
}

static const State_t STATE_MAN_R1_Y2_BUF = {
    .on_entry = manu_R1_Y2_entry,
    .on_run = manu_R1_Y2_run,
    .on_exit = NULL};

// private, entry action for G1_R2
static void manu_G1_R2_entry(void)
{
    // Output steady state: Road 1 Green, Road 2 Red
    *manu_ctx.road_1 = TL_GREEN;
    *manu_ctx.road_2 = TL_RED;
}

// private, normal run action for G1_R2
static void manu_G1_R2_run(void)
{
    // On switch button press, move to Y1_R2 buffer before giving Road 2 Green
    if (event.SWITCH_LIGHT)
    {
        manu_stateTrans(&STATE_MAN_Y1_R2_BUF);
    }
}

static const State_t STATE_MAN_G1_R2 = {
    .on_entry = manu_G1_R2_entry,
    .on_run = manu_G1_R2_run,
    .on_exit = NULL};

// private, entry action for Y1_R2_BUF
static void manu_Y1_R2_entry(void)
{
    // Switch Road 1 from Green to Amber, keep Road 2 Red
    *manu_ctx.road_1 = TL_AMBER;
    *manu_ctx.road_2 = TL_RED;

    // Load Road 1's configured Amber duration (in seconds) into local countdown
    manu_ctx.loc_tim = manu_ctx.config->Y1_dur;
}

// private, normal run action for Y1_R2_BUF
static void manu_Y1_R2_run(void)
{
    // Decrement countdown every 1s tick; once it reaches 0, complete switch back to R1_G2
    if (event.TICK_1S)
    {
        if (manu_ctx.loc_tim > 0 && --manu_ctx.loc_tim == 0)
        {
            manu_stateTrans(&STATE_MAN_R1_G2);
        }
    }
}

static const State_t STATE_MAN_Y1_R2_BUF = {
    .on_entry = manu_Y1_R2_entry,
    .on_run = manu_Y1_R2_run,
    .on_exit = NULL};