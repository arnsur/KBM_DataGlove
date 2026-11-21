#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_config.hpp"
#include "battery_manager.hpp"

namespace BatteryManager
{
    static constexpr uint16_t BATTERY_CAP_MILLIAH = 2000;
    static constexpr uint16_t R_SENSE_MILLIOHM = 5;
    static constexpr uint16_t R_IFULL = 1600;
    static constexpr uint16_t R_ISET = 560;
    static constexpr uint16_t BATT_EMPTY_MILLIV = 3000;
    static constexpr uint16_t BATT_RECOVERY_MILLIV = 3880;
    
    static constexpr float IFULL_MILLIAMP = (R_IFULL * 0.01f / 200) * (530.0f/R_ISET) * 1000;
    static constexpr float CAP_LSB_SCALED = 5.0 / R_SENSE_MILLIOHM; // Scaled 1,000,000x

    static constexpr uint16_t DESIGN_CAP = (uint16_t)(BATTERY_CAP_MILLIAH / CAP_LSB_SCALED);
    static constexpr uint16_t ICHGTERM = (uint16_t)(IFULL_MILLIAMP * R_SENSE_MILLIOHM / 1.5625f);
    static constexpr uint16_t V_EMPTY = ((BATT_EMPTY_MILLIV / 10) << 7) | (3880 / 40);

    static i2c_master_bus_handle_t master_bus_handle;
    static i2c_master_dev_handle_t device_handle;
    
    /**
     * @brief Writes a value to a register
     * 
     * @param reg register to write to
     * @param value value to write
     */
    void write_register(uint8_t reg, uint16_t value)
    {
        uint8_t write_buf[3];
        write_buf[0] = reg;
        write_buf[1] = (uint8_t)(value & 0xFF);
        write_buf[2] = (uint8_t)(value >> 8);

        ESP_ERROR_CHECK(i2c_master_transmit(device_handle, write_buf, 3, -1));
    }

    /**
     * @brief Read the value at a register
     * 
     * @param reg register to read from
     * @return uint16_t
     */
    uint16_t read_register(uint8_t reg)
    {
        uint8_t read_buf[2];

        ESP_ERROR_CHECK(i2c_master_transmit_receive(device_handle, &reg, 1, read_buf, 2, -1));
        return ((uint16_t)read_buf[1] << 8) | (uint16_t)read_buf[0];
    }

    /**
     * @brief Writes a value to a register and verifies if it has been written correctly
     * 
     * @param reg register to write to
     * @param value value to write
     */
    void write_verify_register(uint8_t reg, uint16_t value)
    {
        uint32_t attempts = 0;
        uint16_t reg_value_read = 0;

        do
        {
            write_register(reg, value);
            vTaskDelay(pdMS_TO_TICKS(1));
            reg_value_read = read_register(reg);
        }
        while (value != reg_value_read && attempts++ < 3);
    }

    /**
     * @brief Initializes the IC's registers when the POR bit is 1. Does nothing otherwise.
     * 
     */
    void init_ic()
    {
        if ((read_register(0x00) & 0x0002) != 0 || read_register(0x18) != DESIGN_CAP)
        {
            while(read_register(0x3D) & 1) vTaskDelay(pdMS_TO_TICKS(10));

            uint16_t hib_cfg = read_register(0xBA);
            write_register(0x60, 0x90);
            write_register(0xBA, 0x00);
            write_register(0x60, 0x00);

            write_register(0x18, DESIGN_CAP);
            write_register(0x1E, ICHGTERM);
            write_register(0x3A, V_EMPTY);

            write_register(0xDB, 0x8000);

            while (read_register(0xDB) & 0x8000) vTaskDelay(pdMS_TO_TICKS(10));

            write_register(0xBA, hib_cfg);

            // Reset POR bit and verify
            write_verify_register(0x00, read_register(0x00) & 0xFFFD);
        }
    }

    /**
     * @brief Initializes I2C bus and device. Initializes the IC's registers after doing so.
     * 
     */
    void init()
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

        i2c_device_config_t device_config = {};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = BoardConfig::I2C_MAX_ADDR;
        device_config.scl_speed_hz = 400'000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(master_bus_handle, &device_config, &device_handle));

        init_ic();
    }

    /**
     * @brief Get battery details from the MAX IC
     * 
     * @param details struct to store the details in
     */
    void battery_details(BatteryDetails &details)
    {
        init_ic();

        uint16_t raw_soc = read_register(0x06);

        details.capacity_mah = read_register(0x05) * CAP_LSB_SCALED;
        details.pct = (raw_soc >> 8) + (raw_soc & 0xFF)/256.0f;
    }
}