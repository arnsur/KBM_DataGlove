#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mutex>

extern "C" {
    #include "sh2.h"
    #include "sh2_err.h"
    #include "sh2_hal.h"
}

#include "glove_types.hpp"
#include "hal_analog.hpp"
#include "hal_imu.hpp"
#include "gesture_engine.hpp"

void vSensorTask(void *pvParameters) {
    HalAnalog::init();
    HalIMU::init();

    GloveState currentState = {};
    
    while (1)
    {
        currentState.muxValues = HalAnalog::getSensorValues();

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

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    xTaskCreatePinnedToCore(
        vSensorTask,
        "SensorTask",
        8192,
        NULL,
        5,
        NULL,
        1
    );
}