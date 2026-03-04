/*
 * main.c — Node 2 Showcase
 *
 * BME280 sensor + NRF24 + motor control + stopper buttons + LEDs
 *
 * Pinout:
 *   BME280 SCL  -> PE0  (I2C)
 *   BME280 SDA  -> PE1  (I2C)
 *   Red  LED    -> PD0   — closing indicator
 *   Green LED   -> PD1   — opening indicator
 *   Motor PWM   -> PD4   — connect to L293D pin 1 (EN1)
 *   Motor LEFT  -> PA0   — connect to L293D pin 2 (1A)
 *   Motor RIGHT -> PA1   — connect to L293D pin 7 (2A)
 *   Left stopper button  -> PA6
 *   Right stopper button -> PA5
 *   Mode switch button   -> PA7  (TEMP/LIGHT mode)
 *   Light sensor         -> PA2  (ADC)
 */

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

#include "adc_light.h"

#define NRF_CHANNEL 2
#define MAXBUF      32
#define BME_ADDR    0x76

// light threshold — ADC is 12-bit (0-4095), so 50% = 2047 (i am too tired to fuck with percentage)
#define LIGHT_CLOSE_ABOVE  2050   // close curtains if light is above this
#define LIGHT_OPEN_BELOW   2000   // open curtains if light is below this

uint8_t Node = 2;
uint8_t current_brightness = 0;
int8_t  current_temp = 0;
uint16_t current_light = 0;   // raw ADC reading from PA2 (0-4095)

// NRF pipes
uint8_t pipe0[5] = "0pipe";
uint8_t pipe1[5] = "1pipe";
uint8_t pipe2[5] = "2pipe";
uint8_t pipe3[5] = "3pipe";

// RX interrupt variables
volatile uint8_t rx_flag = 0;
uint8_t rx_packet[MAXBUF];

// timer_flag raised by TCE0 overflow IRQ
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


// window position — remembered so the motor stops when the window/curtain closed/opened
// WIN_MIDDLE = somewhere in between, WIN_OPEN = left stopper hit, WIN_CLOSED = right stopper hit
typedef enum { WIN_MIDDLE, WIN_OPEN, WIN_CLOSED } WindowPos;
WindowPos window_pos = WIN_MIDDLE;

// motor state — is it moving and which way
typedef enum { MOTOR_STOP, MOTOR_OPENING, MOTOR_CLOSING } MotorState;
MotorState motor_state = MOTOR_STOP;

// speed preset — slow or fast
typedef enum { SPEED_SLOW, SPEED_FAST } SpeedPreset;
SpeedPreset speed_preset = SPEED_SLOW;

// operating mode — press both buttons to switch between temperature and light control
typedef enum { MODE_TEMP, MODE_LIGHT } OperatingMode;
OperatingMode current_mode = MODE_TEMP;

#define TEMP_OPEN_ABOVE   23   // open window if temp goes above this
#define TEMP_CLOSE_BELOW  22   // close window if temp drops below this

#define PWM_PERIOD  4095
#define PWM_SLOW    ((uint16_t)(PWM_PERIOD * 0.40))   // 40% speed
#define PWM_FAST    ((uint16_t)(PWM_PERIOD * 0.80))   // 80% speed

#define LED_DIM   ((uint16_t)(4095 * 0.30))
#define LED_FULL  4095


// returns the right PWM value for whichever speed preset is active
uint16_t get_speed(void) {
    if (speed_preset == SPEED_SLOW) return PWM_SLOW;
    if (speed_preset == SPEED_FAST) return PWM_FAST;
}


// ============ TIMER ISR ============

ISR(TCE0_OVF_vect)
{
    timer_flag = 1;
}


// ============ NRF ISR ============

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

    // Interrupt pin config
    NRF24_IRQ_PORT.INT0MASK |= NRF24_IRQ_PIN;
    NRF24_IRQ_PORT.NRF24_IRQ_CTRL = PORT_ISC_FALLING_gc;
    NRF24_IRQ_PORT.INTCTRL |=
        (NRF24_IRQ_PORT.INTCTRL & ~PORT_INT0LVL_gm) | PORT_INT0LVL_LO_gc;

    nrfOpenWritingPipe(pipe1);
    nrfOpenReadingPipe(0, pipe0);
    nrfOpenReadingPipe(1, pipe1);
    nrfOpenReadingPipe(2, pipe2);

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


