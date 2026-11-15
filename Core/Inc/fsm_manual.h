/**
 * Public API for fsm_manual
 */

#ifndef INC_FSM_MANUAL_H_
#define INC_FSM_MANUAL_H_

#include "global.h"

void fsm_manual_init(const Traffic_Light_Config_t *cfg,
                     Traffic_Light_State *r1,
                     Traffic_Light_State *r2,
                     Display_Data_t *disp);
void fsm_manual_enter(void);
void fsm_manual_run(void);
void fsm_manual_exit(void);

#endif