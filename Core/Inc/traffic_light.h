/*
 * traffic_light.h
 *
 *  Created on: Nov 6, 2026
 *      Author: trananhtu
 */
#ifndef INC_TRAFFIC_LIGHT_H_
#define INC_TRAFFIC_LIGHT_H_

#include <stdint.h>

/* Trang thai den giao thong */
typedef enum {
    TL_OFF   = 0,  // 00
    TL_AMBER = 1,  // 01
    TL_GREEN = 2,  // 10
    TL_RED   = 3   // 11
} TrafficLightState;

/* road: 1 hoac 2 */
void traffic_light_init(void);
void traffic_set(uint8_t road, TrafficLightState state);
TrafficLightState traffic_get(uint8_t road);

#endif /* INC_TRAFFIC_LIGHT_H_ */
