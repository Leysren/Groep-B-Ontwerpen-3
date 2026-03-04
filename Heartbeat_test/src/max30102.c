/* ========================= max30102.c =========================
 * BELANGRIJK:
 * - Nep-millis() is verwijderd.
 * - Beat timing gebruikt nu 1x millis() per beat (stabiel).
 * - Aanbevolen: zet sample rate naar ~100sps zodat het matcht met 10ms loop.
 */

#include "max30102.h"
#include "i2c.h"
#include <util/delay.h>
#include <math.h>
#include <limits.h>

// Buffers
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

// Hartslag variabelen
static uint8_t rates[RATE_SIZE];
static uint8_t rateSpot = 0;
static uint32_t lastBeatTime = 0;
int beatAvg = 0;

// SpO2
int32_t spo2 = 0;
int8_t validSPO2 = 0;

// PBA filter
static int16_t IR_AC_Signal_Current = 0;
static int16_t IR_AC_Signal_Previous = 0;
static int16_t IR_AC_Signal_max = INT16_MIN;
static int16_t IR_AC_Signal_min = INT16_MAX;
static uint32_t ir_avg_reg = 0;

static const int16_t FIRCoeffs[12] = {172, 321, 579, 927, 1360, 1858, 2390, 2916, 3391, 3768, 4017, 4096};
static int16_t cbuf[32];
static uint8_t offset = 0;

static int16_t averageDCEstimator(uint32_t *p, uint16_t x)
{
    *p += ((((long)x << 15) - *p) >> 4);
    return (*p >> 15);
}

static int16_t lowPassFIRFilter(int16_t din)
{
    cbuf[offset] = din;
    int32_t z = ((int32_t)FIRCoeffs[11] * cbuf[(offset - 11) & 0x1F]);
    for (uint8_t i = 0; i < 11; i++) {
        z += ((int32_t)FIRCoeffs[i] * (cbuf[(offset - i) & 0x1F] + cbuf[(offset - 22 + i) & 0x1F]));
    }
    offset = (offset + 1) & 0x1F;
    return (z >> 15);
}

uint8_t max30102_readRegister(uint8_t reg)
{
    uint8_t value = 0;
    if (i2c_start(&TWIE, MAX30102_ADDRESS, I2C_WRITE) == I2C_STATUS_OK) {
        i2c_write(&TWIE, reg);
        if (i2c_restart(&TWIE, MAX30102_ADDRESS, I2C_READ) == I2C_STATUS_OK) {
            value = i2c_read(&TWIE, I2C_NACK);
        }
        i2c_stop(&TWIE);
    }
    return value;
}

void max30102_writeRegister(uint8_t reg, uint8_t value)
{
    if (i2c_start(&TWIE, MAX30102_ADDRESS, I2C_WRITE) == I2C_STATUS_OK) {
        i2c_write(&TWIE, reg);
        i2c_write(&TWIE, value);
        i2c_stop(&TWIE);
    }
}

void max30102_readFIFO(uint32_t *ir, uint32_t *red)
{
    uint8_t data[6];
    if (i2c_start(&TWIE, MAX30102_ADDRESS, I2C_WRITE) != I2C_STATUS_OK) return;
    i2c_write(&TWIE, MAX30102_FIFODATA);
    if (i2c_restart(&TWIE, MAX30102_ADDRESS, I2C_READ) != I2C_STATUS_OK) {
        i2c_stop(&TWIE);
        return;
    }
    for (uint8_t i = 0; i < 6; i++) {
        data[i] = i2c_read(&TWIE, (i < 5) ? I2C_ACK : I2C_NACK);
    }
    i2c_stop(&TWIE);

    *ir  = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    *red = ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];
    *ir  &= 0x3FFFF;
    *red &= 0x3FFFF;
}

