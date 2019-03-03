#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "formaldehyde.h"


#define UART2_TXD  (GPIO_NUM_16)
#define UART2_RXD  (GPIO_NUM_4)
#define UART2_RTS  (UART_PIN_NO_CHANGE)
#define UART2_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE    100

char wzs_init_cmd[10]={0xFF,0x01,0x78,0x41,0x00,0x00,0x00,0x00,0x46};
char wzs_query_cmd[10]={0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};

static const char *TAG = "formaldehyde";

void formaldehyde_Read_Task(void* arg);


void formaldehyde_Init(void)
{
     //配置GPIO
    /*gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask =  1 << UART2_RXD;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask =  1 << UART2_TXD;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);*/
    
    
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, UART2_TXD, UART2_RXD, UART2_RTS, UART2_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);

    uart_write_bytes(UART_NUM_2, wzs_init_cmd, 9);
    formaldehyde_ug=0;

    xTaskCreate(&formaldehyde_Read_Task, "formaldehyde_Read_Task", 2046, NULL, 10, NULL);
}


static uint8_t	Check_Sensor_DataValid(uint8_t *i,uint8_t ln)
{
    uint8_t j,tempq=0;
    i+=1;
    for(j=0;j<(ln-2);j++)
    {
        tempq+=*i;
        i++;
    }
    tempq=(~tempq)+1;

    return(tempq);

}





void formaldehyde_Read_Task(void* arg)
{
    uint8_t data_u2[BUF_SIZE];
    vTaskDelay(1000 / portTICK_RATE_MS);
    while(1)
    {
        uart_write_bytes(UART_NUM_2, wzs_query_cmd, 9);
        int len = uart_read_bytes(UART_NUM_2, data_u2, BUF_SIZE, 10 / portTICK_RATE_MS);
        if(len!=0)  //读取到传感器数据
        {
            printf("formaldehyde=");
            for(int i=0;i<len;i++)
            {
                printf("0x%02x ",data_u2[i]);
            }
            printf("\n");
            uint8_t check=Check_Sensor_DataValid(data_u2,len);
            //printf("ckeck=0x%02x\n",check);
            if(Check_Sensor_DataValid(data_u2,len)==data_u2[len-1])
            {
                //printf("check ok\n");
                if(data_u2[1]==0x86)
                {
                    formaldehyde_ug=(uint16_t)((data_u2[2]<<4) | data_u2[3]);
                    ESP_LOGI(TAG, "formaldehyde=%d(ug/m3)", formaldehyde_ug);
                    //formaldehyde_ppb=(uint16_t)((data_u2[6]<<4) | data_u2[7]);
                    //ESP_LOGI(TAG, "formaldehyde=%d(ppb)", formaldehyde_ppb);
                }
            }
            else
            {
               printf("formaldehyde check err\n");
            }
    
            len=0;
            bzero(data_u2,sizeof(data_u2));                 
        }  
        vTaskDelay(5000 / portTICK_RATE_MS);
    }   
}


