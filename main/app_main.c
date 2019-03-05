#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "Smartconfig.h"
#include "Nvs.h"
#include "Mqtt.h"
#include "Json_parse.h"

#include "esp_system.h"
#include "esp_heap_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"

#include "tft_ctl.h"
#include "Uart0.h"
#include "E2prom.h"
#include "RtcUsr.h"
#include "Key.h"
#include "Beep.h"
#include "sht31.h"
#include "ccs811.h"
#include "PM25.h"
#include "formaldehyde.h"
#include "https.h"

static const char *file_photo[3] = {"/spiffs/images/sun1.jpg", 
                                    "/spiffs/images/sun2.jpg", 
                                    "/spiffs/images/sun3.jpg"};




void timer_periodic_cb(void *arg); 
esp_timer_handle_t timer_periodic_handle = 0; //定时器句柄


esp_timer_create_args_t timer_periodic_arg = {
    .callback =
        &timer_periodic_cb, 
    .arg = NULL,            
    .name = "PeriodicTimer" 
};

void timer_periodic_cb(void *arg) //1ms中断一次
{
  static int64_t timer_count = 0;
  timer_count++;
  if (timer_count >= 3000) //1s
  {
    timer_count = 0;
    printf("Free memory: %d bytes\n", esp_get_free_heap_size());

  }
}

void read_flash_usr(void)
{
  esp_err_t err;
  // Open
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) 
  {
      printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } 
  else 
  {
      printf("Done\n");

      // Read
      printf("Reading restart counter from NVS ... ");
      int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS
      err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
      switch (err) 
      {
          case ESP_OK:
            printf("Done\n");
            printf("Restart counter = %d\n", restart_counter);
            break;
          case ESP_ERR_NVS_NOT_FOUND://烧写程序后第一次开机,则清空eeprom，重新烧写序列号
            printf("The first time start after flash!\n");
            char zero_data[256];
            bzero(zero_data,sizeof(zero_data));
            E2prom_Write(0x00, (uint8_t *)zero_data, sizeof(zero_data)); 
            break;
          default :
            printf("Error (%s) reading!\n", esp_err_to_name(err));
      }

      // Write
      printf("Updating restart counter in NVS ... ");
      restart_counter++;
      err = nvs_set_i32(my_handle, "restart_counter", restart_counter);
      printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Commit written value.
      // After setting any values, nvs_commit() must be called to ensure changes are written
      // to flash storage. Implementations may write to storage at other times,
      // but this is not guaranteed.
      printf("Committing updates in NVS ... ");
      err = nvs_commit(my_handle);
      printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Close
      nvs_close(my_handle);
  }
}

/*
co2     400-8192
tvoc    0-1187(999) 
pm2.5   0-1000
甲醛     0-5000
*/

