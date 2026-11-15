#include "fsm_auto.h"
#include "global.h"

typedef struct
{
    const State_t *curr_state;            // Read-only
    const Traffic_Light_Config_t *config; // Read-only
    Traffic_Light_State *road_1;
    Traffic_Light_State *road_2;
    Display_Data_t *disp_data;
    uint16_t loc_tim; // in seconds, loaded with Green duration or Yellow duration depending on the state
} fsm_auto_context_t;

static fsm_auto_context_t auto_ctx = {0};

static const State_t STATE_AUTO_R1_G2;
static const State_t STATE_AUTO_R1_Y2;
static const State_t STATE_AUTO_G1_R2;
static const State_t STATE_AUTO_Y1_R2;

static void auto_stateTrans(const State_t *next_state);
static void auto_R1_G2_entry(void);
static void auto_R1_G2_run(void);
static void auto_R1_Y2_entry(void);
static void auto_R1_Y2_run(void);
static void auto_G1_R2_entry(void);
static void auto_G1_R2_run(void);
static void auto_Y1_R2_entry(void);
static void auto_Y1_R2_run(void);

void fsm_auto_init(const Traffic_Light_Config_t *cfg,
                   Traffic_Light_State *r1,
                   Traffic_Light_State *r2,
                   Display_Data_t *disp)
{
    // Reset internal FSM state and local timer
    auto_ctx.curr_state = NULL;
    auto_ctx.loc_tim = 0;

    // Bind external configuration, road light outputs, and display data pointers
    auto_ctx.config = cfg;
    auto_ctx.road_1 = r1;
    auto_ctx.road_2 = r2;
    auto_ctx.disp_data = disp;
}

void fsm_auto_enter(void)
{
    // Notify the display module that the system is in Automatic mode
    if (auto_ctx.disp_data != NULL)
    {
        auto_ctx.disp_data->mode = MODE_AUTO;
    }

    // Always start Automatic mode with Road 1 Red and Road 2 Green
    auto_stateTrans(&STATE_AUTO_R1_G2);
}

void fsm_auto_run(void)
{
    // Dispatch execution to the active state's run handler
    if (auto_ctx.curr_state != NULL && auto_ctx.curr_state->on_run != NULL)
    {
        auto_ctx.curr_state->on_run();
    }
}

void fsm_auto_exit(void)
{
    // Run the active state's exit cleanup if defined
    if (auto_ctx.curr_state != NULL && auto_ctx.curr_state->on_exit != NULL)
    {
        auto_ctx.curr_state->on_exit();
    }

    // Clear state pointer and timer when leaving Automatic mode
    auto_ctx.curr_state = NULL;
    auto_ctx.loc_tim = 0;
}

static void auto_stateTrans(const State_t *next_state)
{
    // 1. Execute exit action of the outgoing state (if any)
    if (auto_ctx.curr_state != NULL && auto_ctx.curr_state->on_exit != NULL)
    {
        auto_ctx.curr_state->on_exit();
    }

    // 2. Update current state pointer to the new state
    auto_ctx.curr_state = next_state;

    // 3. Execute entry action of the incoming state (if any)
    if (auto_ctx.curr_state != NULL && auto_ctx.curr_state->on_entry != NULL)
    {
        auto_ctx.curr_state->on_entry();
    }
}

static void auto_R1_G2_entry(void)
{
    // Set Road 1 to Red and Road 2 to Green
    *auto_ctx.road_1 = TL_RED;
    *auto_ctx.road_2 = TL_GREEN;

    // Load local state timer with Road 2's Green duration
    auto_ctx.loc_tim = auto_ctx.config->G2_dur;

    // Initialize display countdowns for Road 1 (full Red phase) and Road 2 (Green phase)
    auto_ctx.disp_data->countdown_road1 = auto_ctx.config->R1_dur;
    auto_ctx.disp_data->countdown_road2 = auto_ctx.config->G2_dur;
}

static void auto_R1_G2_run(void)
{
    // Process countdown and state transition on each 1-second tick
    if (event.TICK_1S)
    {
        // Decrement Green timer; transition to Road 2 Yellow when it reaches 0
        if (auto_ctx.loc_tim > 0 && --auto_ctx.loc_tim == 0)
        {
            // Keep Road 1's Red countdown ticking across the Green-to-Yellow transition
            auto_ctx.disp_data->countdown_road1--;
            auto_stateTrans(&STATE_AUTO_R1_Y2);
        }
        else
        {
            // Update remaining seconds for both roads on the display
            auto_ctx.disp_data->countdown_road1--;
            auto_ctx.disp_data->countdown_road2 = auto_ctx.loc_tim;
        }
    }
}

