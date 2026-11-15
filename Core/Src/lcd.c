#include "lcd.h"
#include "i2c.h"
#include <string.h>

#define LCD_RS   0x01   /* P0 */
#define LCD_EN   0x04   /* P2 (P1 = RW, noi GND -> luon ghi) */
#define LCD_BL   0x08   /* P3 = den nen */

#define LCD_I2C_TIMEOUT_MS  20
#define DHT20_PROBE_ADDR8   (0x38U << 1) /* Dia chi 8-bit cua DHT20 tren mach that */

static I2C_HandleTypeDef *lcd_i2c;
static uint8_t lcd_addr8;
static uint8_t lcd_bl = LCD_BL;
static uint8_t lcd_ok = 0;
static uint8_t lcd_use_soft_i2c = 0; /* 0: Mach that (Hardware I2C), 1: Proteus (Soft I2C) */

static char    lcd_want [LCD_ROWS][LCD_COLS];
static char    lcd_shown[LCD_ROWS][LCD_COLS];

/* --- Khoi ham Software I2C du phong tu dong cho Proteus (PB8=SCL, PB9=SDA) --- */
#define SCL_H()  (GPIOB->BSRR = GPIO_PIN_8)
#define SCL_L()  (GPIOB->BSRR = ((uint32_t)GPIO_PIN_8 << 16U))
#define SDA_H()  (GPIOB->BSRR = GPIO_PIN_9)
#define SDA_L()  (GPIOB->BSRR = ((uint32_t)GPIO_PIN_9 << 16U))
#define SDA_IN() ((GPIOB->IDR & GPIO_PIN_9) != 0U)

static inline void soft_i2c_delay(void)
{
    for (volatile uint32_t i = 0; i < 10; i++) { __NOP(); }
}

static void soft_i2c_init_pins(void)
{
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    SCL_H();
    SDA_H();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    soft_i2c_delay();
}

static void soft_i2c_restore_hw(void)
{
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);
    MX_I2C1_Init();
}

static uint8_t soft_i2c_write_byte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80U) SDA_H(); else SDA_L();
        soft_i2c_delay();
        SCL_H(); soft_i2c_delay();
        SCL_L();
        data <<= 1;
    }
    SDA_H(); soft_i2c_delay();
    SCL_H(); soft_i2c_delay();
    uint8_t ack = (SDA_IN() == 0U) ? 1U : 0U;
    SCL_L(); soft_i2c_delay();
    return ack;
}

static HAL_StatusTypeDef lcd_tx(uint8_t *buf, uint16_t len)
{
    if (!lcd_use_soft_i2c) {
        if (lcd_i2c == NULL) return HAL_ERROR;
        return HAL_I2C_Master_Transmit(lcd_i2c, lcd_addr8, buf, len, LCD_I2C_TIMEOUT_MS);
    }

    SDA_H(); SCL_H(); soft_i2c_delay();
    SDA_L(); soft_i2c_delay();
    SCL_L(); soft_i2c_delay();

    if (!soft_i2c_write_byte(lcd_addr8)) {
        SDA_L(); soft_i2c_delay(); SCL_H(); soft_i2c_delay(); SDA_H();
        return HAL_ERROR;
    }
    for (uint16_t i = 0; i < len; i++) {
        soft_i2c_write_byte(buf[i]);
    }
    SDA_L(); soft_i2c_delay();
    SCL_H(); soft_i2c_delay();
    SDA_H(); soft_i2c_delay();
    return HAL_OK;
}

static uint8_t lcd_pack(uint8_t *dst, uint8_t val, uint8_t rs)
{
    uint8_t hi = (uint8_t)((val & 0xF0) | lcd_bl | rs);
    uint8_t lo = (uint8_t)(((val << 4) & 0xF0) | lcd_bl | rs);
    dst[0] = hi | LCD_EN;  dst[1] = hi;
    dst[2] = lo | LCD_EN;  dst[3] = lo;
    return 4;
}

static void lcd_init_nibble(uint8_t nib)
{
    uint8_t b[2];
    uint8_t v = (uint8_t)((nib << 4) | lcd_bl);
    b[0] = v | LCD_EN;
    b[1] = v;
    lcd_tx(b, 2);
}

static void lcd_cmd(uint8_t cmd)
{
    uint8_t b[4];
    lcd_pack(b, cmd, 0);
    lcd_tx(b, 4);
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
    HAL_Delay(2);
    memset(lcd_want,  ' ', sizeof lcd_want);
    memset(lcd_shown, ' ', sizeof lcd_shown);
}

void lcd_init(I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    lcd_i2c   = hi2c;
    lcd_addr8 = (uint8_t)(addr7 << 1);

    /* 1. Kiem tra I2C1 phan cung: Neu LCD (0x21) HOAC DHT20 (0x38) phan hoi -> Mach that */
    uint8_t hw_lcd_ok   = (HAL_I2C_IsDeviceReady(hi2c, lcd_addr8, 2, 10) == HAL_OK);
    uint8_t hw_dht20_ok = (HAL_I2C_IsDeviceReady(hi2c, DHT20_PROBE_ADDR8, 2, 10) == HAL_OK);

    if (hw_lcd_ok || hw_dht20_ok) {
        lcd_use_soft_i2c = 0;
        lcd_ok = hw_lcd_ok;
    } else {
        /* 2. Ca 2 deu khong phan hoi tren I2C1 -> Thu Soft-I2C cho Proteus */
        lcd_use_soft_i2c = 1;
        soft_i2c_init_pins();
        if (lcd_tx(NULL, 0) == HAL_OK) {
            lcd_ok = 1; /* Proteus ACK thanh cong */
        } else {
            /* Khong phai Proteus (hoac LCD mat ket noi) -> Khoi phuc lai I2C1 phan cung cho DHT20 */
            soft_i2c_restore_hw();
            lcd_use_soft_i2c = 0;
            lcd_ok = 0;
        }
    }

    HAL_Delay(50);
    lcd_init_nibble(0x03); HAL_Delay(5);
    lcd_init_nibble(0x03); HAL_Delay(1);
    lcd_init_nibble(0x03); HAL_Delay(1);
    lcd_init_nibble(0x02); HAL_Delay(1);

    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_clear();
}

void lcd_set_backlight(uint8_t on)
{
    uint8_t b = lcd_bl = on ? LCD_BL : 0;
    lcd_tx(&b, 1);
}

uint8_t lcd_is_ok(void) { return lcd_ok; }

void lcd_put_line(uint8_t row, const char *str)
{
    uint8_t i = 0;
    if (row >= LCD_ROWS) return;
    for (; i < LCD_COLS && str[i] != '\0'; i++) lcd_want[row][i] = str[i];
    for (; i < LCD_COLS; i++)                   lcd_want[row][i] = ' ';
}

void lcd_flush_task(void)
{
    uint8_t buf[4 * (1 + LCD_COLS)];
    for (uint8_t r = 0; r < LCD_ROWS; r++) {
        if (memcmp(lcd_want[r], lcd_shown[r], LCD_COLS) == 0) continue;

        uint8_t n = lcd_pack(buf, (uint8_t)(0x80 | (r ? 0x40 : 0x00)), 0);
        for (uint8_t c = 0; c < LCD_COLS; c++)
            n += lcd_pack(buf + n, (uint8_t)lcd_want[r][c], LCD_RS);

        if (lcd_tx(buf, n) == HAL_OK) {
            memcpy(lcd_shown[r], lcd_want[r], LCD_COLS);
            lcd_ok = 1;
        } else {
            lcd_ok = 0;
        }
        return;
    }
}
