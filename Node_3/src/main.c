#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/delay.h>
#include "clock.h"
#include "serialF0.h"
#include "nrf24spiXM2.h"
#include "nrf24L01.h"
#include "i2c.h"
#include "package_temp.h"

#define NRF_CHANNEL 2
#define MAXBUF      32
#define BME_ADDR    0x76

uint8_t Node = 3;

// NRF pipes
uint8_t pipe0[5] = "0pipe";
uint8_t pipe1[5] = "1pipe";
uint8_t pipe2[5] = "2pipe";
uint8_t pipe3[5] = "3pipe";

// RX interrupt variables
volatile uint8_t rx_flag = 0;
uint8_t rx_packet[MAXBUF];

// timer_flag is raised by timer/counter IRQ
volatile uint8_t timer_flag = 0;
// BME280 calibration values
uint16_t dig_T1;
int16_t  dig_T2, dig_T3;
uint16_t dig_P1;
int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
uint8_t  dig_H1, dig_H3;
int16_t  dig_H2, dig_H4, dig_H5;
int8_t   dig_H6;
int32_t  t_fine;


// Timer used to periodically send values through nRF
ISR(TCE0_OVF_vect)
{
    timer_flag = 1;
}



// Interrupt when nRF package is received
ISR(NRF24_IRQ_VEC)
{
    uint8_t tx_ds, max_rt, rx_dr;
    uint8_t packet_length;

    nrfWhatHappened(&tx_ds, &max_rt, &rx_dr);

    if (rx_dr) {
        packet_length = nrfGetDynamicPayloadSize();
        nrfRead(rx_packet, packet_length);
        rx_flag = 1;
    }
}


// ============ NRF SETUP ============

void nrf_init(void) {
    nrfspiInit();
    nrfBegin();
    nrfSetRetries(NRF_SETUP_ARD_1000US_gc, NRF_SETUP_ARC_NORETRANSMIT_gc);
    nrfSetPALevel(NRF_RF_SETUP_PWR_6DBM_gc);
    nrfSetDataRate(NRF_RF_SETUP_RF_DR_250K_gc);
    nrfSetCRCLength(NRF_CONFIG_CRC_16_gc);
    nrfSetChannel(NRF_CHANNEL);
    nrfSetAutoAck(0);
    nrfEnableDynamicPayloads();
    nrfClearInterruptBits();
    nrfFlushRx();
    nrfFlushTx();

    // Interrupt pin config (was missing!)
    NRF24_IRQ_PORT.INT0MASK |= NRF24_IRQ_PIN;
    NRF24_IRQ_PORT.NRF24_IRQ_CTRL = PORT_ISC_FALLING_gc;
    NRF24_IRQ_PORT.INTCTRL |=
        (NRF24_IRQ_PORT.INTCTRL & ~PORT_INT0LVL_gm) | PORT_INT0LVL_LO_gc;

    // Writing pipe for Node 3
    nrfOpenWritingPipe(pipe3);

   

    // Then open the ones we actually want to listen to
    nrfOpenReadingPipe(0, pipe0);  // Listen to screen node (Node 0)
    nrfOpenReadingPipe(1, pipe1);  // Listen to Node 1
    nrfOpenReadingPipe(2, pipe2);  // Listen to light node (Node 2)

    nrfStartListening();
    nrfPowerUp();
}

void nrf_send(uint8_t *data, uint8_t size) {
    nrfStopListening();
    cli();
    nrfWrite(data, size);
    sei();
    nrfStartListening();
}


// ============ BME280 SETUP ============

void bme_read_calibration(void) {
    uint8_t buf[26];

    i2c_start(&TWIE, BME_ADDR, 0);
    i2c_write(&TWIE, 0x88);
    i2c_restart(&TWIE, BME_ADDR, 1);
    for (uint8_t i = 0; i < 26; i++) {
        buf[i] = i2c_read(&TWIE, (i == 25) ? I2C_NACK : I2C_ACK);
    }
    i2c_stop(&TWIE);

    dig_T1 = (buf[1] << 8) | buf[0];
    dig_T2 = (buf[3] << 8) | buf[2];
    dig_T3 = (buf[5] << 8) | buf[4];

    dig_P1 = (buf[7] << 8)  | buf[6];
    dig_P2 = (buf[9] << 8)  | buf[8];
    dig_P3 = (buf[11] << 8) | buf[10];
    dig_P4 = (buf[13] << 8) | buf[12];
    dig_P5 = (buf[15] << 8) | buf[14];
    dig_P6 = (buf[17] << 8) | buf[16];
    dig_P7 = (buf[19] << 8) | buf[18];
    dig_P8 = (buf[21] << 8) | buf[20];
    dig_P9 = (buf[23] << 8) | buf[22];

    dig_H1 = buf[25];

    uint8_t hum[7];
    i2c_start(&TWIE, BME_ADDR, 0);
    i2c_write(&TWIE, 0xE1);
    i2c_restart(&TWIE, BME_ADDR, 1);
    for (uint8_t i = 0; i < 7; i++) {
        hum[i] = i2c_read(&TWIE, (i == 6) ? I2C_NACK : I2C_ACK);
    }
    i2c_stop(&TWIE);

    dig_H2 = (hum[1] << 8) | hum[0];
    dig_H3 = hum[2];
    dig_H4 = (hum[3] << 4) | (hum[4] & 0x0F);
    dig_H5 = (hum[5] << 4) | ((hum[4] >> 4) & 0x0F);
    dig_H6 = hum[6];
}

