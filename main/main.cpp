#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mutex>
#include "esp_sleep.h"
#include <cstring>

extern "C"
{
#include "sh2.h"
#include "sh2_err.h"
#include "sh2_hal.h"
}

#include "glove_types.hpp"
#include "hal_analog.hpp"
#include "hal_imu.hpp"
#include "gesture_engine.hpp"
#include "comms_espnow.hpp"
#include "hal_display.hpp"

TaskHandle_t comms_task_handle = NULL;
TaskHandle_t ui_task_handle = NULL;

QueueHandle_t commsQueue = NULL;
QueueHandle_t power_manager_queue = NULL;

std::mutex uiMutex;
UIState uiState;

void low_battery_shutdown()
{
    printf("Shutdown: suspending UI and comms tasks\n");
    vTaskSuspend(ui_task_handle);
    vTaskSuspend(comms_task_handle);
    
    printf("Shutdown: comms sleep\n");
    Comms::sleep();

    printf("Shutdown: IMU sleep\n");
    HalIMU::sleep();

    printf("Shutdown: display sleep\n");
    HalDisplay::sleep();

    printf("Shutdown: entering deep sleep\n");
    esp_deep_sleep_start();
}

void vPowerManagerTask(void *pvParameters)
{
    PowerManagerMessage message;

    while(1)
    {
        if (xQueueReceive(power_manager_queue, &message, portMAX_DELAY) == pdTRUE)
        {
            if (message.shutdown_requested)
            {
                printf("PowerManagerTask: received shutdown request\n");
                low_battery_shutdown();
            }
        }
    }
}

void vSensorTask(void *pvParameters)
{
    HalAnalog::init();
    HalIMU::init();

    GloveState currentState = {};

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        currentState.muxValues = HalAnalog::getSensorValues();
        currentState.batteryDividerMilliVolts = HalAnalog::readBatteryDividerMilliVolts();

        while (HalIMU::is_data_ready())
        {
            sh2_service();
        }

        float qX, qY, qZ, qReal;
        if (HalIMU::get_quaternion(qX, qY, qZ, qReal))
        {
            currentState.quatX = qX;
            currentState.quatY = qY;
            currentState.quatZ = qZ;
            currentState.quatReal = qReal;
        }

        EngineOutput output = GestureEngine::processData(currentState);

        // Prevent the glove from shutting down when plugged in to upload code.
        // If the battery level appears to be below 2V, which the mV reading can seem to be at when plugged in,
        // the battery cannot be connected and must be plugged in with a cable as this is an impossible voltage for a 3.7V LiPo battery to discharge to.
        if (output.batteryPct <= 0 && output.batteryMilliVolts > 2000)
        {
            PowerManagerMessage power_msg;
            power_msg.shutdown_requested = true;

            if (xQueueSend(power_manager_queue, &power_msg, 0) == pdTRUE)
            {
                printf("Shutdown requested: sensor task sent request and suspending self\n");
                vTaskSuspend(NULL);
            }
        }

        {
            std::lock_guard<std::mutex> lock(uiMutex);

            uiState.inputMode = output.newInputMode;
            uiState.mouseMode = output.newMouseMode;
            uiState.uiCursorX = output.uiCursorX;
            uiState.uiCursorY = output.uiCursorY;
            uiState.batteryPct = output.batteryPct;
            uiState.hasClicked = output.uiClick;
            uiState.yaw = output.yaw;
            uiState.roll = output.roll;

            for (int i = 0; i < 12; i++)
            {
                uiState.rightMuxValues[i] = currentState.muxValues[i];
            }
        }

        TickType_t current_tick = xTaskGetTickCount();
        static TickType_t last_heartbeat_tick = current_tick;

        bool time_for_heartbeat = (current_tick - last_heartbeat_tick) >= pdMS_TO_TICKS(2000);
        
        if ((output.modeChanged || time_for_heartbeat) && !Comms::left_peer.search_timed_out.load(std::memory_order_acquire))
        {
            last_heartbeat_tick = current_tick;

            CommsMessage left_comms_message = {};
            memcpy(left_comms_message.address, Comms::LEFT_GLOVE_ADDRESS, 6);
            left_comms_message.payload_length = sizeof(LeftModeUpdateMessage);
            
            left_comms_message.payload.mode_update_message.newInputMode = output.newInputMode;
            left_comms_message.payload.mode_update_message.newMouseMode = output.newMouseMode;
            left_comms_message.payload.mode_update_message.wakeUpComms = false;

            xQueueSend(commsQueue, &left_comms_message, 0);
        }
        
        if (!Comms::recv_peer.search_timed_out.load(std::memory_order_acquire))
        {
            CommsMessage recv_comms_message = {};
            memcpy(recv_comms_message.address, Comms::RECEIVER_ADDRESS, 6);
            recv_comms_message.payload_length = sizeof(ReceiverMessage);
    
            recv_comms_message.payload.receiver_message = output.message;
    
            xQueueSend(commsQueue, &recv_comms_message, 0);
        }

        BaseType_t task_delayed = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));

        if (task_delayed == pdFALSE)
        {
            printf("vSensorTask took longer than the delay interval.\n");
        }

    }

    vTaskDelete(NULL);
}

void vUITask(void *pvParameters)
{
    HalDisplay::init();

    UIState localUIState = {};

    while (1)
    {
        {
            std::lock_guard<std::mutex> lock(uiMutex);
            uiState.comms_status = Comms::comms_status;

            taskENTER_CRITICAL(&Comms::left_telemetry_mux);
            if (Comms::left_telemetry_available)
            {
                uiState.leftMuxValues = Comms::left_mux_values;
                Comms::left_telemetry_available = false;
            }
            taskEXIT_CRITICAL(&Comms::left_telemetry_mux);

            localUIState = uiState;
        }

        HalDisplay::update_display(localUIState);

        if (HalDisplay::comms_wakeup_requested)
        {
            uint8_t latest_input_mode;
            uint8_t latest_mouse_mode;
            
            {
                std::lock_guard<std::mutex> lock(uiMutex);
                latest_input_mode = uiState.inputMode;
                latest_mouse_mode = uiState.mouseMode;
            }

            Comms::wake_up_comms(latest_input_mode, latest_mouse_mode);
            HalDisplay::comms_wakeup_requested = false;
        }

        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    commsQueue = xQueueCreate(20, sizeof(CommsMessage));
    power_manager_queue = xQueueCreate(10, sizeof(PowerManagerMessage));

    Comms::init();

    xTaskCreatePinnedToCore(
        Comms::vCommsTask,
        "CommsTask",
        4096,
        NULL,
        4,
        &comms_task_handle,
        0);

    xTaskCreatePinnedToCore(
        vUITask,
        "UITask",
        8192,
        NULL,
        3,
        &ui_task_handle,
        0);

    xTaskCreatePinnedToCore(
        vSensorTask,
        "SensorTask",
        8192,
        NULL,
        5,
        NULL,
        1);

    xTaskCreatePinnedToCore(
        vPowerManagerTask,
        "PowerManagerTask",
        4096,
        NULL,
        3,
        NULL,
        1);
}