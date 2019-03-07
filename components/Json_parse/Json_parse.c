#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include "esp_system.h"
#include "Json_parse.h"
#include "Nvs.h"
#include "esp_log.h"

#include "esp_wifi.h"
#include "Smartconfig.h"
#include "E2prom.h"
#include "Beep.h"
#include "sht31.h"
#include "PM25.h"
#include "ccs811.h"
#include "formaldehyde.h"


esp_err_t parse_Uart0(char *json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_ProductKey = NULL;
    cJSON *json_data_parse_DeviceName = NULL;
    cJSON *json_data_parse_DeviceSecret = NULL;

    if(json_data[0]!='{')
    {
        printf("uart0 Json Formatting error1\n");
        return 0;
    }

    json_data_parse = cJSON_Parse(json_data);
    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {
        printf("uart0 Json Formatting error\n");
        cJSON_Delete(json_data_parse);

        return 0;
    }
    else
    {
        /*
            {"Command":"SetupProduct","ProductKey":"a148brGI1Gw","DeviceName":"A0001AIR1","DeviceSecret":"deOTkO9yw6PlAceTLzXlTIPJs4uzbSIc"}
        */
        
        
        json_data_parse_ProductKey = cJSON_GetObjectItem(json_data_parse, "ProductKey"); 
        printf("ProductKey= %s\n", json_data_parse_ProductKey->valuestring);

        json_data_parse_DeviceName = cJSON_GetObjectItem(json_data_parse, "DeviceName"); 
        printf("DeviceName= %s\n", json_data_parse_DeviceName->valuestring);

        json_data_parse_DeviceSecret = cJSON_GetObjectItem(json_data_parse, "DeviceSecret"); 
        printf("DeviceSecret= %s\n", json_data_parse_DeviceSecret->valuestring);

        char zero_data[256];
        bzero(zero_data,sizeof(zero_data));
        E2prom_Write(0x00, (uint8_t *)zero_data, sizeof(zero_data)); 
    
        sprintf(ProductKey,"%s%c",json_data_parse_ProductKey->valuestring,'\0');
        E2prom_Write(PRODUCTKEY_ADDR, (uint8_t *)ProductKey, strlen(ProductKey));
        
        sprintf(DeviceName,"%s%c",json_data_parse_DeviceName->valuestring,'\0');
        E2prom_Write(DEVICENAME_ADDR, (uint8_t *)DeviceName, strlen(DeviceName)); 

        sprintf(DeviceSecret,"%s%c",json_data_parse_DeviceSecret->valuestring,'\0');
        E2prom_Write(DEVICESECRET_ADDR, (uint8_t *)DeviceSecret, strlen(DeviceSecret));


        printf("{\"status\":\"success\",\"err_code\": 0}");
        cJSON_Delete(json_data_parse);
        fflush(stdout);//使stdout清空，就会立刻输出所有在缓冲区的内容。
        esp_restart();//芯片复位 函数位于esp_system.h
        return 1;

    }
}



/*
{
	"method": "thing.service.property.set",
	"id": "216373306",
	"params": {
		"photo": 0
	},
	"version": "1.0.0"
}
*/


esp_err_t parse_objects_mqtt(char *json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_method = NULL;
    cJSON *json_data_parse_params = NULL;

    cJSON *iot_params_parse = NULL;
    cJSON *iot_params_parse_Switch_JDQ = NULL;

    json_data_parse = cJSON_Parse(json_data);

    if(json_data[0]!='{')
    {
        printf("mqtt Json Formatting error\n");

        return 0;       
    }

    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {

        printf("Json Formatting error1\n");
        cJSON_Delete(json_data_parse);
        return 0;
    }
    else
    {
        json_data_parse_method = cJSON_GetObjectItem(json_data_parse, "method"); 
        printf("method= %s\n", json_data_parse_method->valuestring);

        json_data_parse_params = cJSON_GetObjectItem(json_data_parse, "params"); 
        char *iot_params;
        iot_params=cJSON_PrintUnformatted(json_data_parse_params);
        printf("cJSON_Print= %s\n",iot_params);

        iot_params_parse = cJSON_Parse(iot_params);
        iot_params_parse_Switch_JDQ = cJSON_GetObjectItem(iot_params_parse, "photo"); 
        printf("photo= %d\n", iot_params_parse_Switch_JDQ->valueint);
        photo=iot_params_parse_Switch_JDQ->valueint;
        
        free(iot_params);
        cJSON_Delete(iot_params_parse);
    }
    
    cJSON_Delete(json_data_parse);
    

    return 1;
}



/*
{
	"method": "thing.event.property.post",
	"params": {
		"CO2": 1,
		"CurrentHumidity": 5,
		"CurrentTemperature": 3,
		"HCHO": 2,
		"PM25": 4,
		"TVOC": 1,
		"RSSI": 0
	}
}
*/

void create_mqtt_json(creat_json *pCreat_json)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *next = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "method", cJSON_CreateString("thing.event.property.post"));
    cJSON_AddItemToObject(root, "params", next);


    cJSON_AddItemToObject(next, "CurrentTemperature", cJSON_CreateNumber(Temperature));
    cJSON_AddItemToObject(next, "CurrentHumidity", cJSON_CreateNumber(Humidity)); 
    cJSON_AddItemToObject(next, "CO2", cJSON_CreateNumber(eco2));
    cJSON_AddItemToObject(next, "HCHO", cJSON_CreateNumber(formaldehyde_ug));
    cJSON_AddItemToObject(next, "PM25", cJSON_CreateNumber(PM2_5));  
    cJSON_AddItemToObject(next, "TVOC", cJSON_CreateNumber(tvoc));
    cJSON_AddItemToObject(next, "photo", cJSON_CreateNumber(photo));
    wifi_ap_record_t wifidata;
    if (esp_wifi_sta_get_ap_info(&wifidata) == 0)
    {
        cJSON_AddItemToObject(next, "RSSI", cJSON_CreateNumber(wifidata.rssi));
    }

    char *cjson_printunformat;
    cjson_printunformat=cJSON_PrintUnformatted(root);
    pCreat_json->creat_json_c=strlen(cjson_printunformat);
    bzero(pCreat_json->creat_json_b,sizeof(pCreat_json->creat_json_b));
    memcpy(pCreat_json->creat_json_b,cjson_printunformat,pCreat_json->creat_json_c);
    printf("len=%d,mqtt_json=%s\n",pCreat_json->creat_json_c,pCreat_json->creat_json_b);
    free(cjson_printunformat);
    cJSON_Delete(root);
    
}

