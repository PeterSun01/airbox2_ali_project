
#ifndef _TFT_CLT_H_
#define _TFT_CTL_H_

#include "freertos/FreeRTOS.h"

extern void tft_init();
extern uint8_t tft_print_fields(uint8_t field_no,uint16_t data);
extern void tft_main_board(void);
extern unsigned int rand_interval(unsigned int min, unsigned int max);



#endif

