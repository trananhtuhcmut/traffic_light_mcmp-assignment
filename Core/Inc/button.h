/*
 * button.h
 *
 *  Created on: Nov 8, 2026
 *      Author: trananhtu
 */
#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include <stdint.h>

/*
 * 5 buttons:
 *   0 = Button1_1 (PA8, D7)
 *   1 = Button1_2 (PA9, D8)
 *   2 = Button2_1 (PC7, D9)
 *   3 = Button2_2 (PB6, D10)
 *   4 = B1 on-board (PC13)
 */
#define NUM_BUTTONS 5U

/* Call every 10ms as a scheduler task */
void button_init(void);
void button_reading(void);

/* Returns 1 once per press (edge-detect, cleared after read) */
uint8_t is_button_pressed(uint8_t index);

/* Returns 1 repeatedly every 1s while held (cleared after read) */
uint8_t is_button_long_pressed(uint8_t index);

#endif /* INC_BUTTON_H_ */
