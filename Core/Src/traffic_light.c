/*
 * traffic_light.c
 *
 *  Created on: Nov 6, 2026
 *      Author: trananhtu
 */

#include "main.h"
#include "traffic_light.h"

/*
 * Bang to hop tin hieu (Muc 3.2):
 *   Signal A  Signal B  Trang thai
 *      0         0        OFF
 *      0         1        AMBER
 *      1         0        GREEN
 *      1         1        RED
 *
 * Traffic Light 1: LED1_1 (PB3) = Signal A, LED1_2 (PB6) = Signal B
 * Traffic Light 2: LED2_1 (PA8) = Signal A, LED2_2 (PA9) = Signal B
 */

/* Luu trang thai hien tai cua tung duong */
static Traffic_Light_State current_state[2] = {TL_OFF, TL_OFF};

void traffic_light_init(void)
{
    traffic_set(1, TL_OFF);
    traffic_set(2, TL_OFF);
}

void traffic_set(uint8_t road, Traffic_Light_State state)
{
    GPIO_PinState sigA = (state >> 1) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET;
    GPIO_PinState sigB = (state) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET;

    if (road == 1)
    {
        HAL_GPIO_WritePin(LED1_1_GPIO_Port, LED1_1_Pin, sigA);
        HAL_GPIO_WritePin(LED1_2_GPIO_Port, LED1_2_Pin, sigB);
        current_state[0] = state;
    }
    else if (road == 2)
    {
        HAL_GPIO_WritePin(LED2_1_GPIO_Port, LED2_1_Pin, sigA);
        HAL_GPIO_WritePin(LED2_2_GPIO_Port, LED2_2_Pin, sigB);
        current_state[1] = state;
    }
}

Traffic_Light_State traffic_get(uint8_t road)
{
    if (road == 1)
        return current_state[0];
    if (road == 2)
        return current_state[1];
    return TL_OFF;
}
