#include "comms_espnow.hpp"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <cstring>
#include <stdio.h>
#include <atomic>

extern QueueHandle_t commsQueue;

EventGroupHandle_t final_msg_event = NULL;

constexpr int FINAL_MESSAGE_RECEIVED = (1 << 0);
constexpr int SHUTDOWN_READY = FINAL_MESSAGE_RECEIVED; // add other conditions with bitwise OR if needed later

EventBits_t shutdown_bits;

namespace Comms
{
    // A0:F2:62:F2:2B:70 -- SuperMini ESP32-S3
    constexpr static uint8_t RECEIVER_ADDRESS[] = {0xA0, 0xF2, 0x62, 0xF2, 0x2B, 0x70};
    std::atomic<bool> finalMessageSent{false};

    void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
    {
        if (memcmp(tx_info->des_addr, RECEIVER_ADDRESS, 6) == 0)
        {
            if (finalMessageSent.load(std::memory_order_acquire) && status == ESP_NOW_SEND_SUCCESS)
            {
                finalMessageSent.store(false, std::memory_order_release);
                if (final_msg_event != NULL)
                {
                    xEventGroupSetBits(final_msg_event, FINAL_MESSAGE_RECEIVED);
                }
            }
        }
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

        ESP_ERROR_CHECK(esp_now_init());

        final_msg_event = xEventGroupCreate();
        if (final_msg_event == NULL)
        {
            ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
        }
        esp_now_register_send_cb(OnDataSent);

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
            if (xQueueReceive(commsQueue, &outgoingMessage, portMAX_DELAY) == pdTRUE)
            {
                esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&outgoingMessage, sizeof(DataMessage));
            }
        }
    }

    void sleep()
    {
        printf("Comms::sleep: preparing final shutdown message\n");
        DataMessage final_message;
        
        final_message.hand_id = 1;
        
        final_message.mouseX = 0;
        final_message.mouseY = 0;
        final_message.scrollTicks = 0;

        final_message.leftClick = false;
        final_message.rightClick = false;
        final_message.middleClick = false;
        final_message.mouseFwd = false;
        final_message.mouseBack = false;

        memset(&final_message.keysPressed, '\0', sizeof(final_message.keysPressed));

        finalMessageSent.store(true, std::memory_order_release);

        esp_err_t err = esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&final_message, sizeof(DataMessage));
        printf("Comms::sleep: esp_now_send returned %d\n", err);

        if (err == ESP_OK && final_msg_event != NULL)
        {
            printf("Comms::sleep: waiting for final message send confirmation\n");
            shutdown_bits = xEventGroupWaitBits(
                final_msg_event,
                SHUTDOWN_READY,
                pdTRUE,
                pdTRUE,
                pdMS_TO_TICKS(200)
            );

            if ((shutdown_bits & FINAL_MESSAGE_RECEIVED) != 0)
            {
                printf("Comms::sleep: final message send confirmed\n");
            }
            else
            {
                printf("Comms::sleep: timed out waiting for final message send confirmation\n");
            }
        }
        else
        {
            printf("Comms::sleep: skipped wait because send failed or event unavailable\n");
            shutdown_bits = 0;
        }

        finalMessageSent.store(false, std::memory_order_release);
        printf("Comms::sleep: stopping Wi-Fi\n");
        esp_wifi_stop();
    }
}