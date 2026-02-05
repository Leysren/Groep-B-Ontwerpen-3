#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include "clock.h"
#include "serialF0.h"
#include "nrf24spiXM2.h"
#include "nrf24L01.h"
#include "main.h"
#include "i2c.h"


static uint8_t a_pcf8563_bcd2hex(uint8_t val)
{
    uint8_t temp;
    
    temp = val & 0x0F;              /* get ones place */
    val = (val >> 4) & 0x0F;        /* get tens place */
    val = val * 10;                 /* set tens place */
    temp = temp + val;              /* get hex */
    
    return temp;                    /* return hex */
}

void pcf8563_get_time(pcf8563_time_t *t)
{
    uint8_t sec, min, hour, date, week, month, year;

    i2c_start(&TWIE, PCF8563_ADDRESS, 0);
    i2c_write(&TWIE, PCF8563_REG_SECOND);

    i2c_restart(&TWIE, PCF8563_ADDRESS, 1);

    sec   = i2c_read(&TWIE, I2C_ACK);
    min   = i2c_read(&TWIE, I2C_ACK);
    hour  = i2c_read(&TWIE, I2C_ACK);
    date  = i2c_read(&TWIE, I2C_ACK);
    week  = i2c_read(&TWIE, I2C_ACK);
    month = i2c_read(&TWIE, I2C_ACK);
    year  = i2c_read(&TWIE, I2C_NACK);

    i2c_stop(&TWIE);

    // VL bit check
    if (sec & 0x80) return;

    t->second = a_pcf8563_bcd2hex(sec & 0x7F);
    t->minute = a_pcf8563_bcd2hex(min & 0x7F);
    t->hour   = a_pcf8563_bcd2hex(hour & 0x3F);
    t->date   = a_pcf8563_bcd2hex(date & 0x3F);
    t->week   = a_pcf8563_bcd2hex(week & 0x07);
    t->month  = a_pcf8563_bcd2hex(month & 0x1F);
    t->year   = 2000 + a_pcf8563_bcd2hex(year);
}

void pcf8563_print_time(const pcf8563_time_t *t)
{
    printf("Time: %02u:%02u:%02u  Date: %02u-%02u-%04u\n",
           t->hour,
           t->minute,
           t->second,
           t->date,
           t->month,
           t->year);
}

