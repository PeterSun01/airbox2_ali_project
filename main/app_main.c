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






void app_main(void)
{

  ESP_ERROR_CHECK( nvs_flash_init() );

  i2c_init();
  tft_init();
  static ccs811_sensor_t* sensor ;
  sensor= ccs811_init_sensor();

  double Temperature;
  double Humidity;
  char Temperature_c[20];
  char Humidity_c[20];

  uint16_t tvoc;
  uint16_t eco2;
  char tvoc_c[20];
  char eco2_c[20];
  

  while(1)
  {
    if (sht31_readTempHum(&Temperature,&Humidity)) 
    {


      ESP_LOGI("SHT30", "Temperature=%.1f, Humidity=%.1f", Temperature, Humidity);
      font_transparent = 0; //有背景
      TFT_setFont(USER_FONT, "/spiffs/fonts/Grotesk24x48.fon");
      sprintf(Temperature_c,"%-4.1f",Temperature);//左对齐，4长度，不够补空格
      sprintf(Humidity_c,"%-4.1f",Humidity);
      _fg = TFT_YELLOW;
      TFT_print(Temperature_c, 10, 80);
      _fg = TFT_BLUE;
      TFT_print(Humidity_c, 140, 80);
    }

    if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
    {
      printf("CCS811 TVOC=%4d(ppb), eCO2=%4d(ppm)\n", tvoc, eco2);
      if((tvoc<=999)&&(eco2<=8192))
      {
        sprintf(tvoc_c,"%-3d",tvoc);
        sprintf(eco2_c,"%-4d",eco2);
        _fg = TFT_YELLOW;
        TFT_print(tvoc_c, 10, 240);
        _fg = TFT_BLUE;
        TFT_print(eco2_c, 130, 240);
      }

      
    }

    vTaskDelay(1000 / portTICK_RATE_MS);
  }

}
