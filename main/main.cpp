#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mutex>

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

QueueHandle_t commsQueue = NULL;

std::mutex uiMutex;
UIState uiState;

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
        // TODO: add low battery mV shutdown

        {
            std::lock_guard<std::mutex> lock(uiMutex);

            uiState.inputMode = output.newInputMode;
            uiState.mouseMode = output.newMouseMode;
            uiState.uiCursorX = output.uiCursorX;
            uiState.uiCursorY = output.uiCursorY;
            uiState.batteryPct = output.batteryPct;
            uiState.hasClicked = output.uiClick;

            for (int i = 0; i < 12; i++)
            {
                uiState.muxValues[i] = currentState.muxValues[i];
            }
        }

        xQueueSend(commsQueue, &output.message, 0);

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

    commsQueue = xQueueCreate(10, sizeof(DataMessage));
    Comms::init();

    xTaskCreatePinnedToCore(
        Comms::vCommsTask,
        "CommsTask",
        4096,
        NULL,
        4,
        NULL,
        0);

    xTaskCreatePinnedToCore(
        vUITask,
        "UITask",
        8192,
        NULL,
        1,
        NULL,
        0);

    xTaskCreatePinnedToCore(
        vSensorTask,
        "SensorTask",
        8192,
        NULL,
        5,
        NULL,
        1);
}