// ============ BME280 ============

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

    dig_P1 = (buf[7]  << 8) | buf[6];
    dig_P2 = (buf[9]  << 8) | buf[8];
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


// ============ BME280 COMPENSATION ============
// these functions are from the BME280 datasheet, they convert raw sensor
// values into real units (celsius, hPa, %RH)

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


// ============ Temperature RGB LED ============

void LED_init(void) {
    PORTF.DIRSET = PIN1_bm;   // RGB Red
    PORTF.DIRSET = PIN0_bm;   // RGB Green
    PORTC.DIRSET = PIN0_bm;   // RGB Blue

    TCC0.PER   = 4095;
    TCC0.CTRLB = TC0_CCAEN_bm | TC_WGMODE_SINGLESLOPE_gc;
    TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
    TCC0.CCA   = 0;

    TCF0.PER   = 4095;
    TCF0.CTRLB = TC0_CCAEN_bm | TC0_CCBEN_bm | TC_WGMODE_SINGLESLOPE_gc;
    TCF0.CTRLA = TC_CLKSEL_DIV1_gc;
    TCF0.CCA   = 0;
    TCF0.CCB   = 0;
}

void LED_SET_COLOR(uint8_t temp) {
    int8_t brightness_factor = 100 - current_brightness;
    if (brightness_factor > 100) brightness_factor = 100;

    float RED, GREEN, BLUE;
    if (temp >= 23) { RED = 2.5; GREEN = 0.0; BLUE = 0.0; }
    else            { RED = 0.0; GREEN = 2.5; BLUE = 0.0; }

    TCF0.CCA = (brightness_factor * 4095 * RED)   / 100;
    TCF0.CCB = (brightness_factor * 4095 * GREEN)  / 100;
    TCC0.CCA = (brightness_factor * 4095 * BLUE)   / 100;
}


// ============ DIRECTION EXTERNAL LEDs ============

void init_leds(void) {
    PORTD.DIRSET = PIN0_bm | PIN1_bm;   // PD0=red, PD1=green

    TCD0.PER   = 4095;
    TCD0.CCA   = 0;
    TCD0.CCB   = 0;
    TCD0.CTRLB = TC0_CCAEN_bm | TC0_CCBEN_bm | TC_WGMODE_SINGLESLOPE_gc;
    TCD0.CTRLA = TC_CLKSEL_DIV1_gc;
}

void led_off(void) {
    TCD0.CCA = 0;
    TCD0.CCB = 0;
}

// red LED on — window is closing
// brightness shows speed: dim = slow, full = fast
void led_closing(void) {
    TCD0.CCB = 0;
    TCD0.CCA = (speed_preset == SPEED_FAST) ? LED_FULL : LED_DIM;
}

// green LED on — window is opening
// brightness shows speed: dim = slow, full = fast
void led_opening(void) {
    TCD0.CCA = 0;
    TCD0.CCB = (speed_preset == SPEED_FAST) ? LED_FULL : LED_DIM;
}


// ============ MOTOR ============

void motor_init(void) {
    PORTA.DIRSET = PIN0_bm | PIN1_bm;   // direction pins as outputs
    PORTA.OUTCLR = PIN0_bm | PIN1_bm;   // both low = motor off

    PORTD.DIRSET = PIN4_bm;   // PWM pin as output

    TCD1.PER   = PWM_PERIOD;
    TCD1.CCA   = 0;
    TCD1.CTRLB = TC1_CCAEN_bm | TC_WGMODE_SINGLESLOPE_gc;
    TCD1.CTRLA = TC_CLKSEL_DIV1_gc;
}

// open = motor turns left
void motor_open(void) {
    TCD1.CCA = 0;              // cut power before switching direction (protects H-bridge)
    PORTA.OUTSET = PIN0_bm;    // Left high
    PORTA.OUTCLR = PIN1_bm;    // Right low
    TCD1.CCA = get_speed();
    motor_state = MOTOR_OPENING;
    led_opening();
    printf(">> OPENING | T=%d | Speed: %s\n",
           current_temp, speed_preset == SPEED_FAST ? "FAST" : "SLOW");
}

