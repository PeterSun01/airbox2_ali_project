/* HTTPS GET Example using plain mbedTLS sockets
 *
 * Contacts the howsmyssl.com API via TLS v1.2 and reads a JSON
 * response.
 *
 * Adapted from the ssl_client1 example in mbedtls.
 *
 * Original Copyright (C) 2006-2016, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD, Apache 2.0 License.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_tls.h"

#include "https.h"
#include "Json_parse.h"
#include "Smartconfig.h"



#define WEB_SERVER "free-api.heweather.net"
#define WEB_PORT "443"
#define WEB_URL "https://free-api.heweather.net/s6/weather/forecast?location=CN101070201&key=0db1f83d104e4e24aab397c48bd16223"

static const char *TAG = "https";

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

static char buf[2048];
static int ret, len;

extern const int CONNECTED_BIT;
    


static void https_get_task(void *pvParameters)
{
    while(1) 
    {
        xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT , false, true, portMAX_DELAY);  
        ESP_LOGE(TAG, "Connected to Weather");
        esp_tls_cfg_t cfg = 
        {
            .cacert_pem_buf  = server_root_cert_pem_start,
            .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
        };
        
        struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);
        
        if(tls != NULL) {
            ESP_LOGI(TAG, "Connection established...");
        } else {
            ESP_LOGE(TAG, "Connection failed...");
            goto exit;
        }
        
        size_t written_bytes = 0;
        do {
            ret = esp_tls_conn_write(tls, 
                                     REQUEST + written_bytes, 
                                     strlen(REQUEST) - written_bytes);
            if (ret >= 0) {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            } else if (ret != MBEDTLS_ERR_SSL_WANT_READ  && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
                goto exit;
            }
        } while(written_bytes < strlen(REQUEST));

        ESP_LOGI(TAG, "Reading HTTP response...");

        do
        {
            len = sizeof(buf) - 1;
            bzero(buf, sizeof(buf));
            ret = esp_tls_conn_read(tls, (char *)buf, len);
            
            if(ret == MBEDTLS_ERR_SSL_WANT_WRITE  || ret == MBEDTLS_ERR_SSL_WANT_READ)
            {
                //continue;
                ESP_LOGE(TAG, "esp_tls_conn_read1  returned -0x%x", -ret);
                break;
            }
                
            
            if(ret < 0)
            {
                ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
                break;
            }

            if(ret == 0)
            {
                ESP_LOGI(TAG, "connection closed");
                break;
            }

            len = ret;
            ESP_LOGI(TAG, "%d bytes read", len);
            /* Print response directly to stdout as it is read */

            parse_https_respond(strchr(buf, '{'));

            printf("weather=%s\n",buf);

            /*for(int i = 0; i < len; i++) 
            {
                putchar(buf[i]);
            }
            putchar('\n');*/
        } 
        while(1);

    exit:
        esp_tls_conn_delete(tls);    
        putchar('\n'); // JSON output doesn't have a newline at end

        static int request_count;
        ESP_LOGI(TAG, "Completed %d requests", ++request_count);

        for(int countdown = 10; countdown >= 0; countdown--) 
        {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}

void weather_init(void)
{

    xTaskCreate(&https_get_task, "https_get_task", 8192, NULL, 5, NULL);
}
