//package compiler for light data
#ifndef PACKAGE_LIGHT
#define PACKAGE_LIGHT

#include <stdint.h>

typedef struct {
    uint8_t light_percent;  // 0-100%
} sensor_packet_t;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} time_packet_t;

#endif // package with light data