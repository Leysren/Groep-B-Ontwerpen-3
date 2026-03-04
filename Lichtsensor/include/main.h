#ifndef MAIN_H 
#define MAIN_H 

#include <stdint.h>
#define MSG_LIGHT 2

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

#endif