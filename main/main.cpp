#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "board_config.hpp"
#include "battery_manager.hpp"
#include "hal_imu.hpp"
#include "hal_analog.hpp"
#include "gesture_engine.hpp"
#include "comms.hpp"

extern "C"
{
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

static i2c_master_bus_handle_t master_bus_handle;

static QueueHandle_t ui_state_queue = nullptr;
static QueueHandle_t ui_batt_queue = nullptr;

void sensor_task(void *pvParameters)
{
    HalImu::enable_interrupts();

    GestureEngine::GloveState state = {};
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

        state.sensors = HalAnalog::sensor_values();

        GestureEngine::queue_push_data(state);
        xQueueOverwrite(ui_state_queue, &state);
    }   
}

void battery_task(void *pvParameters)
{    
    BatteryManager::BatteryDetails batt_details;

    int last_chrg_ma = 0;

    while(1)
    {
        BatteryManager::battery_details(batt_details);

        xQueueOverwrite(ui_batt_queue, &batt_details);

        // Put the the BQ25040 into ISET Programmed on the first loop or when a charging cable is plugged in
        if (last_chrg_ma <= 0 && batt_details.i_chrg_ma > 0)
        {
            BatteryManager::select_charge_mode(BatteryManager::ChargeMode::ISET_PROGRAMMED);
        }
        last_chrg_ma = batt_details.i_chrg_ma;

        vTaskDelay(pdMS_TO_TICKS(2 * 1000));
    }
}

void gesture_task(void *pvParameters)
{
    while(1)
    {
        GestureEngine::EngineOutput output = GestureEngine::process();

        Comms::queue_push_data(output.recv_msg);
    }
}

// Just for queue testing at the moment
void disp_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    GestureEngine::GloveState state = {};
    BatteryManager::BatteryDetails batt_details = {};

    while(1)
    {
        xQueueReceive(ui_state_queue, &state, 0);
        xQueueReceive(ui_batt_queue, &batt_details, 0);

        printf("Pct: %f%% | Capacity: %u mAh | I_chrg: %d mA",
            batt_details.pct, batt_details.capacity_mah, batt_details.i_chrg_ma);
        printf("| X: %6.3f | Y: %6.3f | Z: %6.3f | W: %6.3f",
            state.q_x, state.q_y, state.q_z, state.q_w);
        printf("\n");

        // for (int i = 0; i < 10; i++)
        // {
        //     printf("| %d: %u |", i, state.sensors[i]);
        // }
        // printf("\n");

        if(xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(33)) == pdFALSE)
        {
            printf("DISPLAY TASK MISSED ITS DEADLINE.\n");
        }
    }
}

void comms_task(void *pvParameters)
{
    while(1)
    {
        Comms::send_data();
    }
}

extern "C" void app_main(void)
{
    ui_state_queue = xQueueCreate(1, sizeof(GestureEngine::GloveState));
    ui_batt_queue = xQueueCreate(1, sizeof(BatteryManager::BatteryDetails));
    
    i2c_master_bus_config_t master_bus_config = {};
    master_bus_config.i2c_port = 0;
    master_bus_config.sda_io_num = BoardConfig::I2C_SDA;
    master_bus_config.scl_io_num = BoardConfig::I2C_SCL;
    master_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    master_bus_config.glitch_ignore_cnt = 0;
    master_bus_config.intr_priority = 0;
    master_bus_config.flags.enable_internal_pullup = false;
    ESP_ERROR_CHECK(i2c_new_master_bus(&master_bus_config, &master_bus_handle));

    HalImu::init();
    HalAnalog::init();
    BatteryManager::init();
    GestureEngine::init();
    Comms::init();

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
        comms_task,
        "Comms Task",
        4096,
        NULL,
        4,
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

    xTaskCreatePinnedToCore(
        gesture_task,
        "Gesture Task",
        4096,
        NULL,
        4,
        NULL,
        1
    );
}
