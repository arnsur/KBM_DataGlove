#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "board_config.hpp"
#include "battery_manager.hpp"
#include "hal_imu.hpp"

extern "C"
{
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

static i2c_master_bus_handle_t master_bus_handle;

void sensor_task(void *pvParameters)
{
    HalImu::init();

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
            printf("X: %6.3f | Y: %6.3f | Z: %6.3f | W: %6.3f\n", x, y, z, w);
        }
    }   
}

void battery_task(void *pvParameters)
{
    BatteryManager::init();
    
    BatteryManager::BatteryDetails details;

    while(1)
    {
        BatteryManager::battery_details(details);

        printf("Pct: %f | Capacity: %u mAh\n", details.pct, details.capacity_mah);

        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
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
