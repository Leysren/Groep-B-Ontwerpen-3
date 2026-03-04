
#ifndef ADC_LIGHT_H
#define ADC_LIGHT_H

void init_adc(void)
{
    PORTA.DIRCLR = PIN2_bm;                          // PA2 as input
    ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN2_gc;        // PA2 to + channel 0
    ADCA.CH0.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc; // Single-ended mode
    ADCA.REFCTRL = ADC_REFSEL_INTVCC_gc;             // VCC/1.6 as reference
    ADCA.CTRLB = ADC_RESOLUTION_12BIT_gc;            // 12-bit UNSIGNED (remove CONMODE_bm)
    ADCA.PRESCALER = ADC_PRESCALER_DIV16_gc;
    ADCA.CTRLA = ADC_ENABLE_bm;
}


uint16_t read_adc(void)  
{
    uint16_t res; 

    ADCA.CH0.CTRL |= ADC_CH_START_bm;
    while (!(ADCA.CH0.INTFLAGS & ADC_CH_CHIF_bm));
    res = ADCA.CH0.RES;
    ADCA.CH0.INTFLAGS |= ADC_CH_CHIF_bm;

    return res;
}

#endif //ADC light library