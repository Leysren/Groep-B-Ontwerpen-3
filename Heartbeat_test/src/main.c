/* ========================= main.c =========================
 * XMEGA @ 32MHz
 * - TCE0 maakt echte millis() (1ms tick)
 * - Print 1x per seconde via millis()
 * - SpO2 update 1x per seconde via millis()
 *
 * BELANGRIJK:
 * 1) Verwijder de nep-millis() uit max30102.c (zie max30102.c hieronder).
 * 2) Zorg dat PMIC low-level interrupts aan staan (doen we hier).
 */

#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "clock.h"
#include "serialF0.h"
#include "i2c.h"
#include "nrf24spiXM2.h"
#include "nrf24L01.h"
#include "max30102.h"

// Define max buffer length
#define MAXBUF 32

// Here the NRF_CHANNEL is defined. This channel must be the same for
// all nodes on the network. Choose a channel from 0 to 125.
#define NRF_CHANNEL 2

//  This network consists of three pipes in a broadcast configuration. 
//  which means that there are three pipes and each node has its own send pipe.
//  Select the node (which also determines the sending pipe): Node 0, Node 1 or Node 2

//uint8_t Node  = 0;      char    ID[] = "0";
uint8_t Node  = 1;    char    ID[] = "1";
//uint8_t Node  = 2;    char    ID[] = "2";

// Select which pipes the device should listen to (Subscribe)
uint8_t ListenTo_pipe0 = 1; // 0 or 1 (R)
uint8_t ListenTo_pipe1 = 1; // 0 or 1 (G)
uint8_t ListenTo_pipe2 = 1; // 0 or 1 (B)
uint8_t ListenTo_pipe3 = 1; // 0 or 1 (B)


// rx_flag will be raised by IRQ once an nRF message has arrived
volatile uint8_t rx_flag = 0;

// Array in which the bytes received are stored
uint8_t rx_packet[MAXBUF];

// Define broadcast pipes for sending data from different modules
uint8_t BroadcastPipe_0[5] = "0pipe";
uint8_t BroadcastPipe_1[5] = "1pipe";
uint8_t BroadcastPipe_2[5] = "2pipe";
uint8_t BroadcastPipe_3[5] = "3pipe";
uint8_t BroadcastPipe_4[5] = "3pipe"; //(Dummy pipe)

// Buffer to store data to be sent to the secondary
char buffer[MAXBUF];

/* ---------- echte millis() via timer ---------- */
volatile uint32_t g_millis = 0;

uint32_t millis(void)
{
    uint32_t m;
    cli();
    m = g_millis;
    sei();
    return m;
}

/* ---------- nRF IRQ ---------- */
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

/* ---------- Timer: 1ms tick ---------- */
ISR(TCE0_OVF_vect)
{
    g_millis++;
}

