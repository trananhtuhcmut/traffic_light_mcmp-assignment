#include "env_display.h"
#include "global.h"
#include "scheduler.h"
#include "dht20.h"
#include "lcd.h"
#include <stdio.h>

#define ENV_READ_DELAY_TICKS 10

int16_t env_get_temp_x10(void) { return curr_disp_data.temp_x10; }
uint16_t env_get_hum_x10(void) { return curr_disp_data.hum_x10; }
uint8_t env_is_valid(void)     { return curr_disp_data.env_valid ? 1U : 0U; }

static void env_read_task(void)
{
    int16_t t;
    uint16_t h;
    if (dht20_read(&t, &h) == DHT20_OK)
    {
        curr_disp_data.temp_x10 = t;
        curr_disp_data.hum_x10  = h;
        curr_disp_data.env_valid = true;
    }
    else
    {
        curr_disp_data.env_valid = false;
    }
}

void env_sample_task(void)
{
    if (dht20_trigger() == DHT20_OK)
        SCH_Add_Task(env_read_task, ENV_READ_DELAY_TICKS, 0);
    else
        curr_disp_data.env_valid = false;
}

void display_task(void)
{
    char l0[32], l1[32];

    /* Uu tien hien thong bao INVALID neu nguoi dung nhap R != G + Y */
    if (curr_disp_data.invalid_ticks > 0)
    {
        curr_disp_data.invalid_ticks--;
        snprintf(l0, sizeof l0, "INVALID! R!=G+Y");
    }

    else if (curr_disp_data.set_ticks > 0)
        {
            curr_disp_data.set_ticks--;
            snprintf(l0, sizeof l0, "      SET!      ");
        }

    else
    {
        switch (curr_disp_data.mode)
        {
            case MODE_AUTO:
                snprintf(l0, sizeof l0, "AUTO R1:%02u R2:%02u",
                         (unsigned)curr_disp_data.countdown_road1,
                         (unsigned)curr_disp_data.countdown_road2);
                break;

            case MODE_MANUAL:
                if (curr_disp_data.countdown_road1 > 0)
                {
                    snprintf(l0, sizeof l0, "MANUAL YEL: %02us",
                             (unsigned)curr_disp_data.countdown_road1);
                }
                else
                {
                    snprintf(l0, sizeof l0, "MANUAL MODE");
                }
                break;

            case MODE_SETTING:
            {
                const char *field_name = "---";
                if (curr_disp_data.active_field == EDIT_RED)        field_name = "RED";
                else if (curr_disp_data.active_field == EDIT_AMBER) field_name = "AMB";
                else if (curr_disp_data.active_field == EDIT_GREEN) field_name = "GRN";

                snprintf(l0, sizeof l0, "SET %s: %02us",
                         field_name, (unsigned)curr_disp_data.draft_duration);
                break;
            }

            default:
                snprintf(l0, sizeof l0, "TRAFFIC SYSTEM");
                break;
        }
    }

    /* Dong 1: Hien thi nhiet do & do am tu DHT20 */
    if (curr_disp_data.env_valid)
    {
        int t = curr_disp_data.temp_x10;
        const char *sign = "";
        if (t < 0)
        {
            sign = "-";
            t = -t;
        }
        snprintf(l1, sizeof l1, "T:%s%d.%dC H:%u.%u%%",
                 sign, t / 10, t % 10,
                 (unsigned)(curr_disp_data.hum_x10 / 10),
                 (unsigned)(curr_disp_data.hum_x10 % 10));
    }
    else
    {
        snprintf(l1, sizeof l1, "T:--.-C H:--.-%%");
    }

    lcd_put_line(0, l0);
    lcd_put_line(1, l1);
}
