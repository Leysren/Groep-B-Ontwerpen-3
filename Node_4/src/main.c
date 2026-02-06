/*
 * nRF24L01+ Broadcast example
 *
 *
 * This example contains code for a Publish / Subscribe configuration of three Nodes
 * Each node sends on its own pipe and listens on the other two pipes. The
 * nodes write data on a timer. 
 * 
 * A DIP switch can be used to setup the system:
 * 
 * DIP1 = listen to pipe 0
 * DIP2 = listen to pipe 1
 * DIP3 = listen to pipe 2
 * DIP4 = Map Potmeter value to own LED Color
 * 
 * This configuration is easy to setup but there is no acknowledgement.
 */
#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include "clock.h"
#include "serialF0.h"
#include "nrf24spiXM2.h"
#include "nrf24L01.h"
#include "main.h"
#include "ucglib_xmega.h" 
#include "i2c.h"

// Here the NRF_CHANNEL is defined. This channel must be the same for
// all nodes on the network. Choose a channel from 0 to 125.
#define NRF_CHANNEL 2

//  This network consists of three pipes in a broadcast configuration. 
//  which means that there are three pipes and each node has its own send pipe.
//  Select the node (which also determines the sending pipe): Node 0, Node 1 or Node 2

uint8_t Node  = 0;      char    ID[] = "0";
//uint8_t Node  = 1;    char    ID[] = "1";
//uint8_t Node  = 2;    char    ID[] = "2";

// Select which pipes the device should listen to (Subscribe)
uint8_t ListenTo_pipe1 = 1; // 0 or 1 (G)
uint8_t ListenTo_pipe2 = 1; // 0 or 1 (B)

// Define max buffer length
#define MAXBUF 32

// Define PWM period for the LED dimmers
#define PWM_PERIOD 500

// rx_flag will be raised by IRQ once an nRF message has arrived
volatile uint8_t rx_flag = 0;

// timer_flag is raised by timer/counter IRQ
volatile uint8_t timer_flag = 0;


// Array in which the bytes received are stored
uint8_t rx_packet[MAXBUF];

// Define broadcast pipes for sending data from different modules
uint8_t BroadcastPipe_0[5] = "0pipe";
uint8_t BroadcastPipe_1[5] = "1pipe";
uint8_t BroadcastPipe_2[5] = "2pipe";
uint8_t BroadcastPipe_3[5] = "3pipe"; //(Dummy pipe)

// Buffer to store data to be sent to the secondary
char buffer[MAXBUF];

//Function declarations
void timer_init(void);
void nrf_init(uint8_t b);

// =============================================
//             Function definitions
//==============================================

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

// Timer used to periodically send values through nRF
ISR(TCE0_OVF_vect)
{
    timer_flag = 1;
}

// Initialize timer
void timer_init(void)
{
    TCE0.CTRLB = TC_WGMODE_NORMAL_gc;
    TCE0.CTRLA = TC_CLKSEL_DIV1024_gc;
    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;
    TCE0.PER = 31249;
}

// Initialize nRF
void nrf_init(uint8_t b)
{
    nrfspiInit();
    nrfBegin();
    nrfSetRetries(NRF_SETUP_ARD_1000US_gc, NRF_SETUP_ARC_NORETRANSMIT_gc);  //No re-transmit for broadcast
    nrfSetPALevel(NRF_RF_SETUP_PWR_6DBM_gc);
    nrfSetDataRate(NRF_RF_SETUP_RF_DR_250K_gc);
    nrfSetCRCLength(NRF_CONFIG_CRC_16_gc);
    nrfSetChannel(NRF_CHANNEL);

    nrfSetAutoAck(0);                                                       //No acknoledgement for broadcast
    nrfEnableDynamicPayloads();
    nrfClearInterruptBits();
    nrfFlushRx();
    nrfFlushTx();

    // Interrupt Pin
    NRF24_IRQ_PORT.INT0MASK |= NRF24_IRQ_PIN;
    NRF24_IRQ_PORT.NRF24_IRQ_CTRL = PORT_ISC_FALLING_gc;

    // Interrupts On
    NRF24_IRQ_PORT.INTCTRL |=
        (NRF24_IRQ_PORT.INTCTRL & ~PORT_INT0LVL_gm) | PORT_INT0LVL_LO_gc;

    // Open pipes to dummy channel (dit is een fix die netter moet)
    nrfOpenReadingPipe(0, (uint8_t *)BroadcastPipe_3);  // open reading pipe 0 to dummy pipe
    nrfOpenReadingPipe(1, (uint8_t *)BroadcastPipe_3);  // open reading pipe 1 to dummy pipe
    nrfOpenReadingPipe(2, (uint8_t *)BroadcastPipe_3);  // open reading pipe 2 to dummy pipe

    // Open reading pipes (selected at start of this script)
    if(ListenTo_pipe1){
        nrfOpenReadingPipe(1, (uint8_t *)BroadcastPipe_1);  // open reading pipe 1 to BroadcastPipe_1
    }
    if(ListenTo_pipe2){
        nrfOpenReadingPipe(2, (uint8_t *)BroadcastPipe_2);  // open reading pipe 2 to BroadcastPipe_2
    }

    // Opening writing pipe. Writing pipe depends on Node number
    if(Node == 0){
        nrfOpenWritingPipe((uint8_t *)BroadcastPipe_0);
    }
    else if(Node == 1){
        nrfOpenWritingPipe((uint8_t *)BroadcastPipe_1);
    } 
    else if(Node == 2){
        nrfOpenWritingPipe((uint8_t *)BroadcastPipe_2);
    }
    else if(Node == 3){
        nrfOpenWritingPipe((uint8_t *)BroadcastPipe_3);
    }
    else{
        printf("Error - wrong Node selection");
    }
    nrfStartListening();
    nrfPowerUp();
}

