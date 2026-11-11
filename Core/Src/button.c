/*
 * button.c
 *
 *  Created on: Nov 8, 2026
 *      Author: trananhtu
 */

#include "button.h"
#include "main.h"

#define BUTTON_IS_PRESSED   GPIO_PIN_RESET
#define BUTTON_IS_RELEASED  GPIO_PIN_SET

/* Duration for long press detection: 100 x 10ms = 1 second */
#define LONG_PRESS_DURATION 100

/*
 * Pin mapping table (must match .ioc labels):
 *   0 = Button1_1 (PA8)
 *   1 = Button1_2 (PA9)
 *   2 = Button2_1 (PC7)
 *   3 = Button2_2 (PB6)
 *   4 = B1        (PC13)
 */
static GPIO_TypeDef* button_ports[NUM_BUTTONS] = {
    Button1_1_GPIO_Port,
    Button1_2_GPIO_Port,
    Button2_1_GPIO_Port,
    Button2_2_GPIO_Port,
    B1_GPIO_Port
};

static uint16_t button_pins[NUM_BUTTONS] = {
    Button1_1_Pin,
    Button1_2_Pin,
    Button2_1_Pin,
    Button2_2_Pin,
    B1_Pin
};

/* Debounce: 2 consecutive readings must match */
static GPIO_PinState debounce_buf1[NUM_BUTTONS];
static GPIO_PinState debounce_buf2[NUM_BUTTONS];
static GPIO_PinState button_state[NUM_BUTTONS];

/* Edge detection (press event) */
static GPIO_PinState prev_state[NUM_BUTTONS];
static uint8_t       press_flag[NUM_BUTTONS];

/* Long press detection */
static uint16_t long_press_counter[NUM_BUTTONS];
static uint8_t  long_press_flag[NUM_BUTTONS];

void button_init(void) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        debounce_buf1[i]      = BUTTON_IS_RELEASED;
        debounce_buf2[i]      = BUTTON_IS_RELEASED;
        button_state[i]       = BUTTON_IS_RELEASED;
        prev_state[i]         = BUTTON_IS_RELEASED;
        press_flag[i]         = 0;
        long_press_counter[i] = 0;
        long_press_flag[i]    = 0;
    }
}

/* Called every 10ms by the scheduler */
void button_reading(void) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        /* Shift debounce buffers */
        debounce_buf2[i] = debounce_buf1[i];
        debounce_buf1[i] = HAL_GPIO_ReadPin(button_ports[i], button_pins[i]);

        /* Accept new state only if 2 consecutive readings agree */
        if (debounce_buf1[i] == debounce_buf2[i]) {
            button_state[i] = debounce_buf1[i];
        }

        /* Edge detection: pressed event (falling edge) */
        if (button_state[i] == BUTTON_IS_PRESSED && prev_state[i] == BUTTON_IS_RELEASED) {
            press_flag[i] = 1;
        }
        prev_state[i] = button_state[i];

        /* Long press: count while held, fire every 1s */
        if (button_state[i] == BUTTON_IS_PRESSED) {
            long_press_counter[i]++;
            if (long_press_counter[i] >= LONG_PRESS_DURATION) {
                long_press_flag[i] = 1;
                long_press_counter[i] = 0; /* Reset to fire again after next 1s */
            }
        } else {
            long_press_counter[i] = 0;
            long_press_flag[i] = 0;
        }
    }
}

uint8_t is_button_pressed(uint8_t index) {
    if (index >= NUM_BUTTONS) return 0;
    if (press_flag[index]) {
        press_flag[index] = 0;
        return 1;
    }
    return 0;
}

uint8_t is_button_long_pressed(uint8_t index) {
    if (index >= NUM_BUTTONS) return 0;
    if (long_press_flag[index]) {
        long_press_flag[index] = 0;
        return 1;
    }
    return 0;
}
