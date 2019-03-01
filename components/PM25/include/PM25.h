
#ifndef _PM25_H_
#define _PM25_H_
#include "freertos/FreeRTOS.h"

//本例使用的传感器为益衫A4-CG

uint16_t PM2_5;
uint16_t PM10;


extern void PM25_Init(void);


#endif

