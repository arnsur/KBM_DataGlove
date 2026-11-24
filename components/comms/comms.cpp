#include "esp_now.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "comms.hpp"
#include "board_config.hpp"

static QueueHandle_t comms_queue = nullptr;

using namespace CommsProtocol;

namespace Comms
{
    // A0:F2:62:F2:2B:70 -- SuperMini ESP32-S3
    constexpr static uint8_t RECV_MAC[6] = {0xA0, 0xF2, 0x62, 0xF2, 0x2B, 0x70};

    // -- ESP23-S3-MINI-1
    // constexpr static uint8_t LEFT_GLOVE_MAC[6] = {1, 2, 3, 4, 5, 6};

    void queue_push_data(ReceiverMessage &msg)
    {
        msg.hand_id = 0;
        xQueueSendToBack(comms_queue, &msg, 0);
    }

    void on_data_sent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
    {
        // Handle connection status
    }

    void init()
    {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        
        comms_queue = xQueueCreate(20, sizeof(ReceiverMessage));

        ESP_ERROR_CHECK(esp_now_init());
        ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

        esp_now_peer_info_t recv_peer_info = {};
        memcpy(&recv_peer_info.peer_addr, RECV_MAC, 6);
        recv_peer_info.channel = 0;
        recv_peer_info.ifidx = WIFI_IF_STA;
        recv_peer_info.encrypt = false;
        ESP_ERROR_CHECK(esp_now_add_peer(&recv_peer_info));

        // esp_now_peer_info_t l_glove_peer_info = {};
        // memcpy(&l_glove_peer_info.peer_addr, LEFT_GLOVE_MAC, 6);
        // l_glove_peer_info.channel = 0;
        // l_glove_peer_info.ifidx = WIFI_IF_STA;
        // l_glove_peer_info.encrypt = false;
        // ESP_ERROR_CHECK(esp_now_add_peer(&l_glove_peer_info));

    }

    void send_data()
    {
        ReceiverMessage outgoing_message;

        if (xQueueReceive(comms_queue, &outgoing_message, portMAX_DELAY) == pdTRUE)
        {
            esp_now_send(RECV_MAC, (uint8_t *)&outgoing_message, sizeof(ReceiverMessage));
        }
    }
}