/*
 * lcd.h - Driver LCD 16x2 (HD44780) qua I2C backpack PCF8574
 * Chi tiet: moi dong LCD duoc dem vao bo dem (shadow buffer); lcd_flush_task()
 * chi ghi ra I2C khi noi dung dong thay doi, moi lan goi toi da 1 dong
 * -> moi lan chay < 10 ms, khong lam tre scheduler.
 */
#ifndef INC_LCD_H_
#define INC_LCD_H_

#include "main.h"
#include <stdint.h>

#define LCD_COLS          16
#define LCD_ROWS          2
#define LCD_DEFAULT_ADDR  0x27   /* dia chi 7-bit: SUA theo ket qua I2C scan (Buoc 3.4) */

/* Goi 1 lan trong main(), TRUOC khi bat timer/scheduler (co dung HAL_Delay). */
void    lcd_init(I2C_HandleTypeDef *hi2c, uint8_t addr7);

/* Ghi chuoi vao bo dem dong `row` (0/1); tu cat/chen khoang trang cho du 16 ky tu. */
void    lcd_put_line(uint8_t row, const char *str);

/* Task scheduler, de xuat chu ky 50 ms (5 tick): day bo dem ra man hinh. */
void    lcd_flush_task(void);

void    lcd_clear(void);                 /* blocking ~2 ms, chi dung luc init */
void    lcd_set_backlight(uint8_t on);
uint8_t lcd_is_ok(void);                 /* 1 neu lan giao tiep I2C gan nhat thanh cong */

#endif /* INC_LCD_H_ */