void bme_init(void) {
    i2c_start(&TWIE, BME_ADDR, 0);
    i2c_write(&TWIE, 0xF2);
    i2c_write(&TWIE, 0x01);
    i2c_stop(&TWIE);

    i2c_start(&TWIE, BME_ADDR, 0);
    i2c_write(&TWIE, 0xF4);
    i2c_write(&TWIE, 0x27);
    i2c_stop(&TWIE);
}

void bme_read_raw(int32_t *temperature, int32_t *pressure, int32_t *humidity) {
    uint8_t data[8];

    i2c_start(&TWIE, BME_ADDR, 0);
    i2c_write(&TWIE, 0xF7);
    i2c_restart(&TWIE, BME_ADDR, 1);
    for (uint8_t i = 0; i < 8; i++) {
        data[i] = i2c_read(&TWIE, (i == 7) ? I2C_NACK : I2C_ACK);
    }
    i2c_stop(&TWIE);

    *pressure    = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | (data[2] >> 4);
    *temperature = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | (data[5] >> 4);
    *humidity    = ((uint32_t)data[6] << 8)  | data[7];
}


// ============ BME280 COMPENSATION (from datasheet section 10.1) ============

int32_t compensate_temperature(int32_t adc_T) {
    int32_t var1, var2;

    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12;
    var2 = (var2 * ((int32_t)dig_T3)) >> 14;

    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

uint32_t compensate_pressure(int32_t adc_P) {
    int64_t var1, var2, p;

    var1 = (int64_t)t_fine - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)dig_P1) >> 33;

    if (var1 == 0) return 0;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);

    return (uint32_t)(p >> 8);
}

uint32_t compensate_humidity(int32_t adc_H) {
    int32_t v;

    v = t_fine - 76800;
    v = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) -
          (((int32_t)dig_H5) * v)) + 16384) >> 15) *
        (((((((v * ((int32_t)dig_H6)) >> 10) *
          (((v * ((int32_t)dig_H3)) >> 11) + 32768)) >> 10) +
          2097152) * ((int32_t)dig_H2) + 8192) >> 14));

    v = v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4);
    if (v < 0) v = 0;
    if (v > 419430400) v = 419430400;

    return (uint32_t)(v >> 12);
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

// TEMPORARY: Only brightness control (white light)
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



// ============ MAIN ============

int main(void) {
    init_clock();
    _delay_ms(100);
    init_stream(F_CPU);
    sei();
    LED_init();  // Initialize LEDs
    printf("Temperature Node %d starting...\n", Node);

    uint8_t current_brightness = 0;
    // Setup I2C
    PR.PRPE &= ~PR_TWI_bm;
    _delay_ms(10);
    i2c_init(&TWIE, TWI_BAUD(F_CPU, 100000UL));

    // Setup BME280
    bme_read_calibration();
    bme_init();

    // Setup NRF
    PMIC.CTRL |= PMIC_LOLVLEN_bm;
    nrf_init();

    printf("Ready!\n");

    while (1) {
        // 1. Read sensor
        int32_t raw_temp, raw_pres, raw_hum;
        bme_read_raw(&raw_temp, &raw_pres, &raw_hum);

        // 2. Convert to readable values
        int16_t  temperature = compensate_temperature(raw_temp) / 100;
        uint16_t pressure    = compensate_pressure(raw_pres) / 100;
        uint16_t humidity    = compensate_humidity(raw_hum) / 1024;

        // 3. Build message
        msg_temp_t msg;
        msg.info.type    = MSG_TEMP;
        msg.info.user_id = Node;
        msg.temperature  = temperature;
        msg.humidity     = humidity;
        msg.pressure     = pressure;

        // 4. Send message
        nrf_send((uint8_t*)&msg, sizeof(msg));

        printf("T:%d H:%u P:%u\n", temperature, humidity, pressure);

        // 5. Check for incoming time messages
        if (rx_flag) {
            rx_flag = 0;
            msg_info_t info;
            memcpy(&info, rx_packet, sizeof(info));

            if (info.type == MSG_TIME) {
                msg_time_t time_msg;
                memcpy(&time_msg, rx_packet, sizeof(time_msg));
                printf("Time: %02u:%02u:%02u\n", time_msg.hour, time_msg.minute, time_msg.second);
            }

            if (info.type == MSG_LIGHT) {
                msg_light_t light_msg;
                memcpy(&light_msg, rx_packet, sizeof(light_msg));
                printf("LIGHT: %d\n", light_msg.light_percent);

                current_brightness = (uint8_t)light_msg.light_percent;
            }
          LED_set_brightness(current_brightness);  
        }


        _delay_ms(500);
    }
}