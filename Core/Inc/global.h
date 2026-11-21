#ifndef INC_GLOBAL_H_
#define INC_GLOBAL_H_

// Includes
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Defines
#define SCHEDULER_TICK_RATE 100U                 // 100 Hz
#define SCHEDULER_TICK_MS 10U                    // 10 ms
#define TICK_PER_SEC (1000U / SCHEDULER_TICK_MS) // 100 ticks = 1s

#define NUM_ROADS 2U
#define NUM_BUTTONS 5U

// In seconds, common to all 3 LEDs
#define MIN_LIGHT_DUR 1U
#define MAX_LIGHT_DUR 99U

// default config
#define DEFAULT_RED 5U
#define DEFAULT_AMBER 2U
#define DEFAULT_GREEN 3U

// Manual mode fixed Amber buffer duration (3 seconds)

// System state/mode
typedef enum
{
    MODE_AUTO = 0,
    MODE_MANUAL,
    MODE_SETTING
} System_Mode;

// Abstract light state encoding
typedef enum
{
    TL_OFF = 0,   // 00
    TL_AMBER = 1, // 01
    TL_GREEN = 2, // 10
    TL_RED = 3    // 11
} Traffic_Light_State;

// Data structure to save traffic light durations
typedef struct
{
    uint16_t R1_dur;
    uint16_t R2_dur;
    uint16_t Y1_dur;
    uint16_t Y2_dur;
    uint16_t G1_dur;
    uint16_t G2_dur;
} Traffic_Light_Config_t;

// Which light is currently being edited
typedef enum
{
    EDIT_NONE = 0,
    EDIT_R1,
    EDIT_Y1,
    EDIT_G1,
    EDIT_R2,
    EDIT_Y2,
    EDIT_G2
} Setting_Field;

typedef struct
{
    bool NO_EVENT;
    bool TICK_1S;
    bool TICK_250MS; // for blinking LEDs while in MODE_SETTING
    bool MODE_CHANGE;
    bool SWITCH_LIGHT;
    bool INCREASE;
    bool DECREASE;
    bool CONFIRM;
} System_Event_t;

typedef enum
{
    NO_ERROR = 0
} System_Error;

// Struct to store LCD display data
typedef struct
{
    /* --- 1. Mode & FSM Data --- */
    System_Mode mode;
    uint16_t countdown_road1;
    uint16_t countdown_road2;
    Setting_Field active_field; // Which light is being edited
    uint16_t draft_duration;    // Current number being entered
    uint8_t invalid_ticks;      // Countdown ticks (x100ms) to show "INVALID" on LCD
    uint8_t set_ticks;  		//For set button
    /* --- 2. Environment (DHT20) Data --- */
    int16_t temp_x10;
    uint16_t hum_x10;
    bool env_valid;
} Display_Data_t;

// Common state struct (containing state logics)
typedef struct
{
    void (*on_entry)(void);
    void (*on_run)(void);
    void (*on_exit)(void);
} State_t;

extern System_Mode curr_mode;
extern Traffic_Light_Config_t curr_light_config;
extern System_Event_t event;
extern Display_Data_t curr_disp_data;

#endif /* INC_GLOBAL_H_ */
