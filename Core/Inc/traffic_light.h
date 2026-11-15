/*
 * traffic_light.h
 *
 *  Created on: Nov 6, 2026
 *      Author: trananhtu, nguyenlekhaitu
 */
#ifndef INC_TRAFFIC_LIGHT_H_
#define INC_TRAFFIC_LIGHT_H_

#include <stdint.h>
#include "global.h"

/* road: 1 hoac 2 */
void traffic_light_init(void);
void traffic_set(uint8_t road, Traffic_Light_State state);
Traffic_Light_State traffic_get(uint8_t road);

#endif /* INC_TRAFFIC_LIGHT_H_ */
