//package compiler for light data
#ifndef PACKAGE_LIGHT
#define PACKAGE_LIGHT

#include <stdint.h>

#define MSG_TIME 1
#define MSG_LIGHT 2

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


#endif // package with light data