char Weather_today[512]="\0";//今日天气json
char Weather_tomorrow[512]="\0";//明日天气json

esp_err_t parse_https_respond(char *https_json_data)
{
    char * lookup1=NULL;
    char * lookup2=NULL;
    int today_json_len,tomorrow_json_len;

    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_Today_Cond_d = NULL;
    cJSON *json_data_parse_Today_Cond_n = NULL;
    cJSON *json_data_parse_Today_Date = NULL;
    cJSON *json_data_parse_Today_Temp_max = NULL;
    cJSON *json_data_parse_Today_Temp_min = NULL;

    cJSON *json_data_parse_Tomorrow_Cond_d = NULL;
    cJSON *json_data_parse_Tomorrow_Cond_n = NULL;
    cJSON *json_data_parse_Tomorrow_Date = NULL;
    cJSON *json_data_parse_Tomorrow_Temp_max = NULL;
    cJSON *json_data_parse_Tomorrow_Temp_min = NULL;



    if(https_json_data==NULL) //发送的不是json数据包
    {
        printf("null\n");
        return 0;
    }
    else
    {
        lookup1=strstr(https_json_data,"\"daily_forecast\"");//查找第一个date为当前日期
        if(lookup1!=NULL)
        {
            lookup2=strchr(lookup1,'}');
            today_json_len=lookup2-lookup1-17;

            bzero(Weather_today,sizeof(Weather_today));
            strncpy(Weather_today,lookup1+18,today_json_len);
            //Weather_today[today_json_len+1]='\0';
            printf("Weather_today=\n%s\n",Weather_today);


            json_data_parse = cJSON_Parse(Weather_today);
            if (json_data_parse == NULL) //如果数据包不为JSON则退出
            {

                printf("Json Formatting error\n");
                cJSON_Delete(json_data_parse);
                return 0;
            }

            json_data_parse_Today_Cond_d = cJSON_GetObjectItem(json_data_parse, "cond_code_d"); 
            printf("today cond_code_d= %s\n", json_data_parse_Today_Cond_d->valuestring);

            json_data_parse_Today_Cond_n = cJSON_GetObjectItem(json_data_parse, "cond_code_n"); 
            printf("today cond_code_n= %s\n", json_data_parse_Today_Cond_n->valuestring);  

            json_data_parse_Today_Date = cJSON_GetObjectItem(json_data_parse, "date"); 
            printf("today date= %s\n", json_data_parse_Today_Date->valuestring);   

            json_data_parse_Today_Temp_max = cJSON_GetObjectItem(json_data_parse, "tmp_max"); 
            printf("today tmp_max= %s\n", json_data_parse_Today_Temp_max->valuestring);  

            json_data_parse_Today_Temp_min = cJSON_GetObjectItem(json_data_parse, "tmp_min"); 
            printf("today tmp_min= %s\n", json_data_parse_Today_Temp_min->valuestring); 
            
            cJSON_Delete(json_data_parse);      

        }

        lookup1=strchr(lookup2,'{');
        if(lookup1!=NULL)
        {
            lookup2=strchr(lookup1,'}');
            tomorrow_json_len=lookup2-lookup1+1;
            bzero(Weather_tomorrow,sizeof(Weather_tomorrow));
            strncpy(Weather_tomorrow,lookup1,tomorrow_json_len);
            printf("Weather_tomorrow=\n%s\n",Weather_tomorrow);

            json_data_parse = cJSON_Parse(Weather_tomorrow);
            if (json_data_parse == NULL) //如果数据包不为JSON则退出
            {

                printf("Json Formatting error\n");
                cJSON_Delete(json_data_parse);
                return 0;
            }

            json_data_parse_Tomorrow_Cond_d = cJSON_GetObjectItem(json_data_parse, "cond_code_d"); 
            printf("Tomorrow cond_code_d= %s\n", json_data_parse_Tomorrow_Cond_d->valuestring);

            json_data_parse_Tomorrow_Cond_n = cJSON_GetObjectItem(json_data_parse, "cond_code_n"); 
            printf("Tomorrow cond_code_n= %s\n", json_data_parse_Tomorrow_Cond_n->valuestring);  

            json_data_parse_Tomorrow_Date = cJSON_GetObjectItem(json_data_parse, "date"); 
            printf("Tomorrow date= %s\n", json_data_parse_Tomorrow_Date->valuestring);   

            json_data_parse_Tomorrow_Temp_max = cJSON_GetObjectItem(json_data_parse, "tmp_max"); 
            printf("Tomorrow tmp_max= %s\n", json_data_parse_Tomorrow_Temp_max->valuestring);  

            json_data_parse_Tomorrow_Temp_min = cJSON_GetObjectItem(json_data_parse, "tmp_min"); 
            printf("Tomorrow tmp_min= %s\n", json_data_parse_Tomorrow_Temp_min->valuestring); 
            
            cJSON_Delete(json_data_parse);   
            
        }


    }

    return 1;
    
}



