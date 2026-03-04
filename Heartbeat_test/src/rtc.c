/*!
 *  \file    clock.c
 *  \author  Wim Dolman 
 *  \date    
 *  \version 
 *
 *  \brief   
 *
 */
#include "i2c.h"
#include "rtc.h"

// Globale RTC variabelen – hier gedefinieerd (extern in rtc.h)
struct rtc_time time = {0};
struct rtc_date date = {0};

void rtc_set_time(TWI_t *twi)
{
    i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE);
    i2c_write(twi, RTC_SECOND);
    i2c_write(twi, time.second);
    i2c_write(twi, time.minute);
    i2c_write(twi, time.hour);
    i2c_stop(twi);
}

int rtc_get_date(TWI_t *twi)
{
    uint8_t x = 0;

    if ((x = i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE)) > 0) {
        return x;
    }
    i2c_write(twi, RTC_DATE);
    if ((x = i2c_restart(twi, RTC_SLAVE_ADDRESS, I2C_READ)) > 0) {
        return x;
    }

    date.day   = i2c_read(twi, I2C_ACK);
    i2c_read(twi, I2C_ACK);  // skip weekday als die er is
    date.month = i2c_read(twi, I2C_ACK);
    date.year  = i2c_read(twi, I2C_NACK);
    i2c_stop(twi);

    return 0;
}

int rtc_get_time(TWI_t *twi)
{
    uint8_t x = 0;

    if ((x = i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE)) > 0) {
        return x;
    }
    i2c_write(twi, RTC_SECOND);
    if ((x = i2c_restart(twi, RTC_SLAVE_ADDRESS, I2C_READ)) > 0) {
        return x;
    }

    time.second = i2c_read(twi, I2C_ACK);
    time.minute = i2c_read(twi, I2C_ACK);
    time.hour   = i2c_read(twi, I2C_NACK);
    i2c_stop(twi);

    return 0;
}

void rtc_set_date(TWI_t *twi)
{
    i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE);
    i2c_write(twi, RTC_DATE);
    i2c_write(twi, date.day);
    i2c_write(twi, 0x00);  // weekday (niet gebruikt)
    i2c_write(twi, date.month);
    i2c_write(twi, date.year);
    i2c_stop(twi);
}

/* Tijd naar string: "HH:MM:SS" */
char *rtc_time_to_string(char *s)
{
    s[0] = '0' + ((time.hour & 0x30) >> 4);
    s[1] = '0' + (time.hour & 0x0F);
    s[2] = ':';
    s[3] = '0' + ((time.minute & 0x70) >> 4);
    s[4] = '0' + (time.minute & 0x0F);
    s[5] = ':';
    s[6] = '0' + ((time.second & 0x70) >> 4);
    s[7] = '0' + (time.second & 0x0F);
    s[8] = '\0';

    return s;
}

/* String "HH:MM:SS" naar tijd */
void string_to_rtc_time(char *s)
{
    uint8_t h10 = s[0] - '0';
    uint8_t h1  = s[1] - '0';
    uint8_t m10 = s[3] - '0';
    uint8_t m1  = s[4] - '0';
    uint8_t s10 = s[6] - '0';
    uint8_t s1  = s[7] - '0';

    time.hour   = (h10 << 4) | h1;
    time.minute = (m10 << 4) | m1;
    time.second = (s10 << 4) | s1;
}

/* Datum naar string: "DD-MM-20YY" */
char *rtc_date_to_string(char *s)
{
    s[0] = '0' + ((date.day & 0x30) >> 4);
    s[1] = '0' + (date.day & 0x0F);
    s[2] = '-';
    s[3] = '0' + ((date.month & 0x10) >> 4);
    s[4] = '0' + (date.month & 0x0F);
    s[5] = '-';
    s[6] = '2';
    s[7] = '0';
    s[8] = '0' + ((date.year & 0xF0) >> 4);
    s[9] = '0' + (date.year & 0x0F);
    s[10] = '\0';

    return s;
}

/* String "DD-MM-YY" naar datum */
void string_to_rtc_date(char *s)
{
    uint8_t d10 = s[0] - '0';
    uint8_t d1  = s[1] - '0';
    uint8_t m10 = s[3] - '0';
    uint8_t m1  = s[4] - '0';
    uint8_t y10 = s[6] - '0';
    uint8_t y1  = s[7] - '0';

    date.day   = (d10 << 4) | d1;
    date.month = (m10 << 4) | m1;
    date.year  = (y10 << 4) | y1;
}