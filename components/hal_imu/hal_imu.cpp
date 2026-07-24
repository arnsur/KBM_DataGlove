#include "hal_imu.hpp"
#include "board_config.hpp"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
extern "C"
{
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

namespace HalIMU
{
    static i2c_master_bus_handle_t bus_handle;
    static i2c_master_dev_handle_t imu_handle;

    static volatile bool imu_interrupt_triggered = false;

    sh2_Hal_t hal;

    struct IMUData
    {
        float x;
        float y;
        float z;
        float real;
        bool new_data_available;
    };
    static IMUData imu_data = {0, 0, 0, 0, false};

    static void IRAM_ATTR imu_isr_handler(void *arg)
    {
        imu_interrupt_triggered = true;
    }

    void imu_callback(void *cookie, sh2_SensorEvent_t *pEvent)
    {
        sh2_SensorValue_t value;
        sh2_decodeSensorEvent(&value, pEvent);

        if (value.sensorId == SH2_GAME_ROTATION_VECTOR)
        {
            ((IMUData *)cookie)->x = value.un.gameRotationVector.i;
            ((IMUData *)cookie)->y = value.un.gameRotationVector.j;
            ((IMUData *)cookie)->z = value.un.gameRotationVector.k;
            ((IMUData *)cookie)->real = value.un.gameRotationVector.real;

            ((IMUData *)cookie)->new_data_available = true;
        }
    }

    bool is_data_ready()
    {
        if (imu_interrupt_triggered)
        {
            imu_interrupt_triggered = false;
            return true;
        }
        return false;
    }

    static int sh2_open(sh2_Hal_t *self)
    {
        gpio_set_level(IMU_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(IMU_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(100));

        gpio_intr_enable(IMU_INT);

        return 0;
    }

    static void sh2_close(sh2_Hal_t *self)
    {
        gpio_intr_disable(IMU_INT);
        gpio_set_level(IMU_RST, 0);
    }

    static int sh2_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
    {
        esp_err_t err = i2c_master_receive(imu_handle, pBuffer, 4, pdMS_TO_TICKS(50));
        if (err != ESP_OK)
            return -1;

        uint16_t packet_len = ((pBuffer[1] & 0x7F) << 8) | pBuffer[0];
        if (packet_len < 4 || packet_len > len)
            return -1;

        if (packet_len > 4)
        {
            err = i2c_master_receive(imu_handle, pBuffer + 4, packet_len - 4, pdMS_TO_TICKS(50));
            if (err != ESP_OK)
                return -1;
        }

        *t_us = (uint32_t)esp_timer_get_time();

        return packet_len;
    }

    static int sh2_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
    {
        if (pBuffer == nullptr || len == 0)
            return -1;

        esp_err_t err = i2c_master_transmit(imu_handle, pBuffer, len, pdMS_TO_TICKS(50));

        if (err != ESP_OK)
            return -1;

        return (int)len;
    }

    static uint32_t sh2_get_time_us(sh2_Hal_t *self)
    {
        return (uint32_t)esp_timer_get_time();
    }

    void init()
    {
        i2c_master_bus_config_t i2c_config = {};
        i2c_config.i2c_port = I2C_NUM_0;
        i2c_config.sda_io_num = I2C_SDA;
        i2c_config.scl_io_num = I2C_SCL;
        i2c_config.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_config.glitch_ignore_cnt = 7;
        i2c_config.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &bus_handle));

        i2c_device_config_t device_config = {};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = IMU_ADDRESS;
        device_config.scl_speed_hz = 400000;
        device_config.scl_wait_us = 5000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &device_config, &imu_handle));

        gpio_config_t rst_config = {};
        rst_config.pin_bit_mask = (1ULL << IMU_RST);
        rst_config.mode = GPIO_MODE_OUTPUT;
        rst_config.pull_up_en = GPIO_PULLUP_DISABLE;
        rst_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        rst_config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&rst_config));
        gpio_set_level(IMU_RST, 1);

        gpio_config_t int_config = {};
        int_config.pin_bit_mask = (1ULL << IMU_INT);
        int_config.mode = GPIO_MODE_INPUT;
        int_config.pull_up_en = GPIO_PULLUP_ENABLE;
        int_config.intr_type = GPIO_INTR_NEGEDGE;
        ESP_ERROR_CHECK(gpio_config(&int_config));

        ESP_ERROR_CHECK(gpio_isr_handler_add(IMU_INT, imu_isr_handler, nullptr));

        hal.open = sh2_open;
        hal.close = sh2_close;
        hal.read = sh2_read;
        hal.write = sh2_write;
        hal.getTimeUs = sh2_get_time_us;

        sh2_setSensorCallback(imu_callback, &imu_data);
    }

    bool get_quaternion(float &x, float &y, float &z, float &real)
    {
        if (!imu_data.new_data_available)
            return false;

        x = imu_data.x;
        y = imu_data.y;
        z = imu_data.z;
        real = imu_data.real;

        imu_data.new_data_available = false;

        return true;
    }
}