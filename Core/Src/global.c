#include "global.h"

System_Mode curr_mode = MODE_AUTO;

Traffic_Light_Config_t curr_light_config = {
    .R1_dur = DEFAULT_RED,   .R2_dur = DEFAULT_RED,
    .Y1_dur = DEFAULT_AMBER, .Y2_dur = DEFAULT_AMBER,
    .G1_dur = DEFAULT_GREEN, .G2_dur = DEFAULT_GREEN
};

System_Event_t event = {0};
Display_Data_t curr_disp_data = {0};
