#ifndef MAIN_H 
#define MAIN_H 

#include <stdint.h>
#include "ucglib_xmega.h"

#define BAUD_100K 100000UL
#define MSG_TIME 1
#define MSG_LIGHT 2
#define MSG_TEMP 3

#define PCF8563_ADDRESS        0x51        /**< iic device address */ 

// Registers map
#define PCF8563_REG_CONTROL_STATUS1      0x00        /**< control status1 register */
#define PCF8563_REG_CONTROL_STATUS2      0x01        /**< control status2 register */
#define PCF8563_REG_SECOND               0x02        /**< second register */
#define PCF8563_REG_MINUTE               0x03        /**< minute register */
#define PCF8563_REG_HOUR                 0x04        /**< hour register */
#define PCF8563_REG_DAY                  0x05        /**< day register */
#define PCF8563_REG_WEEK                 0x06        /**< week register */
#define PCF8563_REG_MONTH                0x07        /**< month register */
#define PCF8563_REG_YEAR                 0x08        /**< year register */
#define PCF8563_REG_MINUTE_ALARM         0x09        /**< minute alarm register */
#define PCF8563_REG_HOUR_ALARM           0x0A        /**< hour alarm register */
#define PCF8563_REG_DAY_ALARM            0x0B        /**< day alarm register */
#define PCF8563_REG_WEEK_ALARM           0x0C        /**< week alarm register */
#define PCF8563_REG_CLKOUT_CONTROL       0x0D        /**< clkout control register */
#define PCF8563_REG_TIMER_CONTROL        0x0E        /**< timer control register */
#define PCF8563_REG_TIMER                0x0F        /**< timer register */

// Time structure
typedef struct pcf8563_time_s
{
    uint16_t year;         /**< year */
    uint8_t month;         /**< month */
    uint8_t week;          /**< week */
    uint8_t date;          /**< date */
    uint8_t hour;          /**< hour */
    uint8_t minute;        /**< minute */
    uint8_t second;        /**< second */
} pcf8563_time_t;

//Give identification and data info over the struct, each struct will contain this information
typedef struct attribute 
{
    uint8_t type;
    uint8_t user_id;
}msg_info_t;

typedef struct light
{
    msg_info_t info;
    uint8_t light_percent;
}msg_light_t;

typedef struct time
{
    msg_info_t info;
    uint8_t hour;          
    uint8_t minute;        
    uint8_t second;  
}msg_time_t;

typedef struct temp
{
    msg_info_t info;
    uint8_t temperature;
    uint8_t humidity;
    uint8_t pressure;
}msg_temp_t;

void pcf8563_get_time(pcf8563_time_t *t);
void pcf8563_print_time(const pcf8563_time_t *t);
void LED_init(void);
void LED_set_brightness(uint8_t brightness);

#endif