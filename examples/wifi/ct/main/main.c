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
#include <sys/socket.h>

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


typedef struct {
//    iperf_cfg_t cfg;
    bool     finish;
    uint32_t total_len;
    uint32_t buffer_len;
    uint8_t  *buffer;
    uint32_t socket;
} iperf_ctrl_t;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

//static iperf_ctrl_t s_iperf_ctrl;

static void start_tcp_server(void)
{
    socklen_t addr_len = sizeof(struct sockaddr);
    struct sockaddr_in remote_addr;
    struct sockaddr_in addr;
    int actual_recv = 0;
    int want_recv = 0;
    uint8_t *buffer;
    int listen_socket;
    struct timeval t;
    int socket;
    int opt;


    ESP_LOGI("TEST", "---1 %s L%d\n", __func__,__LINE__);
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ESP_LOGI("TEST", "---2 %s L%d\n", __func__,__LINE__);
    if (listen_socket < 0) {
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
        return ;
    }

    ESP_LOGI("TEST", "---3 %s L%d\n", __func__,__LINE__);
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ESP_LOGI("TEST", "---4 %s L%d\n", __func__,__LINE__);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8266);
    addr.sin_addr.s_addr = 0;
    ESP_LOGI("TEST", "---5 %s L%d\n", __func__,__LINE__);
    if (bind(listen_socket, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(listen_socket);
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
        return ;
    }

    ESP_LOGI("TEST", "---6 %s L%d\n", __func__,__LINE__);
    if (listen(listen_socket, 5) < 0) {
        close(listen_socket);
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
        return ;
    }

    buffer = (uint8_t *)malloc(2000);

    ESP_LOGI("TEST", "---7 %s L%d\n", __func__,__LINE__);
    socket = accept(listen_socket, (struct sockaddr*)&remote_addr, &addr_len);
    ESP_LOGI("TEST", "---8 %s L%d\n", __func__,__LINE__);
    if (socket < 0) {
        close(listen_socket);
        ESP_LOGE("TEST", "error %s L%d\n", __func__,__LINE__);
        return ;
    }

    ESP_LOGI("TEST", "wait receive data %s L%d\n", __func__,__LINE__);
    while(1){
        memset(buffer,0x0,2000);
        actual_recv = recv(socket, buffer, 2000, 0);
        printf("actual_recv = %d\r\n",actual_recv);
    }
    free(buffer);
    close(listen_socket);
    return ;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    ESP_LOGI("TEST","event = %d",event->event_id);

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI("TEST","SYSTEM_EVENT_STA_START");
        break;
    case SYSTEM_EVENT_SCAN_DONE:
        ESP_LOGI("TEST","SYSTEM_EVENT_SCAN_DONE");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI("TEST","Got IPv4[%s]",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI("TEST","SYSTEM_EVENT_STA_DISCONNECTED");
        break;
    case SYSTEM_EVENT_AP_START:
        ESP_LOGI("TEST","SYSTEM_EVENT_AP_START");
        start_tcp_server();
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

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "FOR_TEST",
            .password = "123456",
            .ssid_len = strlen("FOR_TEST"),
            .max_connection = 1,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
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

