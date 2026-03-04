/* ========================= max30102.h ========================= */

#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>

// Registers
#define MAX30102_ADDRESS        0x57
#define MAX30102_FIFODATA       0x07
#define MAX30102_FIFOCONFIG     0x08
#define MAX30102_MODECONFIG     0x09
#define MAX30102_PARTICLECONFIG 0x0A   // (SPO2_CONFIG in datasheet)
#define MAX30102_LED1_PULSEAMP  0x0C
#define MAX30102_LED2_PULSEAMP  0x0D
#define MAX30102_PARTID         0xFF

#define BUFFER_SIZE 100
#define RATE_SIZE   4

#define MSG_TIME 1
#define MSG_LIGHT 2
#define MSG_TEMP 3

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

// Public functions
void     max30102_setup(void);
uint8_t  max30102_readRegister(uint8_t reg);
void     max30102_writeRegister(uint8_t reg, uint8_t value);
void     max30102_readFIFO(uint32_t *ir, uint32_t *red);
bool     max30102_checkForBeat(uint32_t sample);
void     max30102_calculateSpO2(void);

// millis() komt uit main.c (timer-based)
uint32_t millis(void);

// Public variables
extern uint32_t irBuffer[BUFFER_SIZE];
extern uint32_t redBuffer[BUFFER_SIZE];
extern int      beatAvg;
extern int32_t  spo2;
extern int8_t   validSPO2;

#endif