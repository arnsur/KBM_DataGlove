#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_sleep.h"

#include "board_config.hpp"
#include "battery_manager.hpp"
#include "hal_imu.hpp"
#include "hal_analog.hpp"
#include "gesture_engine.hpp"
#include "hal_ui.hpp"
#include "glove_ui.hpp"
#include "comms.hpp"

extern "C"
{
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

static i2c_master_bus_handle_t master_bus_handle;

static QueueHandle_t ui_sensor_queue = nullptr;
static QueueHandle_t ui_batt_queue = nullptr;
static QueueHandle_t ui_gesture_queue = nullptr;

static bool shutdown_requested;
EventGroupHandle_t shutdown_event_group_handle = nullptr;
constexpr static uint32_t SENSOR_SHUTDOWN_BIT = (1U << 0);
constexpr static uint32_t GESTURE_SHUTDOWN_BIT = (1U << 1);
constexpr static uint32_t DISP_SHUTDOWN_BIT = (1U << 2);
constexpr static uint32_t COMMS_SHUTDOWN_BIT = (1U << 3);
constexpr static uint32_t SHUTDOWN_GO = (SENSOR_SHUTDOWN_BIT | GESTURE_SHUTDOWN_BIT | DISP_SHUTDOWN_BIT | COMMS_SHUTDOWN_BIT);

void sensor_task(void *pvParameters)
{
    
    HalImu::enable_interrupts();
    
    GestureEngine::GloveState state = {};
    while (1)
    {
        if (shutdown_requested)
        {
            HalAnalog::sleep();
            HalImu::sleep();

            xEventGroupSetBits(shutdown_event_group_handle, SENSOR_SHUTDOWN_BIT);
            vTaskDelete(NULL);
        }

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
        xQueueOverwrite(ui_sensor_queue, &state);
    }   
}

void battery_task(void *pvParameters)
{    
    BatteryManager::BatteryDetails batt_details;

    int last_chrg_ma = 0;

    while(1)
    {
        if (!shutdown_requested)
        {
            shutdown_requested = (BatteryManager::battery_details(batt_details) == false);

            xQueueOverwrite(ui_batt_queue, &batt_details);

            // Put the the BQ25040 into ISET Programmed on the first loop or when a charging cable is plugged in
            if (last_chrg_ma <= 0 && batt_details.i_chrg_ma > 0)
            {
                BatteryManager::select_charge_mode(BatteryManager::ChargeMode::ISET_PROGRAMMED);
            }
            last_chrg_ma = batt_details.i_chrg_ma;
        }
        EventBits_t shutdown_bits = xEventGroupGetBits(shutdown_event_group_handle);

        if ((shutdown_bits & SHUTDOWN_GO) == SHUTDOWN_GO)
        {
            BatteryManager::sleep();
            
            esp_deep_sleep_start();

            vTaskDelete(NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(2 * 1000));
    }
}

void gesture_task(void *pvParameters)
{
    while(1)
    {
        if (shutdown_requested)
        {
            xEventGroupSetBits(shutdown_event_group_handle, GESTURE_SHUTDOWN_BIT);
            vTaskDelete(NULL);
        }

        GestureEngine::EngineOutput output = GestureEngine::process();

        xQueueOverwrite(ui_gesture_queue, &output.ui_output);

        Comms::queue_push_data(output.recv_msg);
    }
}

void disp_task(void *pvParameters)
{
    HalUI::init();
    GloveUI::init(50);

    TickType_t last_wake_time = xTaskGetTickCount();

    GestureEngine::GloveState r_state = {};
    BatteryManager::BatteryDetails r_batt_details = {};
    GestureEngine::UIOutput ui_output = {};

    while(1)
    {
        if (shutdown_requested)
        {
            xEventGroupSetBits(shutdown_event_group_handle, DISP_SHUTDOWN_BIT);
            vTaskDelete(NULL);
        }

        xQueueReceive(ui_sensor_queue, &r_state, 0);
        xQueueReceive(ui_batt_queue, &r_batt_details, 0);
        xQueueReceive(ui_gesture_queue, &ui_output, 0);

        GloveUI::UIState ui_state = {};

        ui_state.active_mode = ui_output.active_mode;
        ui_state.pre_rest_mode = ui_output.pre_rest_mode;
        ui_state.mode_substate = ui_output.mode_substate;
        ui_state.cursor = ui_output.cursor;

        GloveUI::CommsStatus comms_status; // Placeholder
        ui_state.comms = comms_status;

        ui_state.right_glove = r_state;
        GestureEngine::GloveState l_state; // Placeholder
        ui_state.left_glove = l_state;

        ui_state.right_batt = r_batt_details;
        BatteryManager::BatteryDetails l_batt; //Placeholder
        ui_state.left_batt = l_batt;

        GloveUI::UICommands commands;
        GloveUI::update(ui_state, commands);

        if (commands.brightness_changed)
        {
            HalUI::set_brightness_pct(commands.brightness_pct);
        }

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
        if (shutdown_requested)
        {
            Comms::sleep();

            xEventGroupSetBits(shutdown_event_group_handle, COMMS_SHUTDOWN_BIT);
            vTaskDelete(NULL);
        }

        Comms::send_data();
    }
}

extern "C" void app_main(void)
{
    shutdown_event_group_handle = xEventGroupCreate();

    gpio_hold_dis(BoardConfig::MUX_S0);
    gpio_hold_dis(BoardConfig::MUX_S1);
    gpio_hold_dis(BoardConfig::MUX_S2);
    gpio_hold_dis(BoardConfig::MUX_S3);
    gpio_hold_dis(BoardConfig::IMU_NRST);
    gpio_hold_dis(BoardConfig::CHG_EN_SET);

    ui_sensor_queue = xQueueCreate(1, sizeof(GestureEngine::GloveState));
    ui_batt_queue = xQueueCreate(1, sizeof(BatteryManager::BatteryDetails));
    ui_gesture_queue = xQueueCreate(1, sizeof(GestureEngine::UIOutput));
    
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
