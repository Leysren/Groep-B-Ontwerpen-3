#ifndef SPI_H
#define SPI_H

#include <stdint.h>

#define SPI_DEV  SPID
#define SPI_PORT PORTD

#define SPI_SS_bm   PIN4_bm
#define SPI_MOSI_bm PIN5_bm
#define SPI_MISO_bm PIN6_bm
#define SPI_SCK_bm  PIN7_bm

void spi_init(void);
uint8_t spi_read_write(uint8_t);
void spi_write(uint8_t data);
void spi_write_block(uint8_t *data, uint16_t len);

#endif /* SPI_H */
