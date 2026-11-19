#include "lcd.h"
#include "i2c.h"
#include <string.h>

#define LCD_RS   0x01   /* P0 */
#define LCD_EN   0x04   /* P2 (P1 = RW, noi GND -> luon ghi) */
#define LCD_BL   0x08   /* P3 = den nen */

#define LCD_I2C_TIMEOUT_MS  20
#define LCD_PROBE_INTERVAL  4U /* 4 x 50ms = 200ms kiem tra ket noi khi chu khong doi */

typedef enum {
    LCD_ST_OFFLINE = 0,
    LCD_ST_INIT_1,
    LCD_ST_INIT_2,
    LCD_ST_READY
} lcd_state_t;

static I2C_HandleTypeDef *lcd_i2c;
static uint8_t lcd_addr8;
static uint8_t lcd_bl = LCD_BL;
static uint8_t lcd_ok = 0;
static uint8_t lcd_use_soft_i2c = 0;
static lcd_state_t lcd_state = LCD_ST_OFFLINE;
static uint8_t lcd_probe_cnt = 0;

static char    lcd_want [LCD_ROWS][LCD_COLS];
static char    lcd_shown[LCD_ROWS][LCD_COLS];

/* --- Khoi ham Software I2C tren PB8=SCL, PB9=SDA (chi dung khi Proteus) --- */
#define SCL_H()  (GPIOB->BSRR = GPIO_PIN_8)
#define SCL_L()  (GPIOB->BSRR = ((uint32_t)GPIO_PIN_8 << 16U))
#define SDA_H()  (GPIOB->BSRR = GPIO_PIN_9)
#define SDA_L()  (GPIOB->BSRR = ((uint32_t)GPIO_PIN_9 << 16U))
#define SDA_IN() ((GPIOB->IDR & GPIO_PIN_9) != 0U)

static inline void soft_i2c_delay(void)
{
    for (volatile uint32_t i = 0; i < 10; i++) { __NOP(); }
}

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
        HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(lcd_i2c, lcd_addr8, buf, len, LCD_I2C_TIMEOUT_MS);
        if (st != HAL_OK) {
            I2C1_Recover_If_Stuck(st);
        }
        return st;
    }

    soft_i2c_acquire_bus();

    SDA_H(); SCL_H(); soft_i2c_delay();
    SDA_L(); soft_i2c_delay();
    SCL_L(); soft_i2c_delay();

    if (!soft_i2c_write_byte(lcd_addr8)) {
        soft_i2c_stop();
        soft_i2c_release_bus();
        return HAL_ERROR;
    }
    for (uint16_t i = 0; i < len; i++) {
        if (!soft_i2c_write_byte(buf[i])) {
            soft_i2c_stop();
            soft_i2c_release_bus();
            return HAL_ERROR;
        }
    }

    soft_i2c_stop();
    soft_i2c_release_bus();
    return HAL_OK;
}

static HAL_StatusTypeDef lcd_probe(void)
{
    if (!lcd_use_soft_i2c) {
        if (lcd_i2c == NULL) return HAL_ERROR;
        HAL_StatusTypeDef st = HAL_I2C_IsDeviceReady(lcd_i2c, lcd_addr8, 1, 10);
        if (st != HAL_OK) {
            I2C1_Recover_If_Stuck(st);
        }
        return st;
    }
    return lcd_tx(NULL, 0);
}

static uint8_t lcd_pack(uint8_t *dst, uint8_t val, uint8_t rs)
{
    uint8_t hi = (uint8_t)((val & 0xF0) | lcd_bl | rs);
    uint8_t lo = (uint8_t)(((val << 4) & 0xF0) | lcd_bl | rs);
    dst[0] = hi | LCD_EN;  dst[1] = hi;
    dst[2] = lo | LCD_EN;  dst[3] = lo;
    return 4;
}

static HAL_StatusTypeDef lcd_init_nibble(uint8_t nib)
{
    uint8_t b[2];
    uint8_t v = (uint8_t)((nib << 4) | lcd_bl);
    b[0] = v | LCD_EN;
    b[1] = v;
    return lcd_tx(b, 2);
}