void ucg_init(ucg_t *ucg) {
  ucg_com_fnptr ucg_xmega_func = &ucg_com_xmega_cb;
  ucg_Init(ucg, ucg_dev_st7735_18x128x160, ucg_ext_st7735_18, ucg_xmega_func);
}


//Start main function
int main(void){
    pcf8563_time_t rtc_time;
    init_clock();           // Set CPU of xmega to 32MHz
    init_stream(F_CPU);     // Enable input/output stream via UARTF0
    timer_init();           // Initialize and start timer
    nrf_init(Node);         // Initialize nrf library
    i2c_init(&TWIE, TWI_BAUD(F_CPU, BAUD_100K));
    LED_init();             // Initialize LEDs
    ucg_t ucg;              // Initialiseer het scherm
    ucg_init(&ucg);
    ucg_SetRotate90(&ucg);
    ucg_SetFontMode(&ucg, UCG_FONT_MODE_TRANSPARENT);
    ucg_ClearScreen(&ucg);
    ucg_SetFont(&ucg, ucg_font_8x13_mr);

    // Enable lo priority interrupts and turn interrupts on globally
    PMIC.CTRL |= PMIC_LOLVLEN_bm;
    sei();

    //Indentify the node 
    printf("Hi, I am Node %d\n",Node);
    fflush(stdout);
   uint8_t current_brightness = 0;

    //Start the loop
    while (1) {
        // Time to send something
        if (timer_flag) {
            pcf8563_get_time(&rtc_time); 
            msg_time_t msg_send;
            msg_send.info.type = MSG_TIME;
            msg_send.info.user_id = Node;
            msg_send.hour = rtc_time.hour;
            msg_send.minute = rtc_time.minute;
            msg_send.second =  rtc_time.second;

            char time_str[20];
            snprintf(time_str, sizeof(time_str),
            "%02u:%02u:%02u", rtc_time.hour, rtc_time.minute, rtc_time.second);

            ucg_ClearScreen(&ucg);
            ucg_DrawString(&ucg, 10, 20, 0, time_str);
            pcf8563_print_time(&rtc_time);

            nrfStopListening();
            nrfWrite((uint8_t *)&msg_send, sizeof(msg_send));
            nrfStartListening();

            //Reset timer flag
            timer_flag = 0;
        }
        
        // Check if there's data to be read from 
        if (rx_flag) {
            rx_flag = 0;
            msg_info_t info;
            memcpy(&info, rx_packet, sizeof(info));

            if (info.type == MSG_LIGHT){
                msg_light_t msg_light;
                memcpy(&msg_light, rx_packet, sizeof(msg_light));
                printf("Light from: %d: %d\n", msg_light.info.user_id, msg_light.light_percent);
                fflush(stdout);
                
                char Light [20];
                snprintf(Light, sizeof(Light), "Light: %d", msg_light.light_percent);
                ucg_DrawString(&ucg, 10, 40, 0, Light);

                current_brightness = (uint8_t)msg_light.light_percent;
            }

            else if (info.type == MSG_TIME){
                msg_time_t msg_time;
                memcpy(&msg_time, rx_packet, sizeof(msg_time));
                printf("Time from: %d: %02u:%02u:%02u\n", msg_time.info.user_id, msg_time.hour, msg_time.minute, msg_time.second);
            }

            else if (info.type == MSG_TEMP){
                msg_temp_t msg_temp;
                memcpy(&msg_temp, rx_packet, sizeof(msg_temp));
                printf("Temp from %d: %d\n", msg_temp.info.user_id, msg_temp.temperature);

                char Temp [20];
                snprintf(Temp, sizeof(Temp), "Temp: %d", msg_temp.temperature);
                ucg_DrawString(&ucg, 10, 60, 0, Temp);
            }
        }
        LED_set_brightness(current_brightness);
        //Timeout check 
        if(0){
            // Do something when no message received for some time
        }

    }
}