void max30102_setup(void)
{
    // reset
    max30102_writeRegister(MAX30102_MODECONFIG, 0x40);
    _delay_ms(50);

    // FIFO: sample averaging/rollover etc (zoals jij had)
    max30102_writeRegister(MAX30102_FIFOCONFIG, 0b01100000);

    // SpO2 mode (RED + IR)
    max30102_writeRegister(MAX30102_MODECONFIG, 0x03);

    /* SPO2_CONFIG (jij noemt het PARTICLECONFIG):
     * Aanrader voor jouw 10ms loop:
     * - ADC range: 16384nA (11)
     * - sample rate: 100 sps (001)
     * - pulse width: 18-bit (11)
     * bits: [7:6]=11, [5:3]=001, [1:0]=11 => 0b11001111 = 0xCF
     */
    max30102_writeRegister(MAX30102_PARTICLECONFIG, 0xCF);

    // LED currents: begin laag om clipping te vermijden, dan fine-tunen
    max30102_writeRegister(MAX30102_LED1_PULSEAMP, 0x20); // IR
    max30102_writeRegister(MAX30102_LED2_PULSEAMP, 0x20); // RED
}

bool max30102_checkForBeat(uint32_t sample)
{
    IR_AC_Signal_Previous = IR_AC_Signal_Current;
    int16_t dc = averageDCEstimator(&ir_avg_reg, (uint16_t)sample);
    IR_AC_Signal_Current = lowPassFIRFilter((int16_t)(sample - dc));

    // update min/max
    if (IR_AC_Signal_Current > IR_AC_Signal_max) IR_AC_Signal_max = IR_AC_Signal_Current;
    if (IR_AC_Signal_Current < IR_AC_Signal_min) IR_AC_Signal_min = IR_AC_Signal_Current;

    // zero-cross rising
    if (IR_AC_Signal_Previous < 0 && IR_AC_Signal_Current >= 0) {
        int32_t amplitude = (int32_t)IR_AC_Signal_max - (int32_t)IR_AC_Signal_min;

        // reset extrema for next cycle
        IR_AC_Signal_max = INT16_MIN;
        IR_AC_Signal_min = INT16_MAX;

        // amplitude gate (eventueel later fine-tunen)
        if (amplitude > 10 && amplitude < 7000) {
            uint32_t now = millis();
            uint32_t delta = now - lastBeatTime;
            lastBeatTime = now;

            // accept 20..200 BPM => 3000..300ms
            if (delta > 300 && delta < 3000) {
                float bpm = 60000.0f / (float)delta;

                rates[rateSpot++] = (uint8_t)bpm;
                rateSpot %= RATE_SIZE;

                int sum = 0;
                for (uint8_t i = 0; i < RATE_SIZE; i++) sum += rates[i];
                beatAvg = sum / RATE_SIZE;
            }
            return true;
        }
    }

    return false;
}

void max30102_calculateSpO2(void)
{
    int64_t ir_sum = 0, red_sum = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        ir_sum += irBuffer[i];
        red_sum += redBuffer[i];
    }
    int32_t ir_mean = (int32_t)(ir_sum / BUFFER_SIZE);
    int32_t red_mean = (int32_t)(red_sum / BUFFER_SIZE);

    int64_t ir_var = 0, red_var = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        int32_t d = (int32_t)irBuffer[i] - ir_mean;
        ir_var += (int64_t)d * d;

        d = (int32_t)redBuffer[i] - red_mean;
        red_var += (int64_t)d * d;
    }

    double ir_rms = sqrt((double)ir_var / (double)BUFFER_SIZE);
    double red_rms = sqrt((double)red_var / (double)BUFFER_SIZE);

    if (ir_rms > 0.0) {
        double ratio = red_rms / ir_rms;
        spo2 = (int32_t)(104.0 - 17.0 * ratio);

        if (spo2 > 99) spo2 = 99;
        if (spo2 < 80) spo2 = -1;

        validSPO2 = (spo2 >= 90 && spo2 <= 99) ? 1 : 0;
    } else {
        spo2 = -1;
        validSPO2 = 0;
    }
}