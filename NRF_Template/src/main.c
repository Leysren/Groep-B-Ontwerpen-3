/*
 * nRF24L01+ Broadcast example
 *
 * Jasper Flohil 29/01/2026
 *
 * This example contains code for a Publish / Subscribe configuration of three Nodes
 * Each node sends on its own pipe and listens on the other two pipes. The
 * nodes write data on a timer. 
 * 
 * Each node reperesents a color, Node 0 = Red, Node 1 = Green, Node 2 is Blue
 * Each node has a potmeter toi set the corresponding color and sends the value
 * through NRF on its own pipe.
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

// Here the NRF_CHANNEL is defined. This channel must be the same for
// all nodes on the network. Choose a channel from 0 to 125.
#define NRF_CHANNEL 27

//  This network consists of three pipes in a broadcast configuration. 
//  which means that there are three pipes and each node has its own send pipe.
//  Select the node (which also determines the sending pipe): Node 0, Node 1 or Node 2

//uint8_t Node  = 0;      char    ID[] = "0";
//uint8_t Node  = 1;    char    ID[] = "1";
uint8_t Node  = 2;    char    ID[] = "2";

// Select which pipes the device should listen to (Subscribe)
uint8_t ListenTo_pipe0 = 1; // 0 or 1 (R)
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

// Timer settings
#define T0_PER 31251


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
void LED_init(void);
void ADC_init(void);
uint16_t  ADC_read(void);


//Start main function
int main(void){
    
    init_clock();           // Set CPU of xmega to 32MHz
    init_stream(F_CPU);     // Enable input/output stream via UARTF0
    timer_init();           // Initialize and start timer
    LED_init();             // Initialize the LED pin settings
    ADC_init();             // Initialize the ADC for the potentiometer
    nrf_init(Node);         // Initialize nrf library

    // Enable lo priority interrupts and turn interrupts on globally
    PMIC.CTRL |= PMIC_LOLVLEN_bm;
    sei();

    //Indentify the node 
    printf("Hi, I am Node %d\n",Node);

    //Start the loop
    while (1) {
        // Time to send something
        if (timer_flag) {
            
            // Read potentionmeter
            uint16_t res = ADC_read();
            
            //printf("Sending ADC value %d\n", res);

            // Load value into buffer and send
            sprintf(buffer, "%d%d", Node,res);
            nrfStopListening();
            cli();
            nrfWrite((uint8_t *)buffer, strlen(buffer));
            sei();
            nrfStartListening();

            //Set own color
            if(Node == 0){
                TCF0.CCB = res;  // Red on
            }
            else if(Node == 1){
                TCF0.CCA = res; // Green onm
            }
            else if(Node == 2){
                TCC0.CCA = res;  // Blue on
            }
            //Reset timer flag
            timer_flag = 0;
        }
        

        // Check if there's data to be read from 
        if (rx_flag) {
            rx_flag = 0;
            char tmp[5];
            memcpy(tmp, &rx_packet[1], 4);
            tmp[4] = '\0';
            // Convert char array to integer
            uint16_t value = atoi(tmp);
            printf("Received from Node: %d\n", value);
        
            //Receive package and check the source
            //Depending on source, show LED R,G or B
            if (rx_packet[0] == '0' ) {
                //printf("Received from Node 0\n");
                TCF0.CCB = value;  // Set value Red LED
            } 
            else if (rx_packet[0] == '1' ) {
                //printf("Received from Node 1\n");
                TCF0.CCA = value;  // Set value Green LED
            }
            else if (rx_packet[0] == '2' ) {
                //printf("Received from Node 2\n");
                TCC0.CCA = value;  // Set value Blue LED
            }
        }
        //Timeout check (dit mogen jullie zelf verzinnen!)
        if(0){
            // Do something when no message received for some time
        }
    }
}

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
        rx_packet[packet_length] = '\0';
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
    TCE0.CTRLA = TC_CLKSEL_DIV64_gc;
    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;
    TCE0.PER = T0_PER;
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
    if(ListenTo_pipe0){
        nrfOpenReadingPipe(0, (uint8_t *)BroadcastPipe_0);  // open reading pipe 0 to BroadcastPipe_0
    }
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
    else{
        printf("Error - wrong Node selection");
    }
    nrfStartListening();
    nrfPowerUp();
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

//Initialize ADC settings
void ADC_init(void)
{
    /* PA7 as analog input */
    PORTA.DIRCLR = PIN7_bm;
    PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;

    /* Enable ADCA */
    ADCA.CTRLA = ADC_ENABLE_bm;

    /* 12-bit resolution */
    ADCA.CTRLB = ADC_RESOLUTION_12BIT_gc;

    /* Reference = VCC */
    ADCA.REFCTRL = ADC_REFSEL_INTVCC_gc;

    /* ADC clock prescaler
       32 MHz / 64 = 500 kHz (within spec) */
    ADCA.PRESCALER = ADC_PRESCALER_DIV64_gc;

    /* Channel 0: single-ended, PA7 */
    ADCA.CH0.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;
    ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN7_gc;
}

// Function for reading ADC
uint16_t ADC_read(void)
{
    ADCA.CH0.CTRL |= ADC_CH_START_bm;

    while (!(ADCA.CH0.INTFLAGS & ADC_CH_CHIF_bm))
        ;

    ADCA.CH0.INTFLAGS = ADC_CH_CHIF_bm;  // clear flag
    uint16_t tmp; 
    ;
    //Only send value when ADC > 200, otherwise send 0 (otherwise the LED's will no fully dim)
    if(ADCA.CH0.RES>200){
        tmp = ADCA.CH0.RES - 200;
    }
    else{
        tmp = 0;
    }
    return tmp;
}