// close = motor turns right
void motor_close(void) {
    TCD1.CCA = 0;              // cut power before switching direction (protects H-bridge)
    PORTA.OUTCLR = PIN0_bm;    // Left low
    PORTA.OUTSET = PIN1_bm;    // Right high
    TCD1.CCA = get_speed();
    motor_state = MOTOR_CLOSING;
    led_closing();
    printf(">> CLOSING | T=%d | Speed: %s\n",
           current_temp, speed_preset == SPEED_FAST ? "FAST" : "SLOW");
}

void motor_stop(void) {
    TCD1.CCA = 0;
    PORTA.OUTCLR = PIN0_bm | PIN1_bm;
    motor_state = MOTOR_STOP;
    led_off();
}


// ============ BUTTONS ============

void init_buttons(void) {
    PORTA.DIRCLR   = PIN7_bm | PIN6_bm | PIN5_bm;
    PORTA.PIN6CTRL = PORT_OPC_PULLUP_gc;   // left stopper  PA6
    PORTA.PIN5CTRL = PORT_OPC_PULLUP_gc;   // right stopper PA5
    PORTA.PIN7CTRL = PORT_OPC_PULLUP_gc;   // mode switch   PA7
}

uint8_t btn_left(void)   { return !(PORTA.IN & PIN6_bm); }   // left stopper  PA6
uint8_t btn_right(void)  { return !(PORTA.IN & PIN5_bm); }   // right stopper PA5
uint8_t btn_mode(void)   { return !(PORTA.IN & PIN7_bm); }   // mode switch   PA7

// checks stopper buttons, speed and mode switch
void handle_buttons(void) {
    static uint8_t both_last = 0;
    static uint8_t mode_last = 0;

    uint8_t left  = btn_left();
    uint8_t right = btn_right();
    uint8_t both  = left && right;
    uint8_t mode  = btn_mode();

    // both stoppers pressed at same time -> toggle speed preset
    if (both && !both_last) {
        if (speed_preset == SPEED_SLOW) {
            speed_preset = SPEED_FAST;
            printf("Speed: FAST (80%%)\n");
        } else {
            speed_preset = SPEED_SLOW;
            printf("Speed: SLOW (40%%)\n");
        }
        // update PWM right away if motor is already running
        if (motor_state != MOTOR_STOP) TCD1.CCA = get_speed();
    }
    both_last = both;

    // mode button (PA7) pressed -> switch between TEMP and LIGHT mode
    if (mode && !mode_last) {
        motor_stop();            // stop motor when switching mode
        window_pos = WIN_MIDDLE; // reset for the new mode
        if (current_mode == MODE_TEMP) {
            current_mode = MODE_LIGHT;
            printf("Mode: LIGHT\n");
        } else {
            current_mode = MODE_TEMP;
            printf("Mode: TEMP\n");
        }
    }
    mode_last = mode;

    // left stopper hit while opening -> window fully open, stop
    if (left && !both && motor_state == MOTOR_OPENING) {
        motor_stop();
        window_pos = WIN_OPEN;
        printf("Stopper: OPEN\n");
    }

    // right stopper hit while closing -> window fully closed, stop
    if (right && !both && motor_state == MOTOR_CLOSING) {
        motor_stop();
        window_pos = WIN_CLOSED;
        printf("Stopper: CLOSED\n");
    }
}


// ============ WINDOW CONTROL ============

// decides whether to open or close based on temperature
// only runs when motor is stopped so it doesnt interrupt mid movement
void handle_window(void) {
    if (motor_state != MOTOR_STOP) return;

    if (current_temp > TEMP_OPEN_ABOVE) {
        // too hot: open window, unless its fully open
        if (window_pos != WIN_OPEN) {
            window_pos = WIN_MIDDLE;
            motor_open();
        }
    } else if (current_temp < TEMP_CLOSE_BELOW) {
        // too cold: close window, unless its fully closed
        if (window_pos != WIN_CLOSED) {
            window_pos = WIN_MIDDLE;
            motor_close();
        }
    }
    // 22-23C grey zone: do nothing
}

