/*
 * env_display.h - "Keo" giua DHT20 va LCD cho che do AUTO / MANUAL
 *   env_sample_task : chu ky 2 s  -> gui lenh do, hen one-shot task doc ket qua sau 100 ms
 *   display_task    : chu ky 100 ms -> ve man hinh AUTO/MANUAL (TUNING do fsm_tuning ve)
 */
#ifndef INC_ENV_DISPLAY_H_
#define INC_ENV_DISPLAY_H_

#include <stdint.h>

void env_sample_task(void);
void display_task(void);

int16_t  env_get_temp_x10(void);   /* 0.1 do C */
uint16_t env_get_hum_x10(void);    /* 0.1 %RH  */
uint8_t  env_is_valid(void);

#endif /* INC_ENV_DISPLAY_H_ */