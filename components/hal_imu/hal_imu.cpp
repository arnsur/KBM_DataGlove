#include "hal_imu.hpp"
#include "board_config.hpp"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include <cstring>

extern "C"
{
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

namespace HalIMU
{
    static i2c_master_bus_handle_t bus_handle = nullptr;
    static i2c_master_dev_handle_t imu_handle = nullptr;

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

    static int hal_sh2_open(sh2_Hal_t *self)
    {
        int timeout_loops = 100;
        while (gpio_get_level(IMU_INT) != 0 && timeout_loops > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout_loops--;
        }

        if (timeout_loops == 0)
        {
            printf("CRITICAL ERROR: BNO085 did not wake up!\n");
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(200));

        gpio_intr_enable(IMU_INT);
        return 0;
    }

    static void hal_sh2_close(sh2_Hal_t *self)
    {
        gpio_intr_disable(IMU_INT);
        gpio_set_level(IMU_RST, 0);

        if (bus_handle)
        {
            i2c_del_master_bus(bus_handle);
            bus_handle = nullptr;
            imu_handle = nullptr;
        }
    }

    static int hal_sh2_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
    {
        int gpio_level = gpio_get_level(IMU_INT);
        if (gpio_level != 0)
        {
            return 0;
        }

        esp_rom_delay_us(200);

        unsigned read_len = (len < 256) ? len : 256;

        esp_err_t err = i2c_master_receive(imu_handle, pBuffer, read_len, pdMS_TO_TICKS(200));
        if (err != ESP_OK)
        {
            return 0;
        }

        uint16_t packet_len = ((pBuffer[1] & 0x7F) << 8) | pBuffer[0];

        if (packet_len < 4 || packet_len > len)
        {
            return 0;
        }

        *t_us = (uint32_t)esp_timer_get_time();
        return packet_len;
    }

    static int hal_sh2_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
    {
        if (pBuffer == nullptr || len == 0)
            return -1;
        esp_err_t err = i2c_master_transmit(imu_handle, pBuffer, len, pdMS_TO_TICKS(200));
        if (err != ESP_OK)
        {
            return -1;
        }

        return (int)len;
    }

    static uint32_t hal_sh2_get_time_us(sh2_Hal_t *self)
    {
        return (uint32_t)esp_timer_get_time();
    }

    void init()
    {
        if (bus_handle)
        {
            i2c_del_master_bus(bus_handle);
            bus_handle = nullptr;
            imu_handle = nullptr;
        }

        gpio_reset_pin(IMU_RST);
        gpio_reset_pin(IMU_INT);

        // Configure IMU Reset Pin
        gpio_config_t rst_config = {};
        rst_config.pin_bit_mask = (1ULL << IMU_RST);
        rst_config.mode = GPIO_MODE_OUTPUT;
        rst_config.pull_up_en = GPIO_PULLUP_DISABLE;
        rst_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        rst_config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&rst_config));

        gpio_set_level(IMU_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(IMU_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(500));

        // I2C master bus config
        i2c_master_bus_config_t i2c_config = {};
        i2c_config.i2c_port = I2C_NUM_0;
        i2c_config.sda_io_num = I2C_SDA;
        i2c_config.scl_io_num = I2C_SCL;
        i2c_config.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_config.glitch_ignore_cnt = 0;
        i2c_config.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &bus_handle));

        ESP_ERROR_CHECK(i2c_master_bus_reset(bus_handle));
        vTaskDelay(pdMS_TO_TICKS(10));

        // I2C device config
        i2c_device_config_t device_config = {};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = IMU_ADDRESS;
        device_config.scl_speed_hz = 400000;
        device_config.scl_wait_us = 13107;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &device_config, &imu_handle));

        // Configure IMU Interrupt Pin
        gpio_config_t int_config = {};
        int_config.pin_bit_mask = (1ULL << IMU_INT);
        int_config.mode = GPIO_MODE_INPUT;
        int_config.pull_up_en = GPIO_PULLUP_ENABLE;
        int_config.intr_type = GPIO_INTR_NEGEDGE;
        ESP_ERROR_CHECK(gpio_config(&int_config));

        ESP_ERROR_CHECK(gpio_isr_handler_add(IMU_INT, imu_isr_handler, nullptr));

        // Set SH2 callbacks
        hal.open = hal_sh2_open;
        hal.close = hal_sh2_close;
        hal.read = hal_sh2_read;
        hal.write = hal_sh2_write;
        hal.getTimeUs = hal_sh2_get_time_us;

        sh2_open(&hal, nullptr, nullptr);

        for (int i = 0; i < 30; i++)
        {
            if (gpio_get_level(IMU_INT) == 0)
            {
                sh2_service();
            }
            vTaskDelay(pdMS_TO_TICKS(15));
        }

        sh2_setSensorCallback(imu_callback, &imu_data);

        sh2_SensorConfig_t config = {};
        config.changeSensitivityEnabled = false;
        config.wakeupEnabled = false;
        config.changeSensitivityRelative = false;
        config.alwaysOnEnabled = false;
        config.changeSensitivity = 0;
        config.reportInterval_us = 10000;
        config.sensorSpecific = 0;
        sh2_setSensorConfig(SH2_GAME_ROTATION_VECTOR, &config);
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

    void sleep()
    {
        gpio_intr_disable(IMU_INT);
        sh2_devSleep();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}