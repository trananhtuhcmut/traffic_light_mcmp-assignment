#include "lcd.h"
#include <string.h>

/* Anh xa chan PCF8574 -> LCD (module pho bien nhat). Neu LCD hien sai/khong hien,
 * kiem tra lai 4 dinh nghia nay theo mach cua module. */
#define LCD_RS   0x01   /* P0 */
#define LCD_EN   0x04   /* P2 (P1 = RW, noi GND -> luon ghi) */
#define LCD_BL   0x08   /* P3 = den nen */
/* D4..D7 = P4..P7 */

#define LCD_I2C_TIMEOUT_MS  20

static I2C_HandleTypeDef *lcd_i2c;
static uint8_t lcd_addr8;                 /* HAL can dia chi 8-bit = addr7 << 1 */
static uint8_t lcd_bl = LCD_BL;
static uint8_t lcd_ok = 0;
static char    lcd_want [LCD_ROWS][LCD_COLS];   /* noi dung can hien */
static char    lcd_shown[LCD_ROWS][LCD_COLS];   /* noi dung dang hien */

static HAL_StatusTypeDef lcd_tx(uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Master_Transmit(lcd_i2c, lcd_addr8, buf, len, LCD_I2C_TIMEOUT_MS);
}

/* Dong goi 1 byte LCD thanh 4 byte PCF8574: [hi|EN, hi, lo|EN, lo]. Tra ve so byte (4). */
static uint8_t lcd_pack(uint8_t *dst, uint8_t val, uint8_t rs)
{
    uint8_t hi = (uint8_t)((val & 0xF0) | lcd_bl | rs);
    uint8_t lo = (uint8_t)(((val << 4) & 0xF0) | lcd_bl | rs);
    dst[0] = hi | LCD_EN;  dst[1] = hi;
    dst[2] = lo | LCD_EN;  dst[3] = lo;
    return 4;
}

static void lcd_init_nibble(uint8_t nib)   /* chi dung trong chuoi khoi tao */
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
    HAL_Delay(2);                          /* lenh Clear can ~1.52 ms */
    memset(lcd_want,  ' ', sizeof lcd_want);
    memset(lcd_shown, ' ', sizeof lcd_shown);
}

void lcd_init(I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    lcd_i2c   = hi2c;
    lcd_addr8 = (uint8_t)(addr7 << 1);
    lcd_ok    = (HAL_I2C_IsDeviceReady(hi2c, lcd_addr8, 2, 10) == HAL_OK);

    HAL_Delay(50);                         /* cho LCD len nguon */
    lcd_init_nibble(0x03); HAL_Delay(5);   /* chuoi khoi tao 4-bit theo datasheet HD44780 */
    lcd_init_nibble(0x03); HAL_Delay(1);
    lcd_init_nibble(0x03); HAL_Delay(1);
    lcd_init_nibble(0x02); HAL_Delay(1);   /* chuyen sang giao tiep 4-bit */

    lcd_cmd(0x28);                         /* 4-bit, 2 dong, font 5x8 */
    lcd_cmd(0x0C);                         /* bat hien thi, tat con tro */
    lcd_cmd(0x06);                         /* con tro tu tang */
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
    uint8_t buf[4 * (1 + LCD_COLS)];           /* 1 lenh dat con tro + 16 ky tu */
    for (uint8_t r = 0; r < LCD_ROWS; r++) {
        if (memcmp(lcd_want[r], lcd_shown[r], LCD_COLS) == 0) continue;

        uint8_t n = lcd_pack(buf, (uint8_t)(0x80 | (r ? 0x40 : 0x00)), 0);
        for (uint8_t c = 0; c < LCD_COLS; c++)
            n += lcd_pack(buf + n, (uint8_t)lcd_want[r][c], LCD_RS);

        if (lcd_tx(buf, n) == HAL_OK) {         /* ca dong gui trong 1 giao dich I2C (~6 ms @100 kHz) */
            memcpy(lcd_shown[r], lcd_want[r], LCD_COLS);
            lcd_ok = 1;
        } else {
            lcd_ok = 0;                         /* giu nguyen -> lan sau thu lai */
        }
        return;                                 /* moi lan chi 1 dong */
    }
}