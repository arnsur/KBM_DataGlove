#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include <cstring>
#include <stdio.h>
#include <atomic>

#include "comms_espnow.hpp"

extern QueueHandle_t commsQueue;

TimerHandle_t xTimeoutTimerHandle = NULL;
TimerHandle_t xLeftWatchdogTimerHandle = NULL;

EventGroupHandle_t final_msg_event = NULL;

constexpr int FINAL_MESSAGE_RECEIVED = (1 << 0);
constexpr int SHUTDOWN_READY = FINAL_MESSAGE_RECEIVED; // add other conditions with bitwise OR if needed later

EventBits_t shutdown_bits;

namespace Comms
{

    CommsStatus comms_status = {
        .r_to_recv_conn_status = SEARCHING,
        .r_to_l_conn_status = SEARCHING,
        .l_to_recv_conn_status = SEARCHING
    };

    PeerConnection recv_peer = {
        .search_timed_out = false,
    };
    PeerConnection left_peer = {
        .search_timed_out = false,
    };

    std::array<int, 12> left_mux_values;
    bool left_telemetry_available = false;
    portMUX_TYPE left_telemetry_mux = portMUX_INITIALIZER_UNLOCKED;

    void check_and_stop_timer()
    {
        bool r_recv_resolved = (comms_status.r_to_recv_conn_status != SEARCHING);
        bool r_l_resolved = (comms_status.r_to_l_conn_status != SEARCHING);
        
        bool l_recv_resolved = (comms_status.l_to_recv_conn_status == CONNECTED || 
                                comms_status.l_to_recv_conn_status == DISCONNECTED);

        if (r_recv_resolved && r_l_resolved && l_recv_resolved)
        {
            if (xTimerIsTimerActive(xTimeoutTimerHandle) != pdFALSE)
            {
                xTimerStop(xTimeoutTimerHandle, 0);
            }
        }
    }

    void vTimeoutCallback(TimerHandle_t xTimer)
    {
        if (comms_status.r_to_recv_conn_status == SEARCHING)
        {
            recv_peer.search_timed_out.store(true, std::memory_order_release);
            comms_status.r_to_recv_conn_status = DISCONNECTED;
        }

        if (comms_status.r_to_l_conn_status == SEARCHING)
        {
            left_peer.search_timed_out.store(true, std::memory_order_release);
            comms_status.r_to_l_conn_status = DISCONNECTED;
        }

        if (comms_status.l_to_recv_conn_status == SEARCHING)
        {
            comms_status.l_to_recv_conn_status = UNKNOWN;
        }
    }

    void vLeftWatchdogCallback(TimerHandle_t xTimer)
    {
        if (comms_status.l_to_recv_conn_status == CONNECTED || comms_status.l_to_recv_conn_status == DISCONNECTED)
        {
            comms_status.l_to_recv_conn_status = UNKNOWN;
            printf("Left glove telemetry lost.\n");
        }
    }

