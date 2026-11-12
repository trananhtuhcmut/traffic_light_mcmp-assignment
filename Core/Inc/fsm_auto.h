/**
 * Public API for fsm_auto
 */
#ifndef INC_FSM_AUTO_H_
#define INC_FSM_AUTO_H_

#include "global.h"

void fsm_auto_init(const Traffic_Light_Config_t *cfg,
                   Traffic_Light_State *r1,
                   Traffic_Light_State *r2,
                   Display_Data_t *disp);
void fsm_auto_enter(void);
void fsm_auto_run(void);
void fsm_auto_exit(void);

#endif