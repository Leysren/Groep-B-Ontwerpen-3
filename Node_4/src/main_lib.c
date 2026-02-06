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

// Initialize LED settings
void LED_init(void){
    PORTF.DIRSET = PIN1_bm; // LED Red
    PORTF.DIRSET = PIN0_bm; // LED Green
    PORTC.DIRSET = PIN0_bm; // LED Blue

    // PWM period Blue LED
    TCC0.PER = 4095;
    TCC0.CTRLB = TC0_CCAEN_bm | TC_WGMODE_SINGLESLOPE_gc;
    TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
    TCC0.CCA = 0;

    // PWM period Red and Green LED
    TCF0.PER = 4095;
    TCF0.CTRLB = TC0_CCAEN_bm | TC0_CCBEN_bm | TC_WGMODE_SINGLESLOPE_gc;
    TCF0.CTRLA = TC_CLKSEL_DIV1_gc;
    TCF0.CCA = 0;
    TCF0.CCB = 0;
}

void LED_set_brightness(uint8_t brightness){
    
    // Clamp input to valid range
    if (brightness > 100) brightness = 100;

    // Calculate brightness factor (inverse of ambient light)
    uint16_t brightness_factor = 100 - brightness;
    
    // Apply brightness to all channels equally (white light)
    uint16_t pwm_value = (brightness_factor * 4095) / 100;
    
    // Set PWM duty cycles
    TCF0.CCA = pwm_value;  // Red LED
    TCF0.CCB = pwm_value;  // Green LED
    TCC0.CCA = pwm_value;  // Blue LED
}

void LED_SET_COLOR(uint8_t *temperature){
    
}