static void timer_init(void)
{
    // 32MHz / 64 = 500kHz; 500 ticks = 1ms -> PER=499
    TCE0.CTRLB = TC_WGMODE_NORMAL_gc;
    TCE0.CTRLA = TC_CLKSEL_DIV64_gc;
    TCE0.PER   = 499;

    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;
    PMIC.CTRL |= PMIC_LOLVLEN_bm; // enable low level interrupts
}

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
    nrfOpenReadingPipe(0, (uint8_t *)BroadcastPipe_4);  // open reading pipe 0 to dummy pipe
    nrfOpenReadingPipe(1, (uint8_t *)BroadcastPipe_4);  // open reading pipe 1 to dummy pipe
    nrfOpenReadingPipe(2, (uint8_t *)BroadcastPipe_4);  // open reading pipe 2 to dummy pipe
    nrfOpenReadingPipe(3, (uint8_t *)BroadcastPipe_4);  // open reading pipe 2 to dummy pipe

    // Open reading pipes (selected at start of this script)
    if (ListenTo_pipe0) {
        nrfOpenReadingPipe(0, (uint8_t *)BroadcastPipe_0);
    }
    if (ListenTo_pipe1) {
        nrfOpenReadingPipe(1, (uint8_t *)BroadcastPipe_1);
    }
    if (ListenTo_pipe2) {
        nrfOpenReadingPipe(2, (uint8_t *)BroadcastPipe_2);
    }
    if (ListenTo_pipe3) {
        nrfOpenReadingPipe(3, (uint8_t *)BroadcastPipe_3);
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
    init_clock();
    init_stream(F_CPU);
    timer_init();
    nrf_init(Node);         // Initialize nrf library
    sei();

    i2c_init(&TWIE, TWI_BAUD(F_CPU, 100000));
    PORTE.DIRSET = PIN0_bm | PIN1_bm;
    PORTE.PIN0CTRL = PORT_OPC_WIREDANDPULL_gc;
    PORTE.PIN1CTRL = PORT_OPC_WIREDANDPULL_gc;
    PORTD.DIRSET = PIN1_bm;   // PD1 als output
    PORTD.OUTCLR = PIN1_bm;   // begin met LED uit

    printf("\r\n=== MAX30102 Hartslag + SpO2 Test ===\r\n");

    uint8_t partID = max30102_readRegister(MAX30102_PARTID);
    printf("Part ID: 0x%02X (moet 0x15 zijn)\r\n", partID);
    if (partID != 0x15) {
        printf("Sensor niet gevonden! Controleer aansluitingen.\r\n");
        while (1) {}
    }

    max30102_setup();
    printf("Sensor klaar. Plaats je vinger erop.\r\n\r\n");

    // buffer vullen
    for (int i = 0; i < BUFFER_SIZE; i++) {
        max30102_readFIFO(&irBuffer[i], &redBuffer[i]);
        _delay_ms(10); // ~100Hz
    }

    uint32_t lastPrint = millis();
    uint32_t lastSpO2  = millis();

    while (1) {
        uint32_t ir, red;
        max30102_readFIFO(&ir, &red);

        // shift buffer
        for (int i = 0; i < BUFFER_SIZE - 1; i++) {
            irBuffer[i] = irBuffer[i + 1];
            redBuffer[i] = redBuffer[i + 1];
        }
        irBuffer[BUFFER_SIZE - 1]  = ir;
        redBuffer[BUFFER_SIZE - 1] = red;

        // beat detect
        (void)max30102_checkForBeat(ir);

        uint32_t now = millis();

        // SpO2 1x per seconde
        if ((uint32_t)(now - lastSpO2) >= 1000) {
            lastSpO2 += 1000;
            max30102_calculateSpO2();
        }

        // print 1x per seconde
        if ((uint32_t)(now - lastPrint) >= 1000) {
            lastPrint += 1000;

            printf("IR:%6lu RED:%6lu BPM:%3d ", ir, red, beatAvg);
            if (beatAvg >= 40 && beatAvg <= 200) printf("OK ");
            printf("SpO2:%3ld ", spo2);
            if (validSPO2) printf("OK ");
            if (ir < 7000) {
                PORTD.OUTSET = PIN1_bm;   // PD1 AAN
                printf("Geen vinger?");
            } else {
                PORTD.OUTCLR = PIN1_bm;   // PD1 UIT
            }
            printf("\r\n");
            snprintf(buffer, sizeof(buffer), "BPM=%d SpO2=%ld", beatAvg, spo2);
            // nrfStopListening();
            // cli();
            // nrfWrite((uint8_t *)buffer, strlen(buffer));
            // sei();
            // nrfStartListening();
        }

        // sample pacing ~100Hz (match je sensor settings!)
        _delay_ms(10);

        if (rx_flag) {
            printf("hello");
            rx_flag = 0;
            msg_info_t info;
            memcpy(&info, rx_packet, sizeof(info));

            if (info.type == MSG_LIGHT){
                msg_light_t msg_light;
                memcpy(&msg_light, rx_packet, sizeof(msg_light));
                printf("Light from: %d: %d\n", msg_light.info.user_id, msg_light.light_percent);
            }
        }
        //Timeout check (dit mogen jullie zelf verzinnen!)
        if(0){
            // Do something when no message received for some time
        }
    }
}