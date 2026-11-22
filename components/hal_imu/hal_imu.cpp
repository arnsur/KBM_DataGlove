#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_timer.h"

extern "C"
{
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

#include "hal_imu.hpp"
#include "board_config.hpp"

namespace HalImu
{
    struct ImuData
    {
        float x;
        float y;
        float z;
        float w;
        bool new_data_available;
    };
    ImuData data = {0, 0, 0, 0, false};

    sh2_Hal_t hal = {};

    static i2c_master_dev_handle_t imu_device_handle = nullptr;

    static TaskHandle_t sensor_task_handle = nullptr;

    static void IRAM_ATTR imu_int_isr_handler(void *arg)
    {
        BaseType_t higher_priority_task_woken = pdFALSE;
        if (sensor_task_handle != nullptr)
        {
            vTaskNotifyGiveFromISR(sensor_task_handle, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
    }

    int open(sh2_Hal_t *self)
    {
        gpio_reset_pin(BoardConfig::IMU_NRST);
        gpio_reset_pin(BoardConfig::IMU_H_INTN);

        // Configure reset pin
        gpio_config_t rst_config = {};
        rst_config.pin_bit_mask = (1ULL << BoardConfig::IMU_NRST);
        rst_config.mode = GPIO_MODE_OUTPUT;
        rst_config.pull_up_en = GPIO_PULLUP_DISABLE;
        rst_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        rst_config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&rst_config));
        
        // Configure interrupt pin
        gpio_config_t int_config = {};
        int_config.pin_bit_mask = (1ULL << BoardConfig::IMU_H_INTN);
        int_config.mode = GPIO_MODE_INPUT;
        int_config.pull_up_en = GPIO_PULLUP_ENABLE;
        int_config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&int_config));

        gpio_set_level(BoardConfig::IMU_NRST, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(BoardConfig::IMU_NRST, 1);

        int timeout_loops = 100;
        while (gpio_get_level(BoardConfig::IMU_H_INTN) != 0 && timeout_loops > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout_loops--;
        }

        if (timeout_loops == 0)
        {
            printf("BNO086 DID NOT WAKE UP\n");
            return -1;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        i2c_master_bus_handle_t master_bus_handle = nullptr;
        ESP_ERROR_CHECK(i2c_master_get_bus_handle(0, &master_bus_handle));

        i2c_device_config_t device_config = {};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = BoardConfig::I2C_IMU_ADDR;
        device_config.scl_speed_hz = 400'000;
        device_config.scl_wait_us = 13107;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(master_bus_handle, &device_config, &imu_device_handle));

        return 0;
    }

    void close(sh2_Hal_t *self)
    {
        gpio_set_level(BoardConfig::IMU_NRST, 0);
    }

    int read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
    {
        if (gpio_get_level(BoardConfig::IMU_H_INTN) != 0) return 0;

        uint8_t header[4] = {0};
        if (i2c_master_receive(imu_device_handle, header, 4, 100) != ESP_OK) return 0;

        uint16_t packet_len = ((uint16_t)(header[1] & 0x7F) << 8) | header[0];
        if (packet_len < 4 || packet_len > len)
        {
            return 0;
        }

        if (i2c_master_receive(imu_device_handle, pBuffer, packet_len, 100) != ESP_OK) return 0;

        *t_us = (uint32_t)esp_timer_get_time();
        return (int)packet_len;
    }

    int write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
    {
        if (pBuffer == NULL || len == 0) return -1;

        if (i2c_master_transmit(imu_device_handle, pBuffer, len, 200) != ESP_OK) return -1;

        return (int)len;
    }

    uint32_t getTimeUs(sh2_Hal_t *self)
    {
        return (uint32_t)esp_timer_get_time();
    }

    void callback(void *cookie, sh2_SensorEvent_t *pEvent)
    {
        sh2_SensorValue_t value;
        sh2_decodeSensorEvent(&value, pEvent);

        if (value.sensorId == SH2_GAME_ROTATION_VECTOR)
        {
            ((ImuData *)cookie)->x = value.un.gameRotationVector.i;
            ((ImuData *)cookie)->y = value.un.gameRotationVector.j;
            ((ImuData *)cookie)->z = value.un.gameRotationVector.k;
            ((ImuData *)cookie)->w = value.un.gameRotationVector.real;

            ((ImuData *)cookie)->new_data_available = true;
        }
    }

    bool quaternion(float &x, float &y, float &z, float &real)
    {
        if (!data.new_data_available)
            return false;

        x = data.x;
        y = data.y;
        z = data.z;
        real = data.w;

        data.new_data_available = false;

        return true;
    }

    bool is_data_ready()
    {
        return gpio_get_level(BoardConfig::IMU_H_INTN) == 0;
    }

    void init()
    {
        hal.open = open;
        hal.close = close;
        hal.read = read;
        hal.write = write;
        hal.getTimeUs = getTimeUs;

        sh2_open(&hal, NULL, NULL);

        // Clear advertisement packets
        for (int i = 0; i < 30; i++)
        {
            if (is_data_ready())
            {
                sh2_service();
            }
            vTaskDelay(pdMS_TO_TICKS(15));
        }

        sensor_task_handle = xTaskGetCurrentTaskHandle();

        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        ESP_ERROR_CHECK(gpio_set_intr_type(BoardConfig::IMU_H_INTN, GPIO_INTR_NEGEDGE));
        ESP_ERROR_CHECK(gpio_isr_handler_add(BoardConfig::IMU_H_INTN, imu_int_isr_handler, nullptr));

        sh2_setSensorCallback(callback, &data);

        sh2_SensorConfig_t sensor_config = {};
        sensor_config.changeSensitivityEnabled = false;
        sensor_config.wakeupEnabled = false;
        sensor_config.changeSensitivityRelative = false;
        sensor_config.alwaysOnEnabled = false;
        sensor_config.changeSensitivity = 0;
        sensor_config.reportInterval_us = 10000;
        sensor_config.sensorSpecific = 0;
        sh2_setSensorConfig(SH2_GAME_ROTATION_VECTOR, &sensor_config);
    }
}