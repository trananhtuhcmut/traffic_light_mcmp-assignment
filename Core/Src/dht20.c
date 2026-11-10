#include "dht20.h"

#define DHT20_ADDR8      ((uint8_t)(DHT20_I2C_ADDR << 1))   /* = 0x70 cho HAL */
#define DHT20_TIMEOUT_MS 20

static I2C_HandleTypeDef *dht20_i2c;

/* CRC-8, da thuc x^8 + x^5 + x^4 + 1 (0x31), khoi tao 0xFF - theo datasheet DHT20 */
static uint8_t dht20_crc8(const uint8_t *d, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= d[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

/* Khoi tao lai thanh ghi 0x1B/0x1C/0x1E (chi can khi trang thai != 0x18) */
static void dht20_reset_register(uint8_t reg)
{
    uint8_t cmd[3] = { reg, 0x00, 0x00 };
    uint8_t rx[3];
    HAL_I2C_Master_Transmit(dht20_i2c, DHT20_ADDR8, cmd, 3, DHT20_TIMEOUT_MS);
    HAL_Delay(5);
    HAL_I2C_Master_Receive(dht20_i2c, DHT20_ADDR8, rx, 3, DHT20_TIMEOUT_MS);
    HAL_Delay(10);
    cmd[0] = (uint8_t)(0xB0 | reg);
    cmd[1] = rx[1];
    cmd[2] = rx[2];
    HAL_I2C_Master_Transmit(dht20_i2c, DHT20_ADDR8, cmd, 3, DHT20_TIMEOUT_MS);
}

DHT20_Status dht20_init(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd = 0x71, status = 0;
    dht20_i2c = hi2c;

    HAL_Delay(100);                                    /* >= 100 ms sau khi cap nguon */
    if (HAL_I2C_Master_Transmit(hi2c, DHT20_ADDR8, &cmd, 1, DHT20_TIMEOUT_MS) != HAL_OK)
        return DHT20_ERR_I2C;
    if (HAL_I2C_Master_Receive(hi2c, DHT20_ADDR8, &status, 1, DHT20_TIMEOUT_MS) != HAL_OK)
        return DHT20_ERR_I2C;

    if ((status & 0x18) != 0x18) {                     /* chua hieu chuan -> khoi tao thanh ghi */
        dht20_reset_register(0x1B);
        dht20_reset_register(0x1C);
        dht20_reset_register(0x1E);
        HAL_Delay(10);
    }
    return DHT20_OK;
}

DHT20_Status dht20_trigger(void)
{
    uint8_t cmd[3] = { 0xAC, 0x33, 0x00 };
    if (HAL_I2C_Master_Transmit(dht20_i2c, DHT20_ADDR8, cmd, 3, DHT20_TIMEOUT_MS) != HAL_OK)
        return DHT20_ERR_I2C;
    return DHT20_OK;
}

DHT20_Status dht20_read(int16_t *temp_x10, uint16_t *hum_x10)
{
    uint8_t rx[7];
    if (HAL_I2C_Master_Receive(dht20_i2c, DHT20_ADDR8, rx, 7, DHT20_TIMEOUT_MS) != HAL_OK)
        return DHT20_ERR_I2C;
    if (rx[0] & 0x80)                    return DHT20_ERR_BUSY;   /* bit 7 = busy */
    if (dht20_crc8(rx, 6) != rx[6])      return DHT20_ERR_CRC;

    /* rx[1..3] cao: do am 20 bit; rx[3] thap..rx[5]: nhiet do 20 bit */
    uint32_t raw_h = ((uint32_t)rx[1] << 12) | ((uint32_t)rx[2] << 4) | (rx[3] >> 4);
    uint32_t raw_t = ((uint32_t)(rx[3] & 0x0F) << 16) | ((uint32_t)rx[4] << 8) | rx[5];

    /* RH[%] = raw_h / 2^20 * 100      ->  x10: raw_h * 1000 / 2^20
     * T[C]  = raw_t / 2^20 * 200 - 50 ->  x10: raw_t * 2000 / 2^20 - 500
     * Cong 2^19 truoc khi dich 20 bit de lam tron (thay vi cat). raw*2000 toi da ~2.1e9 < 2^32
     * nen dung uint32 khong tran, khong can float (printf %f mac dinh tat trong newlib-nano). */
    *hum_x10  = (uint16_t)(((raw_h * 1000UL) + (1UL << 19)) >> 20);
    *temp_x10 = (int16_t)((int32_t)(((raw_t * 2000UL) + (1UL << 19)) >> 20) - 500);
    return DHT20_OK;
}