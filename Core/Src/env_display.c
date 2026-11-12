#include "env_display.h"
#include "global.h"
#include "scheduler.h"
#include "dht20.h"
#include "lcd.h"
#include <stdio.h>

#define ENV_READ_DELAY_TICKS 10 /* 10 tick x 10 ms = 100 ms >= 80 ms DHT20 can de do xong */

static int16_t env_temp_x10;
static uint16_t env_hum_x10;
static uint8_t env_valid = 0;

int16_t env_get_temp_x10(void) { return env_temp_x10; }
uint16_t env_get_hum_x10(void) { return env_hum_x10; }
uint8_t env_is_valid(void) { return env_valid; }

/* One-shot task: Period = 0 -> scheduler tu xoa sau khi chay */
static void env_read_task(void)
{
    int16_t t;
    uint16_t h;
    if (dht20_read(&t, &h) == DHT20_OK)
    {
        env_temp_x10 = t;
        env_hum_x10 = h;
        env_valid = 1;
    }
    else
    {
        env_valid = 0; /* LCD hien "--" thay vi so cu */
    }
}

void env_sample_task(void)
{
    if (dht20_trigger() == DHT20_OK)
        SCH_Add_Task(env_read_task, ENV_READ_DELAY_TICKS, 0);
    else
        env_valid = 0;
}

void display_task(void)
{
    char l0[32], l1[32]; /* lcd_put_line() tu cat con 16 ky tu */

    if (IS_TUNING(status))
        return; /* man hinh TUNING do fsm_tuning quan ly */

    if (IS_AUTO(status))
        snprintf(l0, sizeof l0, "AUTO R%02u G%02u A%02u",
                 (unsigned)time_red, (unsigned)time_green, (unsigned)time_amber);
    else
        snprintf(l0, sizeof l0, "MANUAL MODE");

    if (env_valid)
    {
        int t = env_temp_x10;
        const char *sign = "";
        if (t < 0)
        {
            sign = "-";
            t = -t;
        }
        snprintf(l1, sizeof l1, "T:%s%d.%dC H:%u.%u%%",
                 sign, t / 10, t % 10,
                 (unsigned)(env_hum_x10 / 10), (unsigned)(env_hum_x10 % 10));
    }
    else
    {
        snprintf(l1, sizeof l1, "T:--.-C H:--.-%%");
    }
    lcd_put_line(0, l0); /* lcd_put_line chi danh dau thay doi khi noi dung khac */
    lcd_put_line(1, l1);
}