static HAL_StatusTypeDef lcd_cmd(uint8_t cmd)
{
    uint8_t b[4];
    lcd_pack(b, cmd, 0);
    return lcd_tx(b, 4);
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

    /* Kiem tra tren Hardware I2C1 truoc (LCD 0x21 hoac DHT20 0x38) */
    uint8_t hw_lcd_ok   = (hi2c != NULL && HAL_I2C_IsDeviceReady(hi2c, lcd_addr8, 2, 10) == HAL_OK);
    uint8_t hw_dht20_ok = (hi2c != NULL && HAL_I2C_IsDeviceReady(hi2c, (0x38U << 1), 2, 10) == HAL_OK);

    if (hw_lcd_ok || hw_dht20_ok) {
        lcd_use_soft_i2c = 0;
        lcd_ok = hw_lcd_ok;
    } else {
        /* Thu Soft-I2C cho Proteus; neu van khong ACK thi quay ve Hardware I2C */
        lcd_use_soft_i2c = 1;
        if (lcd_tx(NULL, 0) == HAL_OK) {
            lcd_ok = 1;
        } else {
            lcd_use_soft_i2c = 0;
            lcd_ok = 0;
        }
    }

    memset(lcd_want, ' ', sizeof lcd_want);
    memset(lcd_shown, 0, sizeof lcd_shown);

    if (!lcd_ok) {
        lcd_state = LCD_ST_OFFLINE;
        return;
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
    lcd_state = LCD_ST_READY;
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
    /* Buoc 1: Neu LCD dang mat ket noi -> cho cam lai */
    if (lcd_state == LCD_ST_OFFLINE) {
        if (lcd_probe() == HAL_OK) {
            /* Vua thay LCD cam lai -> doi 1 nhip 50ms cho nguon LCD on dinh */
            lcd_state = LCD_ST_INIT_1;
        }
        return;
    }

    /* Buoc 2: Gui nibble 0x03 dau tien (cho 50ms tiep theo > 4.1ms datasheet) */
    if (lcd_state == LCD_ST_INIT_1) {
        if (lcd_init_nibble(0x03) == HAL_OK) {
            lcd_state = LCD_ST_INIT_2;
        } else {
            lcd_ok = 0;
            lcd_state = LCD_ST_OFFLINE;
        }
        return;
    }

    /* Buoc 3: Hoan tat chuoi khoi tao 4-bit HD44780 (khong can HAL_Delay) */
    if (lcd_state == LCD_ST_INIT_2) {
        if (lcd_init_nibble(0x03) == HAL_OK &&
            lcd_init_nibble(0x03) == HAL_OK &&
            lcd_init_nibble(0x02) == HAL_OK &&
            lcd_cmd(0x28) == HAL_OK &&
            lcd_cmd(0x0C) == HAL_OK &&
            lcd_cmd(0x06) == HAL_OK &&
            lcd_cmd(0x01) == HAL_OK)
        {
            /* Xoa bo dem shown ve 0 de ep ve lai ca 2 dong o 2 lan goi tiep theo */
            memset(lcd_shown, 0, sizeof lcd_shown);
            lcd_ok = 1;
            lcd_probe_cnt = 0;
            lcd_state = LCD_ST_READY;
        } else {
            lcd_ok = 0;
            lcd_state = LCD_ST_OFFLINE;
        }
        return;
    }

    /* Buoc 4 (LCD_ST_READY): Ghi dong thay doi len LCD */
    uint8_t buf[4 * (1 + LCD_COLS)];
    for (uint8_t r = 0; r < LCD_ROWS; r++) {
        if (memcmp(lcd_want[r], lcd_shown[r], LCD_COLS) == 0) continue;

        uint8_t n = lcd_pack(buf, (uint8_t)(0x80 | (r ? 0x40 : 0x00)), 0);
        for (uint8_t c = 0; c < LCD_COLS; c++)
            n += lcd_pack(buf + n, (uint8_t)lcd_want[r][c], LCD_RS);

        if (lcd_tx(buf, n) == HAL_OK) {
            memcpy(lcd_shown[r], lcd_want[r], LCD_COLS);
            lcd_ok = 1;
            lcd_probe_cnt = 0;
        } else {
            lcd_ok = 0;
            lcd_state = LCD_ST_OFFLINE;
        }
        return;
    }

    /* Neu khong co dong nao thay doi, dinh ky 200ms ping thu de phat hien rut LCD */
    if (++lcd_probe_cnt >= LCD_PROBE_INTERVAL) {
        lcd_probe_cnt = 0;
        if (lcd_probe() != HAL_OK) {
            lcd_ok = 0;
            lcd_state = LCD_ST_OFFLINE;
        }
    }
}
