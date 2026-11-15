#include "lcd.h"
#include "i2c.h"
#include <string.h>

#define LCD_RS   0x01   /* P0 */
#define LCD_EN   0x04   /* P2 (P1 = RW, noi GND -> luon ghi) */
#define LCD_BL   0x08   /* P3 = den nen */

#define LCD_I2C_TIMEOUT_MS  20

static I2C_HandleTypeDef *lcd_i2c;
static uint8_t lcd_addr8;
static uint8_t lcd_bl = LCD_BL;
static uint8_t lcd_ok = 0;
static uint8_t lcd_use_soft_i2c = 0; /* 0: Hardware I2C, 1: Soft I2C */

static char    lcd_want [LCD_ROWS][LCD_COLS];
static char    lcd_shown[LCD_ROWS][LCD_COLS];

/* --- Khoi ham Software I2C tren PB8=SCL, PB9=SDA --- */
#define SCL_H()  (GPIOB->BSRR = GPIO_PIN_8)
#define SCL_L()  (GPIOB->BSRR = ((uint32_t)GPIO_PIN_8 << 16U))
#define SDA_H()  (GPIOB->BSRR = GPIO_PIN_9)
#define SDA_L()  (GPIOB->BSRR = ((uint32_t)GPIO_PIN_9 << 16U))
#define SDA_IN() ((GPIOB->IDR & GPIO_PIN_9) != 0U)

static inline void soft_i2c_delay(void)
{
    for (volatile uint32_t i = 0; i < 10; i++) { __NOP(); }
}

/* Chuyen tam thoi PB8/PB9 sang GPIO Open-Drain de phat xung Soft-I2C */
static void soft_i2c_acquire_bus(void)
{
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

/* Tra PB8/PB9 ve che do Hardware I2C1 (AF_OD) ngay sau khi truyen xong de DHT20 su dung */
static void soft_i2c_release_bus(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    __HAL_RCC_I2C1_CLK_ENABLE();
}

static void soft_i2c_stop(void)
{
    SDA_L(); soft_i2c_delay();
    SCL_H(); soft_i2c_delay();
    SDA_H(); soft_i2c_delay();
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

    /* Muon bus PB8/PB9 sang Soft-I2C trong giao dich nay */
    soft_i2c_acquire_bus();

    /* START condition */
    SDA_H(); SCL_H(); soft_i2c_delay();
    SDA_L(); soft_i2c_delay();
    SCL_L(); soft_i2c_delay();

    /* Gui byte dia chi va kiem tra ACK */
    if (!soft_i2c_write_byte(lcd_addr8)) {
        soft_i2c_stop();
        soft_i2c_release_bus();
        return HAL_ERROR;
    }

    /* Gui tung byte du lieu va kiem tra ACK cua MOI byte (Sua Loi 1) */
    for (uint16_t i = 0; i < len; i++) {
        if (!soft_i2c_write_byte(buf[i])) {
            soft_i2c_stop();
            soft_i2c_release_bus();
            return HAL_ERROR;
        }
    }

    /* STOP condition & tra lai bus cho Hardware I2C1 (Sua Loi 3) */
    soft_i2c_stop();
    soft_i2c_release_bus();
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

    /* Thu Hardware I2C1 truoc; neu khong phan hoi (Proteus) thi dung Soft-I2C theo tung goi */
    if (hi2c != NULL && HAL_I2C_IsDeviceReady(hi2c, lcd_addr8, 2, 10) == HAL_OK) {
        lcd_use_soft_i2c = 0;
        lcd_ok = 1;
    } else {
        lcd_use_soft_i2c = 1;
        lcd_ok = (lcd_tx(NULL, 0) == HAL_OK) ? 1U : 0U;
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