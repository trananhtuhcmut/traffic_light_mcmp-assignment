#include "dht20.h"
#include "i2c.h"

#define DHT20_ADDR8      ((uint8_t)(DHT20_I2C_ADDR << 1))   /* = 0x70 cho HAL */
#define DHT20_TIMEOUT_MS 20

static I2C_HandleTypeDef *dht20_i2c;

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

static DHT20_Status dht20_reset_register(uint8_t reg)
{
    uint8_t cmd[3] = { reg, 0x00, 0x00 };
    uint8_t rx[3]  = { 0x00, 0x00, 0x00 };
    HAL_StatusTypeDef st;

    if (dht20_i2c == NULL)
        return DHT20_ERR_I2C;

    st = HAL_I2C_Master_Transmit(dht20_i2c, DHT20_ADDR8, cmd, 3, DHT20_TIMEOUT_MS);
    if (st != HAL_OK) { I2C1_Recover_If_Stuck(st); return DHT20_ERR_I2C; }
    HAL_Delay(5);

    st = HAL_I2C_Master_Receive(dht20_i2c, DHT20_ADDR8, rx, 3, DHT20_TIMEOUT_MS);
    if (st != HAL_OK) { I2C1_Recover_If_Stuck(st); return DHT20_ERR_I2C; }
    HAL_Delay(10);

    cmd[0] = (uint8_t)(0xB0 | reg);
    cmd[1] = rx[1];
    cmd[2] = rx[2];
    st = HAL_I2C_Master_Transmit(dht20_i2c, DHT20_ADDR8, cmd, 3, DHT20_TIMEOUT_MS);
    if (st != HAL_OK) { I2C1_Recover_If_Stuck(st); return DHT20_ERR_I2C; }

    return DHT20_OK;
}

DHT20_Status dht20_init(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd = 0x71, status = 0;
    HAL_StatusTypeDef st;
    if (hi2c == NULL)
        return DHT20_ERR_I2C;
    dht20_i2c = hi2c;

    HAL_Delay(100);
    st = HAL_I2C_Master_Transmit(hi2c, DHT20_ADDR8, &cmd, 1, DHT20_TIMEOUT_MS);
    if (st != HAL_OK) { I2C1_Recover_If_Stuck(st); return DHT20_ERR_I2C; }

    st = HAL_I2C_Master_Receive(hi2c, DHT20_ADDR8, &status, 1, DHT20_TIMEOUT_MS);
    if (st != HAL_OK) { I2C1_Recover_If_Stuck(st); return DHT20_ERR_I2C; }

    if ((status & 0x18) != 0x18) {
        if (dht20_reset_register(0x1B) != DHT20_OK) return DHT20_ERR_I2C;
        if (dht20_reset_register(0x1C) != DHT20_OK) return DHT20_ERR_I2C;
        if (dht20_reset_register(0x1E) != DHT20_OK) return DHT20_ERR_I2C;
        HAL_Delay(10);
    }
    return DHT20_OK;
}

DHT20_Status dht20_trigger(void)
{
    uint8_t cmd[3] = { 0xAC, 0x33, 0x00 };
    if (dht20_i2c == NULL)
        return DHT20_ERR_I2C;

    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(dht20_i2c, DHT20_ADDR8, cmd, 3, DHT20_TIMEOUT_MS);
    if (st != HAL_OK) {
        I2C1_Recover_If_Stuck(st);
        return DHT20_ERR_I2C;
    }
    return DHT20_OK;
}

DHT20_Status dht20_read(int16_t *temp_x10, uint16_t *hum_x10)
{
    uint8_t rx[7] = {0};
    if (dht20_i2c == NULL || temp_x10 == NULL || hum_x10 == NULL)
        return DHT20_ERR_I2C;

    HAL_StatusTypeDef st = HAL_I2C_Master_Receive(dht20_i2c, DHT20_ADDR8, rx, 7, DHT20_TIMEOUT_MS);
    if (st != HAL_OK) {
        I2C1_Recover_If_Stuck(st);
        return DHT20_ERR_I2C;
    }
    if (rx[0] & 0x80)               return DHT20_ERR_BUSY;
    if (dht20_crc8(rx, 6) != rx[6]) return DHT20_ERR_CRC;

    uint32_t raw_h = ((uint32_t)rx[1] << 12) | ((uint32_t)rx[2] << 4) | (rx[3] >> 4);
    uint32_t raw_t = ((uint32_t)(rx[3] & 0x0F) << 16) | ((uint32_t)rx[4] << 8) | rx[5];

    *hum_x10  = (uint16_t)(((raw_h * 1000UL) + (1UL << 19)) >> 20);
    *temp_x10 = (int16_t)((int32_t)(((raw_t * 2000UL) + (1UL << 19)) >> 20) - 500);
    return DHT20_OK;
}