static const State_t STATE_AUTO_R1_G2 = {.on_entry = auto_R1_G2_entry,
                                         .on_run = auto_R1_G2_run,
                                         .on_exit = NULL};

static void auto_R1_Y2_entry(void)
{
    // Switch Road 2 to Yellow while Road 1 remains Red
    *auto_ctx.road_2 = TL_AMBER;

    // Load local state timer with Road 2's Yellow duration
    auto_ctx.loc_tim = auto_ctx.config->Y2_dur;

    // Update Road 2's display countdown for the Yellow phase
    auto_ctx.disp_data->countdown_road2 = auto_ctx.config->Y2_dur;
}

static void auto_R1_Y2_run(void)
{
    // Process countdown and state transition on each 1-second tick
    if (event.TICK_1S)
    {
        // Decrement Yellow timer; switch to Road 1 Green / Road 2 Red when it reaches 0
        if (auto_ctx.loc_tim > 0 && --auto_ctx.loc_tim == 0)
        {
            auto_ctx.disp_data->countdown_road1--;
            auto_stateTrans(&STATE_AUTO_G1_R2);
        }
        else
        {
            // Update remaining seconds for both roads on the display
            auto_ctx.disp_data->countdown_road1--;
            auto_ctx.disp_data->countdown_road2 = auto_ctx.loc_tim;
        }
    }
}

static const State_t STATE_AUTO_R1_Y2 = {.on_entry = auto_R1_Y2_entry,
                                         .on_run = auto_R1_Y2_run,
                                         .on_exit = NULL};

static void auto_G1_R2_entry(void)
{
    // Set Road 1 to Green and Road 2 to Red
    *auto_ctx.road_1 = TL_GREEN;
    *auto_ctx.road_2 = TL_RED;

    // Load local state timer with Road 1's Green duration
    auto_ctx.loc_tim = auto_ctx.config->G1_dur;

    // Initialize display countdowns for Road 1 (Green phase) and Road 2 (full Red phase)
    auto_ctx.disp_data->countdown_road1 = auto_ctx.config->G1_dur;
    auto_ctx.disp_data->countdown_road2 = auto_ctx.config->R2_dur;
}

static void auto_G1_R2_run(void)
{
    // Process countdown and state transition on each 1-second tick
    if (event.TICK_1S)
    {
        // Decrement Green timer; transition to Road 1 Yellow when it reaches 0
        if (auto_ctx.loc_tim > 0 && --auto_ctx.loc_tim == 0)
        {
            // Keep Road 2's Red countdown ticking across the Green-to-Yellow transition
            auto_ctx.disp_data->countdown_road2--;
            auto_stateTrans(&STATE_AUTO_Y1_R2);
        }
        else
        {
            // Update remaining seconds for both roads on the display
            auto_ctx.disp_data->countdown_road1 = auto_ctx.loc_tim;
            auto_ctx.disp_data->countdown_road2--;
        }
    }
}

static const State_t STATE_AUTO_G1_R2 = {.on_entry = auto_G1_R2_entry,
                                         .on_run = auto_G1_R2_run,
                                         .on_exit = NULL};

static void auto_Y1_R2_entry(void)
{
    // Switch Road 1 to Yellow while Road 2 remains Red
    *auto_ctx.road_1 = TL_AMBER;

    // Load local state timer with Road 1's Yellow duration
    auto_ctx.loc_tim = auto_ctx.config->Y1_dur;

    // Update Road 1's display countdown for the Yellow phase
    auto_ctx.disp_data->countdown_road1 = auto_ctx.config->Y1_dur;
}

static void auto_Y1_R2_run(void)
{
    // Process countdown and state transition on each 1-second tick
    if (event.TICK_1S)
    {
        // Decrement Yellow timer; wrap back to Road 1 Red / Road 2 Green when it reaches 0
        if (auto_ctx.loc_tim > 0 && --auto_ctx.loc_tim == 0)
        {
            auto_stateTrans(&STATE_AUTO_R1_G2);
        }
        else
        {
            // Update remaining seconds for both roads on the display
            auto_ctx.disp_data->countdown_road1 = auto_ctx.loc_tim;
            auto_ctx.disp_data->countdown_road2--;
        }
    }
}

static const State_t STATE_AUTO_Y1_R2 = {.on_entry = auto_Y1_R2_entry,
                                         .on_run = auto_Y1_R2_run,
                                         .on_exit = NULL};