    void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len)
    {
        if (memcmp(esp_now_info->src_addr, LEFT_GLOVE_ADDRESS, 6) == 0)
        {
            if (data_len == sizeof(LeftTelemetryMessage))
            {
                LeftTelemetryMessage* msg = (LeftTelemetryMessage*)data;
                comms_status.l_to_recv_conn_status = msg->l_to_recv_conn_status;

                taskENTER_CRITICAL(&left_telemetry_mux);
                left_mux_values = msg->muxValues;
                left_telemetry_available = true;
                taskEXIT_CRITICAL(&left_telemetry_mux);

                if (xLeftWatchdogTimerHandle != NULL)
                {
                    xTimerReset(xLeftWatchdogTimerHandle, 0);
                }

                check_and_stop_timer();
            }
        }
    }

    std::atomic<bool> finalMessageSent{false};

    void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
    {
        bool is_receiver = memcmp(tx_info->des_addr, RECEIVER_ADDRESS, 6) == 0;
        bool is_left = memcmp(tx_info->des_addr, LEFT_GLOVE_ADDRESS, 6) == 0;

        if (is_receiver)
        {
            if (status == ESP_NOW_SEND_SUCCESS)
            {
                recv_peer.search_timed_out.store(false, std::memory_order_release);
                comms_status.r_to_recv_conn_status = CONNECTED;

                if (finalMessageSent.load(std::memory_order_acquire))
                {
                    finalMessageSent.store(false, std::memory_order_release);
                    if (final_msg_event != NULL)
                    {
                        xEventGroupSetBits(final_msg_event, FINAL_MESSAGE_RECEIVED);
                    }
                }
            } 
            else 
            {
                if (!recv_peer.search_timed_out.load(std::memory_order_acquire))
                {
                    comms_status.r_to_recv_conn_status = SEARCHING;
                    if (xTimerIsTimerActive(xTimeoutTimerHandle) == pdFALSE)
                    {
                        xTimerStart(xTimeoutTimerHandle, 0);
                    }
                }
            }
        }
        else if (is_left)
        {
            if (status == ESP_NOW_SEND_SUCCESS)
            {
                left_peer.search_timed_out.store(false, std::memory_order_release);
                comms_status.r_to_l_conn_status = CONNECTED;
            }
            else
            {
                if (!left_peer.search_timed_out.load(std::memory_order_acquire))
                {
                    comms_status.r_to_l_conn_status = SEARCHING;
                    if (xTimerIsTimerActive(xTimeoutTimerHandle) == pdFALSE)
                    {
                        xTimerStart(xTimeoutTimerHandle, 0);
                    }
                }
            }
        }

        check_and_stop_timer();
    }

    void init()
    {
        xTimeoutTimerHandle = xTimerCreate("TimeoutTimer", pdMS_TO_TICKS(MAX_SEARCH_TIME_S * 1000), pdFALSE, (void*) 0, vTimeoutCallback);
        xLeftWatchdogTimerHandle = xTimerCreate("LeftWatchdog", pdMS_TO_TICKS(2000), pdFALSE, (void*) 0, vLeftWatchdogCallback);

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
        esp_now_register_recv_cb(OnDataRecv);

        esp_now_peer_info_t recvPeerInfo = {};
        memcpy(recvPeerInfo.peer_addr, RECEIVER_ADDRESS, 6);
        recvPeerInfo.channel = 0;
        recvPeerInfo.encrypt = false;
        ESP_ERROR_CHECK(esp_now_add_peer(&recvPeerInfo));

        esp_now_peer_info_t leftPeerInfo = {};
        memcpy(leftPeerInfo.peer_addr, LEFT_GLOVE_ADDRESS, 6);
        leftPeerInfo.channel = 0;
        leftPeerInfo.encrypt = false;
        ESP_ERROR_CHECK(esp_now_add_peer(&leftPeerInfo));

        printf("ESP-NOW Initialized cleanly.\n");

        xTimerStart(xTimeoutTimerHandle, 0);
    }

    void vCommsTask(void *pvParameters)
    {
        CommsMessage outgoingMessage;

        while (1)
        {
            if (xQueueReceive(commsQueue, &outgoingMessage, portMAX_DELAY) == pdTRUE)
            {
                esp_now_send(outgoingMessage.address, (uint8_t *)&outgoingMessage.payload, outgoingMessage.payload_length);
            }
        }
    }

    void sleep()
    {
        printf("Comms::sleep: preparing final shutdown message\n");
        ReceiverMessage final_message;

        final_message.hand_id = 1;

        final_message.deltaMouseX = 0;
        final_message.deltaMouseY = 0;
        final_message.scrollTicks = 0;

        final_message.leftClick = false;
        final_message.rightClick = false;
        final_message.middleClick = false;
        final_message.mouseFwd = false;
        final_message.mouseBack = false;

        memset(&final_message.keysPressed, '\0', sizeof(final_message.keysPressed));

        finalMessageSent.store(true, std::memory_order_release);

        esp_err_t err = esp_now_send(RECEIVER_ADDRESS, (uint8_t *)&final_message, sizeof(ReceiverMessage));
        printf("Comms::sleep: esp_now_send returned %d\n", err);

        if (err == ESP_OK && final_msg_event != NULL)
        {
            printf("Comms::sleep: waiting for final message send confirmation\n");
            shutdown_bits = xEventGroupWaitBits(
                final_msg_event,
                SHUTDOWN_READY,
                pdTRUE,
                pdTRUE,
                pdMS_TO_TICKS(200));

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

    void wake_up_comms()
    {
        xTimerReset(xTimeoutTimerHandle, 0);

        recv_peer.search_timed_out.store(false, std::memory_order_release);
        comms_status.r_to_recv_conn_status = SEARCHING;

        left_peer.search_timed_out.store(false, std::memory_order_release);
        comms_status.r_to_l_conn_status = SEARCHING;

        comms_status.l_to_recv_conn_status = SEARCHING;
    }
}