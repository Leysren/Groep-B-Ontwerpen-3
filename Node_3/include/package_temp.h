#ifndef PACKAGE_TEMP
#define PACKAGE_TEMP

#include <stdint.h>

#define MSG_LIGHT 2
#define MSG_TEMP  3
#define MSG_TIME  1

typedef struct {
    uint8_t type;
    uint8_t user_id;
} msg_info_t;

typedef struct {
    msg_info_t info;
    uint8_t  temperature;
    uint8_t humidity;
    uint8_t pressure;
} msg_temp_t;

typedef struct {
    msg_info_t info;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} msg_time_t;

typedef struct light
{
    msg_info_t info;
    uint8_t light_percent;
}msg_light_t;

#endif