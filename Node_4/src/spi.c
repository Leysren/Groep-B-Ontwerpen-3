#include "spi.h"

#include <stddef.h>
#include <avr/io.h>

static inline void _activate_slave(void);
static inline void _deactivate_slave(void);

void spi_init(void)
{
    // Configure the SPI master pins as output
    SPI_PORT.DIRSET = (SPI_SCK_bm|SPI_MOSI_bm|SPI_SS_bm);
    SPI_PORT.DIRCLR = SPI_MISO_bm;
    // Set nSS pin to idle (high, since it is active low)
    SPI_PORT.OUTSET = SPI_SS_bm;
    // enable SPI
    SPI_DEV.CTRL =
        SPI_ENABLE_bm |
        SPI_MASTER_bm |
        SPI_CLK2X_bm  |
        SPI_MODE_0_gc |
        SPI_PRESCALER_DIV4_gc;
    /*
      Enable the following in SPI_DEV.CTRL for :
        SPI_CLK2X_bm -> double clock speed
        SPI_DORD_bm  -> LSB is transmitted first
    */
}

uint8_t spi_read_write(uint8_t data)
{
    _activate_slave();

    SPI_DEV.DATA = data;

    // Wait for the interrupt flag IF to be set
    while (!(SPI_DEV.STATUS & SPI_IF_bm))
        ;
    data = SPI_DEV.DATA;

    _deactivate_slave();

    return data;
}

void spi_write(uint8_t data)
{
    _activate_slave();

    SPI_DEV.DATA = data;

    // Wait for the interrupt flag IF to be set
    while (!(SPI_DEV.STATUS & SPI_IF_bm))
        ;

    // Manually reset the interrupt flag since we're not reading here
    SPI_DEV.INTCTRL &= ~SPI_IF_bm;

    _deactivate_slave();
}

void spi_write_block(uint8_t *data, uint16_t len)
{
    if (data != NULL) {
        _activate_slave();
        for (uint8_t i=0; i<len; i++) {
            SPI_DEV.DATA = data[i];
            while (!(SPI_DEV.STATUS & SPI_IF_bm))
                ;
            // Manually reset the interrupt flag since we're not reading here
            SPI_DEV.INTCTRL &= ~SPI_IF_bm;
        }
        _deactivate_slave();
    }
}

static inline void _activate_slave()
{
    SPI_PORT.OUTCLR = SPI_SS_bm;
}

static inline void _deactivate_slave()
{
    SPI_PORT.OUTSET = SPI_SS_bm;
}