void Tft_Print_main(void)
{
    font_transparent = 0; //有背景
    TFT_setFont(DEJAVU24_FONT, NULL);
    sprintf(Temperature_c,"%02d",(int)Temperature);//左对齐，2长度
    sprintf(Humidity_c,"%02d",(int)Humidity);
    _fg = TFT_WHITE;
    TFT_print(Temperature_c, 25, 3);
    TFT_print(Humidity_c, 97, 3);
    
    if(tvoc<=200)
    {
      _fg = TFT_GREEN;
      tvoc_flag=1;
      tft_print_fields(4,tvoc);
    }
    else if((tvoc>200)&&(tvoc<=400))
    {
      _fg = TFT_YELLOW;
      tvoc_flag=2;
      tft_print_fields(4,tvoc);
    }
    else if((tvoc>400)&&(tvoc<=999))
    {
      _fg = TFT_RED;
      tvoc_flag=3;
      tft_print_fields(4,tvoc);
    }

    if(eco2<=1000)
    {
      _fg = TFT_GREEN;
      eco2_flag=1;
      tft_print_fields(3,eco2);
    }
    else if((eco2>1000)&&(eco2<=2000))
    {
      _fg = TFT_YELLOW;
      eco2_flag=2;
      tft_print_fields(3,eco2);
    }
    else if((eco2>2000)&&(eco2<=8192))
    {
      _fg = TFT_RED;
      eco2_flag=3;
      tft_print_fields(3,eco2);
    }
    

    if(PM2_5<=150)
    {
      _fg = TFT_GREEN;
      PM2_5_flag=1;
      tft_print_fields(2,PM2_5);
    }
    else if((PM2_5>150)&&(PM2_5<=300))
    {
      _fg = TFT_YELLOW;
      PM2_5_flag=2;
      tft_print_fields(2,PM2_5);
    }
    else if((PM2_5>300)&&(PM2_5<=999))
    {
      _fg = TFT_RED;
      PM2_5_flag=3;
      tft_print_fields(2,PM2_5);
    }
    else if((PM2_5>999))
    {
      _fg = TFT_RED;
      PM2_5_flag=3;
      tft_print_fields(2,999);
    }

    
    if(formaldehyde_ug<=80)
    {
      _fg = TFT_GREEN;
      formaldehyde_flag=1;
      tft_print_fields(1,formaldehyde_ug);
    }
    else if((formaldehyde_ug>80)&&(formaldehyde_ug<=100))
    {
      _fg = TFT_YELLOW;
      formaldehyde_flag=2;
      tft_print_fields(1,formaldehyde_ug);
    }
    else if((formaldehyde_ug>100)&&(formaldehyde_ug<=5000))
    {
      _fg = TFT_RED;
      formaldehyde_flag=3;
      tft_print_fields(1,formaldehyde_ug);
    }

    if((tvoc_flag==1)&&(eco2_flag==1)&&(PM2_5_flag==1)&&(formaldehyde_flag==1))
    {
      TFT_jpg_image(80, 293, 0, SPIFFS_BASE_PATH"/images/cp1.jpg", NULL, 0);
    }
    else if((tvoc_flag==3)||(eco2_flag==3)||(PM2_5_flag==3)||(formaldehyde_flag==3))
    {
      TFT_jpg_image(80, 293, 0, SPIFFS_BASE_PATH"/images/cp3.jpg", NULL, 0);
    }
    else if((tvoc_flag==0)&&(eco2_flag==0)&&(PM2_5_flag==0)&&(formaldehyde_flag==0))
    {
      TFT_jpg_image(80, 293, 0, SPIFFS_BASE_PATH"/images/cp1.jpg", NULL, 0);
    }    
    else
    {
      TFT_jpg_image(80, 293, 0, SPIFFS_BASE_PATH"/images/cp2.jpg", NULL, 0);
    }

  
    
    if(TouchStatus==TOUCHSTATUS_TOUCH)
    {
      static bool a=0;
      if(a==0)
      {
        TFT_jpg_image(180, 0, 0, SPIFFS_BASE_PATH"/images/wifi-touch1.jpg", NULL, 0);  
      }
      else if(a==1)
      {
        TFT_jpg_image(180, 0, 0, SPIFFS_BASE_PATH"/images/wifi-touch2.jpg", NULL, 0);  
      }
      a=!a;
    }
    else
    {
      if(WifiStatus==WIFISTATUS_CONNET)
      {
        TFT_jpg_image(180, 0, 0, SPIFFS_BASE_PATH"/images/wifi-connect.jpg", NULL, 0);
      }
      else if(WifiStatus==WIFISTATUS_DISCONNET)
      {
        TFT_jpg_image(180, 0, 0, SPIFFS_BASE_PATH"/images/wifi-error.jpg", NULL, 0);
      }        
    }

    int year,month,day,hour,min,sec;
    char time_dis[20];
    Rtc_Read(&year,&month,&day,&hour,&min,&sec);     
    printf("%d-%d-%d %d:%d:%d\n",year, month, day, hour, min, sec);
    if(year>2000)
    {
      font_transparent = 0; //有背景
      _fg = TFT_WHITE;
      TFT_setFont(DEJAVU18_FONT, NULL);
      sprintf(time_dis,"%02d-%02d %02d:%02d", month, day, hour, min);
      TFT_print(time_dis, 120, 300);
    }


}

