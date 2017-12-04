/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "bt.h"
//#include "bt_app_core.h"
//#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;


//static const char *TAG = "TEST";

#define AP_PRINT_MASK   (0x1F)
static uint32_t ap_print_mask = AP_PRINT_MASK;

#define test_sprintf(s,...)           sprintf((char*)(s), ##__VA_ARGS__)

#define SCAN_DONE_FORMAT_PACKET(flag,buf,format,par)    \
    do {                                                \
        if (mask & 0x01) {  \
            if(flag == false){    \
                test_sprintf((buf), format,par);    \
                flag = true;    \
            } else {  \
                test_sprintf((buf), ","format,par);    \
            }\
        }                   \
        mask = mask >>1;    \
    } while(0)

static void start_tcp_client(void)
{
    struct sockaddr_in addr;
    int actual_send = 0;
    int want_send = 0;
    uint8_t *buffer;
    int socket;
    int opt;

    ESP_LOGI("TEST", "---1 %s L%d\n", __func__,__LINE__);
    socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket < 0) {
        ESP_LOGI("TEST", "---2 %s L%d\n", __func__,__LINE__);
        return;
    }

    ESP_LOGI("TEST", "---3 %s L%d\n", __func__,__LINE__);
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI("TEST", "---7 %s L%d\n", __func__,__LINE__);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8266);
    addr.sin_addr.s_addr = htonl(0xC0A80401);
    ESP_LOGI("TEST", "---8 %s L%d\n", __func__,__LINE__);
    if (connect(socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGI("TEST", "---9 %s L%d\n", __func__,__LINE__);
        return ;
    }
    ESP_LOGI("TEST", "---10 %s L%d\n", __func__,__LINE__);
    buffer = (uint8_t *)malloc(1460);
    memset(buffer,0x0,1460);
    while (1)
    {
        actual_send = send(socket, buffer, 1460, 0);
        printf("\nactual_send = %d\n",actual_send);
        vTaskDelay(1000/(1000 /xPortGetTickRateHz()));
    }

    close(socket);
    return ;
}

static void wifi_connection(void)
{
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "FOR_TEST",
            .password = "123456",
        },
    };
    ESP_LOGI("TEST", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    esp_wifi_connect();
}

static void wifi_scan(void)
{
    uint16_t ap_num = 0;
    wifi_scan_config_t config;
    wifi_ap_record_t *ap_list = NULL;
    uint8_t ssid[33];
    bool comma_flag = false;
    uint32_t mask = ap_print_mask;
    uint8_t *temp = NULL;

    memset(&config,0x0,sizeof(config));
    config.show_hidden = true;
    if (esp_wifi_scan_start(&config,true) != ESP_OK) {
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
        return ;
    }
    if (esp_wifi_scan_get_ap_num(&ap_num) != ESP_OK) {
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
        return ;
    }
    ap_list = (wifi_ap_record_t *)malloc(ap_num * sizeof(wifi_ap_record_t));

    if (ap_list == NULL) {
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
        return ;
    }
    if (esp_wifi_scan_get_ap_records(&ap_num, ap_list) == ESP_OK) {
        for (int loop = 0; loop < ap_num; loop++) {
            temp = (uint8_t *)malloc(512);
            memset(ssid, 0, 33);
            memset(temp, 0, 512);
            memcpy(ssid, ap_list[loop].ssid, sizeof(ap_list[loop].ssid));


            comma_flag = false;
            mask = ap_print_mask;

            SCAN_DONE_FORMAT_PACKET(comma_flag, temp + strlen((char *)temp), "%d", ap_list[loop].authmode);
            SCAN_DONE_FORMAT_PACKET(comma_flag, temp + strlen((char *)temp), "\"%s\"", ssid);
            SCAN_DONE_FORMAT_PACKET(comma_flag, temp + strlen((char *)temp), "%d", ap_list[loop].rssi);
            SCAN_DONE_FORMAT_PACKET(comma_flag, temp + strlen((char *)temp), "\""MACSTR"\"",MAC2STR(ap_list[loop].bssid));
            SCAN_DONE_FORMAT_PACKET(comma_flag, temp + strlen((char *)temp), "%d",ap_list[loop].primary);

            ESP_LOGI("TEST","%s",temp);
        }

        free(ap_list);
        free(temp);
    }else{
        free(ap_list);
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    ESP_LOGI("TEST","event = %d",event->event_id);

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI("TEST","SYSTEM_EVENT_STA_START");
        wifi_scan();
        break;
    case SYSTEM_EVENT_SCAN_DONE:
        ESP_LOGI("TEST","SYSTEM_EVENT_SCAN_DONE");
        wifi_connection();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI("TEST","Got IPv4[%s]",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        start_tcp_client();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently auto-reassociate. */
        ESP_LOGI("TEST","SYSTEM_EVENT_STA_DISCONNECTED");
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());
}


static void init_bluetooth(void)
{
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE("TEST", "%s initialize controller failed\n", __func__);
        return;
    }
    //ESP_BT_MODE_BTDM
    //ESP_BT_MODE_CLASSIC_BT
    if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) {
        ESP_LOGE("TEST", "%s enable controller failed\n", __func__);
        return;
    }

    if (esp_bluedroid_init() != ESP_OK) {
        ESP_LOGE("TEST", "%s initialize bluedroid failed\n", __func__);
        return;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE("TEST", "%s enable bluedroid failed\n", __func__);
        return;
    }
}

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    init_bluetooth();
    initialise_wifi();
}

