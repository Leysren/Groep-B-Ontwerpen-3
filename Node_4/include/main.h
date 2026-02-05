#ifndef MAIN_H 
#define MAIN_H 

#include "ucglib_xmega.h"

#define BAUD_100K 100000UL

#define PCF8563_ADDRESS        0x51        /**< iic device address */ 

#include <stdint.h>

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

//Received message struct
typedef struct msg_received
{
    uint8_t light_percent;
}msg_r;

typedef struct msg_sent
{  
    uint8_t hour;          
    uint8_t minute;        
    uint8_t second;  
}msg_s;


void pcf8563_get_time(pcf8563_time_t *t);
void pcf8563_print_time(const pcf8563_time_t *t);


#endif