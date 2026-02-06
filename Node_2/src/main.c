//Light node for dummie code
#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include "clock.h"
#include "serialF0.h"
#include "nrf24spiXM2.h"
#include "nrf24L01.h"
#include "adc_light.h"
#include "package_light.h"

//Define the NRF Channel
#define NRF_CHANNEL 2


#define MAXBUF 32

uint8_t Node  = 2;    
char    ID[] = "2"; //assigning the number to our current node

// Select which pipes the device should listen to (Subscribe)
uint8_t ListenTo_pipe0 = 1; // Node 4
uint8_t ListenTo_pipe2 = 1; // Node 3

// Define broadcast pipes for sending data to different Nodes
uint8_t BroadcastPipe_0[5] = "0pipe";
uint8_t BroadcastPipe_1[5] = "1pipe";
uint8_t BroadcastPipe_2[5] = "2pipe";
uint8_t BroadcastPipe_3[5] = "3pipe"; //(Dummy pipe)

volatile uint8_t rx_flag = 0;

uint8_t rx_packet[MAXBUF];




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



void timer_init(void)
{
    TCE0.CTRLB = TC_WGMODE_NORMAL_gc;
    TCE0.CTRLA = TC_CLKSEL_DIV1_gc;
    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;
    TCE0.PER = 31999;
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

void nrf_init(uint8_t b)
{
    printf("1. SPI init\n");
    nrfspiInit();
    
    printf("2. NRF begin\n");
    nrfBegin();
    
    printf("3. Set retries\n");
    nrfSetRetries(NRF_SETUP_ARD_1000US_gc, NRF_SETUP_ARC_NORETRANSMIT_gc);
    
    printf("4. Set PA level\n");
    nrfSetPALevel(NRF_RF_SETUP_PWR_6DBM_gc);
    
    printf("5. Set data rate\n");
    nrfSetDataRate(NRF_RF_SETUP_RF_DR_250K_gc);
    
    printf("6. Set CRC\n");
    nrfSetCRCLength(NRF_CONFIG_CRC_16_gc);
    
    printf("7. Set channel\n");
    nrfSetChannel(NRF_CHANNEL);
    
    printf("8. Set AutoAck\n");
    nrfSetAutoAck(0);
    
    printf("9. Enable dynamic payloads\n");
    nrfEnableDynamicPayloads();
    
    printf("10. Clear interrupts\n");
    nrfClearInterruptBits();
    
    printf("11. Flush RX/TX\n");
    nrfFlushRx();
    nrfFlushTx();

    // **ADD INTERRUPT CONFIGURATION HERE**
    printf("12. Configure IRQ pin\n");
    NRF24_IRQ_PORT.INT0MASK |= NRF24_IRQ_PIN;
    NRF24_IRQ_PORT.NRF24_IRQ_CTRL = PORT_ISC_FALLING_gc;
    NRF24_IRQ_PORT.INTCTRL |=
        (NRF24_IRQ_PORT.INTCTRL & ~PORT_INT0LVL_gm) | PORT_INT0LVL_LO_gc;

    // Open writing pipe for Node 2
    printf("13. Open writing pipe\n");
    nrfOpenWritingPipe((uint8_t *)BroadcastPipe_2);

    // **FIX THE POINTER CASTS**
    if(ListenTo_pipe0){
        nrfOpenReadingPipe(0, (uint8_t *)BroadcastPipe_0); 
    }
    if(ListenTo_pipe2){
        nrfOpenReadingPipe(2, (uint8_t *)BroadcastPipe_2);
    }

    printf("14. Start listening\n");
    nrfStartListening();
    
    printf("15. Power up\n");
    nrfPowerUp();
    
    printf("NRF init done!\n");
}


// Structure to hold time data
//
// =====i recieve this via nrf from screen node======





int main(void){
    init_clock();
    _delay_ms(100);
    init_stream(F_CPU);
    //printf("Clock and serial OK\n");  //debugging Clock
    
    init_adc();
    //printf("ADC OK\n"); //debugging ADC
    
    LED_init();  // Initialize LEDs
    //printf("LED OK\n"); //debugging led
    // **ADD THIS - enable low priority interrupts**
    PMIC.CTRL |= PMIC_LOLVLEN_bm;


    sei();
    //printf("Interrupts enabled\n"); //debugging interrupts
    
    printf("Hi, I am Node %d\n", Node);
    
    //printf("Starting NRF init...\n"); //debugging nrf
    nrf_init(0);
    //printf("NRF init complete!\n"); //debugging nrf
    

while (1) {
    // Your existing light sensor code
    uint16_t adc_value = read_adc();
    uint8_t percentage = (adc_value * 100UL) / 4095;
    
    msg_light_t msg_light;
    msg_light.info.type = MSG_LIGHT;
    msg_light.info.user_id = Node;
    msg_light.light_percent = percentage;
    
    nrfStopListening();
    nrfWrite((uint8_t*)&msg_light, sizeof(msg_light));
    nrfStartListening();
    
    printf("ADC: %u, Light: %u%% SENT\n", adc_value, percentage);
    _delay_ms(100);
    
// Check if time data is available to receive
if (rx_flag) {
    rx_flag = 0;
    msg_info_t info; 
    memcpy(&info, rx_packet, sizeof(info));
    
    if (info.type == MSG_TIME){
        msg_time_t msg_time;
        memcpy(&msg_time, rx_packet, sizeof(msg_time));
        printf("Time from: %d: %02u:%02u:%02u\n", msg_time.info.user_id, msg_time.hour, msg_time.minute, msg_time.second);
    }
}
    LED_set_brightness(percentage);
    
    _delay_ms(500);
}
}