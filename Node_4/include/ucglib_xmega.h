#ifndef UCGLIB_XMEGA_H
#define UCGLIB_XMEGA_H

#include "ucglib/csrc/ucg.h"

#define UCGLIB_PORT   PORTD
#define UCGLIB_RST_bm PIN3_bm
#define UCGLIB_CD_bm  PIN2_bm

int16_t ucg_com_xmega_cb(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data);

#endif /* UCGLIB_XMEGA_H */
