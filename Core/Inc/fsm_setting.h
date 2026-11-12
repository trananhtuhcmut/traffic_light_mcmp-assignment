/**
 * Public API for fsm_setting
 */
#ifndef INC_FSM_SETTING_H_
#define INC_FSM_SETTING_H_

#include "global.h"

void fsm_setting_init(Traffic_Light_Config_t *cfg,
                      Traffic_Light_State *r1,
                      Traffic_Light_State *r2,
                      Display_Data_t *disp);
void fsm_setting_enter(void);
void fsm_setting_run(void);
void fsm_setting_exit(void);

#endif /* INC_FSM_SETTING_H_ */