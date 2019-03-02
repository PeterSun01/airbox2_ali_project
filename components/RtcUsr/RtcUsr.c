#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#include "RtcUsr.h"
#include "Smartconfig.h"

static const char *TAG = "RtcUsr";


void Rtc_Set(int year,int mon,int day,int hour,int min,int sec)
{
    struct tm tm;
    tm.tm_year = year - 1900;
    tm.tm_mon = mon-1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    time_t t = mktime(&tm);
    ESP_LOGI(TAG, "Setting time: %s", asctime(&tm));
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
}

void Rtc_Read(int* year,int* month,int* day,int* hour,int* min,int* sec)
{    
    time_t timep;
    struct tm *p;
    time (&timep);
    timep=timep+28800;
    p=gmtime(&timep);
    *year=(1900+p->tm_year);
    *month=(1+p->tm_mon);
    *day=p->tm_mday;
    *hour=p->tm_hour;
    *min=p->tm_min;
    *sec=p->tm_sec;
    //ESP_LOGI(TAG, "Read:%d-%d-%d %d:%d:%d No.%d",(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour,p->tm_min,p->tm_sec,(1+p->tm_yday));
    
    
    /*printf("%d\n",p->tm_sec); //获取当前秒
    printf("%d\n",p->tm_min); //获取当前分
    printf("%d\n",p->tm_hour);//获取当前时,这里获取西方的时间,刚好相差八个小时
    printf("%d\n",p->tm_mday);//获取当前月份日数,范围是1-31
    printf("%d\n",1+p->tm_mon);//获取当前月份,范围是0-11,所以要加1
    printf("%d\n",1900+p->tm_year);//获取当前年份,从1900开始，所以要加1900
    printf("%d\n",p->tm_yday); //从今年1月1日算起至今的天数，范围为0-365
    */

}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT , false, true, portMAX_DELAY); 
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

void sntp_usr_init(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) 
    {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];


    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
}