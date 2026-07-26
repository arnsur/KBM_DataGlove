#include "comms_espnow.hpp"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstring>
#include <stdio.h>

// Import the queue you will create in main.cpp
extern QueueHandle_t commsQueue;

namespace Comms
{
    // A0:F2:62:F2:2B:70 -- SuperMini ESP32-S3
    constexpr static uint8_t RECEIVER_ADDRESS[] = {0xA0, 0xF2, 0x62, 0xF2, 0x2B, 0x70};

    void init()
    {
        // 1. Initialize NVS (Required for Wi-Fi)
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        // 2. Initialize Wi-Fi in Station Mode
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());

        // 3. Initialize ESP-NOW
        ESP_ERROR_CHECK(esp_now_init());

        // 4. Register the Receiver as a Peer
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, RECEIVER_ADDRESS, 6);
        peerInfo.channel = 0; // 0 means use the current Wi-Fi channel
        peerInfo.encrypt = false;

        ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

        printf("ESP-NOW Initialized cleanly.\n");
    }

    void vCommsTask(void *pvParameters)
    {
        DataMessage outgoingMessage;

        while (1)
        {
            // 1. Wait here indefinitely until the Engine Task puts a message in the queue.
            // Using portMAX_DELAY means this task uses 0% CPU while waiting!
            if (xQueueReceive(commsQueue, &outgoingMessage, portMAX_DELAY) == pdTRUE)
            {
                // 2. Blast it over the air!
                esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&outgoingMessage, sizeof(DataMessage));
            }
        }
    }
}