// ============ WINDOW CONTROL — LIGHT MODE ============

// opens or closes curtains based on light reading
void handle_window_light(void) {
    if (motor_state != MOTOR_STOP) return;   // already moving, leave it alone

    if (current_light > LIGHT_CLOSE_ABOVE) {
        // too bright — close curtains, unless already closed
        if (window_pos != WIN_CLOSED) {
            window_pos = WIN_MIDDLE;
            motor_close();
            printf("Light high (%u) -> closing\n", current_light);
        }
    } else {
        // not bright enough — open curtains, unless already open
        if (window_pos != WIN_OPEN) {
            window_pos = WIN_MIDDLE;
            motor_open();
            printf("Light low (%u) -> opening\n", current_light);
        }
    }
}


// ============ MAIN ============

int main(void) {
    init_clock();
    _delay_ms(100);
    init_stream(F_CPU);
    sei();

    LED_init();
    init_leds();
    motor_init();
    init_buttons();
    init_adc();

    printf("Temperature Node %d starting...\n", Node);

    // I2C for BME280
    PR.PRPE &= ~PR_TWI_bm;
    _delay_ms(10);
    i2c_init(&TWIE, TWI_BAUD(F_CPU, 100000UL));

    bme_read_calibration();
    bme_init();
    printf("BME280 OK\n");

    PMIC.CTRL |= PMIC_LOLVLEN_bm;
    nrf_init();

    printf("Ready!\n");

    while (1) {

        handle_buttons();

        // read BME280
        int32_t raw_temp, raw_pres, raw_hum;
        bme_read_raw(&raw_temp, &raw_pres, &raw_hum);

        int16_t  temperature = compensate_temperature(raw_temp) / 100;
        uint16_t pressure    = compensate_pressure(raw_pres) / 100;
        uint16_t humidity    = compensate_humidity(raw_hum) / 1024;

        current_temp = (int8_t)temperature;

        // read light sensor
        current_light = read_adc();

        // decide if window needs to move
        if (current_mode == MODE_TEMP) {
            handle_window();        // temperature mode
        } else {
            handle_window_light();  // light mode
        }

        // send temperature reading over NRF
        msg_temp_t msg;
        msg.info.type    = MSG_TEMP;
        msg.info.user_id = Node;
        msg.temperature  = temperature;
        msg.humidity     = humidity;
        msg.pressure     = pressure;
        nrf_send((uint8_t*)&msg, sizeof(msg));

        // print a status update every 2 seconds while motor is moving
        static uint16_t move_counter = 0;
        if (motor_state != MOTOR_STOP) {
            if (++move_counter >= 200) {   // 200 * 10ms = 2s
                move_counter = 0;
                printf("   ...%s | T=%d\n",
                       motor_state == MOTOR_OPENING ? "opening" : "closing",
                       current_temp);
            }
        } else {
            move_counter = 0;
        }

        // handle incoming NRF messages
        if (rx_flag) {
            rx_flag = 0;
            msg_info_t info;
            memcpy(&info, rx_packet, sizeof(info));

            if (info.type == MSG_TIME) {
                msg_time_t time_msg;
                memcpy(&time_msg, rx_packet, sizeof(time_msg));
                printf("Time: %02u:%02u:%02u\n",
                       time_msg.hour, time_msg.minute, time_msg.second);
            }
            if (info.type == MSG_LIGHT) {
                msg_light_t light_msg;
                memcpy(&light_msg, rx_packet, sizeof(light_msg));
                printf("LIGHT: %d\n", light_msg.light_percent);
                current_brightness = (uint8_t)light_msg.light_percent;
            }
            if (info.type == MSG_TEMP) {
                msg_temp_t temp_msg;
                memcpy(&temp_msg, rx_packet, sizeof(temp_msg));
                printf("Temperature: %d\n", temp_msg.temperature);
            }
        }

        // update onboard RGB based on temperature
        LED_SET_COLOR(current_temp);

        _delay_ms(10);
    }

    return 0;
}