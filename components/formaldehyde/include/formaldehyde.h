
#ifndef _FORMALDEHYDE_H_
#define _FORMALDEHYDE_H_
#include "freertos/FreeRTOS.h"

//本例使用的甲醛传感器为WZ-S

uint16_t formaldehyde_ppb;
uint16_t formaldehyde_ug;

uint8_t formaldehyde_flag;


extern void formaldehyde_Init(void);


#endif

