#define F_CPU 32000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <util/delay.h>
#include <string.h>

#include "clock.h"
#include "serialF0.h"
#include "nrf24L01.h"
#include "nrf24spiXM2.h"
#include "main.h"

#define MAX_VALUE   2047                                   // only 11 bits are used
#define VCC         3.30
#define VREF        (((double) VCC) / 1.6)                 // is 2.06125

// Here the NRF_CHANNEL is defined. This channel must be the same for
// all nodes on the network. Choose a channel from 0 to 125.
#define NRF_CHANNEL 2

// Define max buffer length
#define MAXBUF 32

// Define PWM period for the LED dimmers
#define PWM_PERIOD 500

uint8_t licht =0;

//  This network consists of three pipes in a broadcast configuration. 
//  which means that there are three pipes and each node has its own send pipe.
//  Select the node (which also determines the sending pipe): Node 0, Node 1 or Node 2

uint8_t Node  = 0;      char    ID[] = "0";
//uint8_t Node  = 1;    char    ID[] = "1";
//uint8_t Node  = 2;    char    ID[] = "2";

// Select which pipes the device should listen to (Subscribe)
uint8_t ListenTo_pipe1 = 1; // 0 or 1 (G)
uint8_t ListenTo_pipe2 = 1; // 0 or 1 (B)

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

void init_adc(void)
{
  PORTA.DIRCLR     = PIN2_bm|PIN3_bm;                      // PA3 can be used for offset
  ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN2_gc |               // PA2 to + channel 0
                     ADC_CH_MUXNEG_GND_MODE3_gc;           // GND to - channel 0
  ADCA.CH0.CTRL    = ADC_CH_INPUTMODE_DIFF_gc;             // channel 0 differential
  ADCA.REFCTRL     = ADC_REFSEL_INTVCC_gc;
  ADCA.CTRLB       = ADC_RESOLUTION_12BIT_gc |
                     ADC_CONMODE_bm;                       // signed conversion
  ADCA.PRESCALER   = ADC_PRESCALER_DIV16_gc;
  ADCA.CTRLA       = ADC_ENABLE_bm;
}

int16_t read_adc(void)                                     // return a signed
{
  int16_t res;                                             // is also signed

  ADCA.CH0.CTRL |= ADC_CH_START_bm;
  while ( !(ADCA.CH0.INTFLAGS & ADC_CH_CHIF_bm) ) ;
  res = ADCA.CH0.RES;
  ADCA.CH0.INTFLAGS |= ADC_CH_CHIF_bm;

  return res;
}

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

int main(void)
{
  int16_t res;                                             // is also signed
  init_stream(F_CPU);
  init_clock();
  timer_init();
  nrf_init(Node);
  init_adc();

  PMIC.CTRL |= PMIC_LOLVLEN_bm;
  sei();

  PORTD_DIRSET = PIN1_bm;
  PORTD_DIRSET = PIN2_bm;

  printf("Hi, I am Node %d\n",Node);

  while (1) {
    res = read_adc();
    // int32_t vin_mv;
    // vin_mv = ((int32_t)res * 2063) / 2048;

    //printf("res: %4d  spanning: %ld mV\n", res, vin_mv);
    msg_light_t msg_light;
    if (res < 80){ // test waarde
      PORTD_OUTSET = PIN1_bm;
      PORTD_OUTCLR = PIN2_bm;
      msg_light.info.type = MSG_LIGHT;
      msg_light.info.user_id = Node; 
      msg_light.light_percent = 1; // lamp aan
    }
    else {
      PORTD_OUTCLR = PIN1_bm;
      PORTD_OUTCLR = PIN2_bm;
      msg_light.info.type = MSG_LIGHT;
      msg_light.info.user_id = Node; 
      msg_light.light_percent = 0; // lamp uit
    } 
    if (timer_flag) {
            nrfStopListening();
            cli();
            nrfWrite((uint8_t *)&msg_light, sizeof(msg_light));
            sei();
            nrfStartListening();
            printf("light_percent: %d\n", msg_light.light_percent);

            //Reset timer flag
            timer_flag = 0;
        }
        
        //Timeout check 
        if(0){
            // Do something when no message received for some time
        }
    }
}
