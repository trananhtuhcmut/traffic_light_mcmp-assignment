/*
 * dht20.h - Driver cam bien nhiet do/do am DHT20 (I2C, dia chi 7-bit 0x38)
 * Driver KHONG block 80 ms: tach thanh trigger() va read(); lop tren (env_display)
 * dung one-shot task cua scheduler de goi read() sau >= 80 ms.
 */
#ifndef INC_DHT20_H_
#define INC_DHT20_H_

#include "main.h"
#include <stdint.h>

#define DHT20_I2C_ADDR   0x38        /* 7-bit */

typedef enum {
    DHT20_OK = 0,
    DHT20_ERR_I2C,                   /* khong giao tiep duoc (NACK/timeout) */
    DHT20_ERR_BUSY,                  /* dang do, chua xong */
    DHT20_ERR_CRC                    /* sai CRC-8 */
} DHT20_Status;

/* Goi 1 lan trong main() truoc scheduler (co HAL_Delay 100 ms theo datasheet). */
DHT20_Status dht20_init(I2C_HandleTypeDef *hi2c);

/* Gui lenh do 0xAC 0x33 0x00. Sau do phai doi >= 80 ms roi moi goi dht20_read(). */
DHT20_Status dht20_trigger(void);

/* Doc 7 byte, kiem tra busy + CRC, doi sang don vi 0.1 do C va 0.1 %RH (so nguyen, khong dung float). */
DHT20_Status dht20_read(int16_t *temp_x10, uint16_t *hum_x10);

#endif /* INC_DHT20_H_ */