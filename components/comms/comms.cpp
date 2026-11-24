#include "esp_now.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
        ESP_ERROR_CHECK(esp_now_init());

        esp_now_peer_info_t recv_peer_info = {};
        memcpy(&recv_peer_info.peer_addr, RECV_MAC, 6);
        ESP_ERROR_CHECK(esp_now_add_peer(&recv_peer_info));

        // esp_now_peer_info_t l_glove_peer_info = {};
        // memcpy(&l_glove_peer_info.peer_addr, LEFT_GLOVE_MAC, 6);
        // ESP_ERROR_CHECK(esp_now_add_peer(&l_glove_peer_info));

        ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

        comms_queue = xQueueCreate(20, sizeof(ReceiverMessage));
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