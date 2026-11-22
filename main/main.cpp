#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "board_config.hpp"
#include "battery_manager.hpp"
#include "hal_imu.hpp"
#include "hal_analog.hpp"

extern "C"
{
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

struct GloveState
{
    HalAnalog::SensorArray sensors;
    float q_x;
    float q_y;
    float q_z;
    float q_w;
};


static i2c_master_bus_handle_t master_bus_handle;

QueueHandle_t gesture_state_queue = xQueueCreate(1, sizeof(GloveState));
QueueHandle_t ui_state_queue = xQueueCreate(1, sizeof(GloveState));
QueueHandle_t ui_batt_queue = xQueueCreate(1, sizeof(BatteryManager::BatteryDetails));

void sensor_task(void *pvParameters)
{
    HalImu::init();

    GloveState state = {};
    while (1)
    {
        if (!HalImu::is_data_ready())
        {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        }

        while (HalImu::is_data_ready())
        {
            sh2_service();
        }

        float x, y, z, w;
        if (HalImu::quaternion(x, y, z, w))
        {
            state.q_x = x;
            state.q_y = y;
            state.q_z = z;
            state.q_w = w;
        }

        // Add analog behavior

        xQueueOverwrite(gesture_state_queue, &state);
        xQueueOverwrite(ui_state_queue, &state);
    }   
}

void battery_task(void *pvParameters)
{
    BatteryManager::init();
    
    BatteryManager::BatteryDetails batt_details;

    while(1)
    {
        BatteryManager::battery_details(batt_details);

        xQueueOverwrite(ui_batt_queue, &batt_details);

        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
}

// Just for queue testing at the moment
void disp_task(void *pvParameters)
{
    GloveState state;
    BatteryManager::BatteryDetails batt_details;

    while(1)
    {
        xQueueReceive(ui_state_queue, &state, 0);
        xQueueReceive(ui_batt_queue, &batt_details, 0);

        printf("Pct: %f%% | Capacity: %u mAh | X: %6.3f | Y: %6.3f | Z: %6.3f | W: %6.3f\n",
            batt_details.pct, batt_details.capacity_mah, state.q_x, state.q_y, state.q_z, state.q_w);

        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

extern "C" void app_main(void)
{
    i2c_master_bus_config_t master_bus_config = {};
    master_bus_config.i2c_port = 0;
    master_bus_config.sda_io_num = BoardConfig::I2C_SDA;
    master_bus_config.scl_io_num = BoardConfig::I2C_SCL;
    master_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    master_bus_config.glitch_ignore_cnt = 0;
    master_bus_config.intr_priority = 0;
    master_bus_config.flags.enable_internal_pullup = false;
    ESP_ERROR_CHECK(i2c_new_master_bus(&master_bus_config, &master_bus_handle));

    xTaskCreatePinnedToCore(
        disp_task,
        "Display Task",
        8192,
        NULL,
        3,
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        battery_task,
        "Battery Task",
        4096,
        NULL,
        3,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        sensor_task,
        "Sensor Task",
        8192,
        NULL,
        5,
        NULL,
        1
    );
}