void Main_Task(void* arg)
{
  bool main_board=1;
  while(1)
  {
    if(photo==0)
    {
      if(main_board==0)
      {
        tft_main_board();
        main_board=1;
      }
      Tft_Print_main();  
      vTaskDelay(500 / portTICK_RATE_MS);
    }
    else if(photo==1)
    {
      //TFT_jpg_image(CENTER, CENTER, 0, SPIFFS_BASE_PATH"/images/sun1.jpg", NULL, 0);
      uint8_t rand=(uint8_t)rand_interval(0,2);
      TFT_jpg_image(CENTER, CENTER, 0, file_photo[rand], NULL, 0);
      main_board=0;
      while(photo)
      {
        vTaskDelay(500 / portTICK_RATE_MS);
      }
    }
  }
}

/*void Time_Task(void* arg)
{
  while(1)
  {
    if(photo==0)
    {
      int year,month,day,hour,min,sec;
      char time_dis[20];
      Rtc_Read(&year,&month,&day,&hour,&min,&sec);
      printf("%d-%d-%d %d:%d:%d\n",year, month, day, hour, min, sec);
      
      font_transparent = 0; //有背景
      _fg = TFT_WHITE;
      TFT_setFont(UBUNTU16_FONT, NULL);
      sprintf(time_dis,"%d-%d-%d %d:%d:%d",year, month, day, hour, min, sec);
      TFT_print(time_dis, 100, 300);
      
    }
    vTaskDelay(1000 / portTICK_RATE_MS);  
  }  
}*/


void app_main(void)
{

  ESP_ERROR_CHECK( nvs_flash_init() );

  i2c_init();
  Uart0_Init();
  read_flash_usr();//读取开机次数
  key_Init();
  
  ccs811_init();
  tft_init();
  PM25_Init();
  formaldehyde_Init();

    /*step1 判断是否有ProductKey/DeviceName/DeviceSecret****/
  E2prom_Read(PRODUCTKEY_ADDR,(uint8_t *)ProductKey,PRODUCTKEY_LEN);
  printf("ProductKey=%s\n", ProductKey);

  E2prom_Read(DEVICENAME_ADDR,(uint8_t *)DeviceName,DEVICENAME_LEN);
  printf("DeviceName=%s\n", DeviceName);

  E2prom_Read(DEVICESECRET_ADDR,(uint8_t *)DeviceSecret,DEVICESECRET_LEN);
  printf("DeviceSecret=%s\n", DeviceSecret); 


  if((strlen(DeviceName)==0)||(strlen(DeviceName)==0)||(strlen(DeviceSecret)==0)) //未获取到ProductKey/DeviceName/DeviceSecret，未烧写序列号
  {
    printf("no ProductKey/DeviceName/DeviceSecret!\n");
    while(1)
    {
      //故障灯
      //Led_Status=LED_STA_NOSER;
      vTaskDelay(500 / portTICK_RATE_MS);
    }
  }

  xTaskCreate(&Main_Task, "Main_Task", 8192, NULL, 10, NULL);
  

  /*******************************timer 1s init**********************************************/
  esp_err_t err = esp_timer_create(&timer_periodic_arg, &timer_periodic_handle);
  err = esp_timer_start_periodic(timer_periodic_handle, 1000); //创建定时器，单位us，定时1ms
  if (err != ESP_OK)
  {
    printf("timer periodic create err code:%d\n", err);
  }

  /*step3 创建WIFI连接****/
  initialise_wifi();
  //阻塞等待wifi连接
  xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT , false, true, portMAX_DELAY); 
  initialise_mqtt();
  sntp_usr_init();
  weather_init();
  //xTaskCreate(&Time_Task, "Time_Task", 8192, NULL, 10, NULL);
  

}
