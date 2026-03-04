/*!
 *  \file    clock.c
 *  \author  Wim Dolman 
 *  \date    10-07-2019
 *  \version 1.4
 *
 *  \brief   Clock functions for Xmega 
 *
 */
#include <avr/io.h>

void Config32MHzClock(void)
{
  OSC.CTRL |= OSC_RC32MEN_bm;
  while(!(OSC.STATUS & OSC_RC32MRDY_bm));
  CCP = CCP_IOREG_gc;
  CLK.CTRL = CLK_SCLKSEL_RC32M_gc;
}

void Config32MHzClock_Ext16M(void)
{
  OSC.XOSCCTRL = OSC_FRQRANGE_12TO16_gc | OSC_XOSCSEL_XTAL_16KCLK_gc;
  OSC.CTRL |= OSC_XOSCEN_bm;
  while ( ! (OSC.STATUS & OSC_XOSCRDY_bm) );

  OSC.PLLCTRL = OSC_PLLSRC_XOSC_gc | (OSC_PLLFAC_gm & 2);
  OSC.CTRL |= OSC_PLLEN_bm;
  while ( ! (OSC.STATUS & OSC_PLLRDY_bm) );

  CCP = CCP_IOREG_gc;
  CLK.CTRL = CLK_SCLKSEL_PLL_gc;
  OSC.CTRL &= ~OSC_RC2MEN_bm;
  OSC.CTRL &= ~OSC_RC32MEN_bm;
}

void Config16MHzClock_Ext16M(void)
{
  OSC.XOSCCTRL = OSC_FRQRANGE_12TO16_gc | OSC_XOSCSEL_XTAL_16KCLK_gc;
  OSC.CTRL |= OSC_XOSCEN_bm;
  while ( ! (OSC.STATUS & OSC_XOSCRDY_bm) );

  OSC.PLLCTRL = OSC_PLLSRC_XOSC_gc | (OSC_PLLFAC_gm & 1);
  OSC.CTRL |= OSC_PLLEN_bm;
  while ( ! (OSC.STATUS & OSC_PLLRDY_bm) );

  CCP = CCP_IOREG_gc;
  CLK.CTRL = CLK_SCLKSEL_PLL_gc;
  OSC.CTRL &= ~OSC_RC2MEN_bm;
  OSC.CTRL &= ~OSC_RC32MEN_bm;
}

void AutoCalibration32M(void)
{
  OSC.CTRL       |= OSC_RC32KEN_bm;
  do {} while ( (OSC.STATUS & OSC_RC32KRDY_bm ) == 0 );
  OSC.DFLLCTRL   &= ~(OSC_RC32MCREF_gm);
  OSC.DFLLCTRL   |=   OSC_RC32MCREF_RC32K_gc;
  DFLLRC32M.CTRL |= DFLL_ENABLE_bm;
}

void AutoCalibration2M(void)
{
  OSC.CTRL      |= OSC_RC32KEN_bm;
  while(!(OSC.STATUS & OSC_RC32KRDY_bm));
  OSC.DFLLCTRL  &= ~(OSC_RC2MCREF_bm);
  OSC.DFLLCTRL  |=   OSC_RC2MCREF_RC32K_gc;
  DFLLRC2M.CTRL |= DFLL_ENABLE_bm;
}

void AutoCalibrationTosc32M(void)
{
  OSC.XOSCCTRL   |= OSC_XOSCSEL_32KHz_gc;
  OSC.CTRL       |= OSC_XOSCEN_bm;
  while(!(OSC.STATUS & OSC_XOSCRDY_bm));
  OSC.DFLLCTRL   &= ~(OSC_RC32MCREF_gm);
  OSC.DFLLCTRL   |=   OSC_RC32MCREF_XOSC32K_gc;
  DFLLRC32M.CTRL |= DFLL_ENABLE_bm;
}

void AutoCalibrationTosc2M(void)
{
  OSC.XOSCCTRL  |= OSC_XOSCSEL_32KHz_gc;
  CCP = CCP_IOREG_gc;
  OSC.CTRL      |= OSC_XOSCEN_bm;
  while(!(OSC.STATUS & OSC_XOSCRDY_bm));
  OSC.DFLLCTRL  &= ~(OSC_RC2MCREF_bm);
  OSC.DFLLCTRL  |= OSC_RC2MCREF_XOSC32K_gc;
  DFLLRC2M.CTRL |= DFLL_ENABLE_bm;
}