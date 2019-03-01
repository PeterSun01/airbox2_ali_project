#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "esp_system.h"
#include "esp_heap_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"

#include "tft_ctl.h"
#include "Led.h"
#include "E2prom.h"
#include "RtcUsr.h"
#include "Key.h"
#include "Beep.h"
#include "sht31.h"
#include "ccs811.h"
#include "PM25.h"
#include "formaldehyde.h"

/*
co2     400-8192
tvoc    0-1187(999) 
pm2.5   0-1000
甲醛     0-5000


*/


void app_main(void)
{

  ESP_ERROR_CHECK( nvs_flash_init() );

  i2c_init();
  static ccs811_sensor_t* sensor ;
  sensor= ccs811_init_sensor();
  tft_init();
  PM25_Init();
  formaldehyde_Init();

  double Temperature;
  double Humidity;
  char Temperature_c[20];
  char Humidity_c[20];

  uint16_t tvoc;
  uint16_t eco2;

  while(1)
  {
    
    if (sht31_readTempHum(&Temperature,&Humidity)) 
    {
      //ccs811_set_environmental_data(sensor,(float)Temperature,(float)Humidity);//补偿CCS811
      ESP_LOGI("SHT30", "Temperature=%.1f, Humidity=%.1f", Temperature, Humidity);
      font_transparent = 0; //有背景
      TFT_setFont(DEJAVU24_FONT, NULL);
      sprintf(Temperature_c,"%02d",(int)Temperature);//左对齐，2长度
      sprintf(Humidity_c,"%02d",(int)Humidity);
      _fg = TFT_WHITE;
      TFT_print(Temperature_c, 25, 3);
      TFT_print(Humidity_c, 97, 3);
    }

    if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
    {
      printf("TVOC=%4d(ppb), eCO2=%4d(ppm)\n", tvoc, eco2);

      if(tvoc<=200)
      {
        _fg = TFT_GREEN;
        tft_print_fields(4,tvoc);
      }
      else if((tvoc>200)&&(tvoc<=400))
      {
        _fg = TFT_YELLOW;
        tft_print_fields(4,tvoc);
      }
      else if((tvoc>400)&&(tvoc<=999))
      {
        _fg = TFT_RED;
        tft_print_fields(4,tvoc);
      }

      if(eco2<=800)
      {
        _fg = TFT_GREEN;
        tft_print_fields(3,eco2);
      }
      else if((eco2>800)&&(eco2<=1200))
      {
        _fg = TFT_YELLOW;
        tft_print_fields(3,eco2);
      }
      else if((eco2>1200)&&(eco2<=8192))
      {
        _fg = TFT_RED;
        tft_print_fields(3,eco2);
      }
    }

    if(PM2_5<=150)
    {
      _fg = TFT_GREEN;
      tft_print_fields(2,PM2_5);
    }
    else if((PM2_5>150)&&(PM2_5<=300))
    {
      _fg = TFT_YELLOW;
      tft_print_fields(2,PM2_5);
    }
    else if((PM2_5>300)&&(PM2_5<=999))
    {
      _fg = TFT_RED;
      tft_print_fields(2,PM2_5);
    }
    else if((PM2_5>999))
    {
      _fg = TFT_RED;
      tft_print_fields(2,999);
    }

    if(formaldehyde_ug<=60)
    {
      _fg = TFT_GREEN;
      tft_print_fields(1,formaldehyde_ug);
    }
    else if((formaldehyde_ug>60)&&(formaldehyde_ug<=80))
    {
      _fg = TFT_YELLOW;
      tft_print_fields(1,formaldehyde_ug);
    }
    else if((formaldehyde_ug>80)&&(formaldehyde_ug<=5000))
    {
      _fg = TFT_RED;
      tft_print_fields(1,formaldehyde_ug);
    }




    vTaskDelay(1500 / portTICK_RATE_MS);
  }

}
