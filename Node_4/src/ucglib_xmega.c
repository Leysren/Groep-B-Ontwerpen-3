#include "ucglib_xmega.h"

#include <stdint.h>
#include <avr/io.h>
#define F_CPU 32000000UL
#include <util/delay.h>
#include "spi.h"

static void my_init_spi_xmega(uint16_t sclk_period_ns);

static void my_delay_us(uint16_t);

int16_t ucg_com_xmega_cb(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data)
{
    switch(msg) {
    case UCG_COM_MSG_POWER_UP:
        /* "data" is a pointer to ucg_com_info_t structure with the following
           information: */
        /*	((ucg_com_info_t *)data)->serial_clk_speed value in nanoseconds */
        /*	((ucg_com_info_t *)data)->parallel_clk_speed value in nanoseconds */
        /* "arg" is not used */
        /* This message is sent once at the uC startup and for power up. */
        /* setup i/o or do any other setup */
        my_init_spi_xmega(((ucg_com_info_t *) data)->serial_clk_speed);
        break;

    case UCG_COM_MSG_POWER_DOWN:
        /* "data" and "arg" are not used*/
        /* This message is sent for a power down request */

        break;
    case UCG_COM_MSG_DELAY:
        /* "data" is not used */
        /* "arg" contains the number of microseconds for the delay */
        /* By receiving this message, the following code should delay by */
        /* "arg" microseconds. One microsecond is 0.000001 second */
        my_delay_us(arg);
        break;
    case UCG_COM_MSG_CHANGE_RESET_LINE:
        /* "data" is not used */
        /* "arg" = 1: set the reset output line to 1 */
        /* "arg" = 0: set the reset output line to 0 */
        if (arg) {
            UCGLIB_PORT.OUTSET = UCGLIB_RST_bm;
        } else {
            UCGLIB_PORT.OUTCLR = UCGLIB_RST_bm;
        }

        break;
    case UCG_COM_MSG_CHANGE_CD_LINE:
        /* "ucg->com_status"  bit 0 contains the old level for the CD line */
        /* "data" is not used */
        /* "arg" = 1: set the command/data (a0) output line to 1 */
        /* "arg" = 0: set the command/data (a0) output line to 0 */
        if (arg) {
            UCGLIB_PORT.OUTSET = UCGLIB_CD_bm;
        } else {
            UCGLIB_PORT.OUTCLR = UCGLIB_CD_bm;
        }

        break;
    case UCG_COM_MSG_CHANGE_CS_LINE:
        /* "ucg->com_status"  bit 1 contains the old level for the CS line */
        /* "data" is not used */
        /* "arg" = 1: set the chipselect output line to 1 */
        /* "arg" = 0: set the chipselect output line to 0 */
        if (arg) {
            SPI_PORT.OUTSET = SPI_SS_bm;
        } else {
            SPI_PORT.OUTCLR = SPI_SS_bm;
        }

        break;
    case UCG_COM_MSG_SEND_BYTE:
        /* "data" is not used */
        /* "arg" contains one byte, which should be sent to the display */
        /* The current status of the CD line is available */
        /* in bit 0 of u8g->com_status */
        spi_write(arg);

        break;
    case UCG_COM_MSG_REPEAT_1_BYTE:
        /* "data[0]" contains one byte */
        /* repeat sending the byte in data[0] "arg" times */
        /* The current status of the CD line is available */
        /* in bit 0 of u8g->com_status */
        while (arg--) {
            spi_write(data[0]);
        }

        break;
    case UCG_COM_MSG_REPEAT_2_BYTES:
        /* "data[0]" contains first byte */
        /* "data[1]" contains second byte */
        /* repeat sending the two bytes "arg" times */
        /* The current status of the CD line is available */
        /* in bit 0 of u8g->com_status */
        while (arg--) {
            spi_write_block(data, 2);
        }

        break;
    case UCG_COM_MSG_REPEAT_3_BYTES:
        /* "data[0]" contains first byte */
        /* "data[1]" contains second byte */
        /* "data[2]" contains third byte */
        /* repeat sending the three bytes "arg" times */
        /* The current status of the CD line is available */
        /* in bit 0 of u8g->com_status */
        while (arg--) {
            spi_write_block(data, 3);
        }

        break;
    case UCG_COM_MSG_SEND_STR:
        /* "data" is an array with "arg" bytes */
        /* send "arg" bytes to the display */
        spi_write_block(data, arg);

        break;
    case UCG_COM_MSG_SEND_CD_DATA_SEQUENCE:
        /* "data" is a pointer to two bytes, which contain the cd line */
        /* status and display data */
        /* "arg" contains the number of these two byte tuples which need to */
        /* be analysed and sent. Bellow is a example sequence */
        /* The content of bit 0 in u8g->com_status is undefined for this message */

        while(arg > 0) {
            if (*data != 0) {
                if (*data == 1) {
                    /* set CD (=D/C=A0) line to low */
                    UCGLIB_PORT.OUTCLR = UCGLIB_CD_bm;
                } else {
                    /* set CD (=D/C=A0) line to high */
                    UCGLIB_PORT.OUTSET = UCGLIB_CD_bm;
                }
            }
            data++;
            /* send *data to the display */
            spi_write(*data);
            data++;
            arg--;
        }
        break;
    default:
        return 1;
    }

    return 1;
}

static void my_init_spi_xmega(uint16_t sclk_period_ns)
{
    // Configure pins RST and CD as output and set output low
    UCGLIB_PORT.DIRSET = (UCGLIB_RST_bm | UCGLIB_CD_bm);
    UCGLIB_PORT.OUTCLR = (UCGLIB_RST_bm | UCGLIB_CD_bm);

    spi_init();
}

static void my_delay_us(uint16_t us)
{
    _delay_us(us);
}
