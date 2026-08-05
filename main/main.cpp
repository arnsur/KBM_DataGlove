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

    while (1)
    {
        currentState.muxValues = HalAnalog::getSensorValues();
        currentState.batteryDividerMilliVolts = HalAnalog::readBatteryDividerMilliVolts();

        if (HalIMU::is_data_ready())
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
                uiState.muxValues[i] = currentState.muxValues[i];
            }
        }

        CommsMessage comms_message = {};
        memcpy(comms_message.address, Comms::RECEIVER_ADDRESS, 6);
        comms_message.payload_length = sizeof(ReceiverMessage);

        comms_message.payload.receiver_message = output.message;

        xQueueSend(commsQueue, &comms_message, 0);

        vTaskDelay(pdMS_TO_TICKS(10));
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
            localUIState = uiState;
        }

        HalDisplay::update_display(localUIState);

        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    commsQueue = xQueueCreate(10, sizeof(CommsMessage));
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
        1,
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