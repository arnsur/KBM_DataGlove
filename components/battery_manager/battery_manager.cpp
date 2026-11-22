#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

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
    static constexpr float CAP_LSB = 5.0f / R_SENSE_MILLIOHM;
    static constexpr float CURRENT_LSB = 1.5625f / R_SENSE_MILLIOHM;

    static constexpr uint16_t DESIGN_CAP = (uint16_t)(BATTERY_CAP_MILLIAH / CAP_LSB);
    static constexpr uint16_t ICHGTERM = (uint16_t)(IFULL_MILLIAMP / CAP_LSB);
    static constexpr uint16_t V_EMPTY = ((BATT_EMPTY_MILLIV / 10) << 7) | (3880 / 40);

    static i2c_master_dev_handle_t chrg_device_handle = nullptr;
    
    /**
     * @brief Writes a value to a register on the MAX charger
     * 
     * @param reg register to write to
     * @param value value to write
     */
    void chrg_write_register(uint8_t reg, uint16_t value)
    {
        uint8_t write_buf[3];
        write_buf[0] = reg;
        write_buf[1] = (uint8_t)(value & 0xFF);
        write_buf[2] = (uint8_t)(value >> 8);

        ESP_ERROR_CHECK(i2c_master_transmit(chrg_device_handle, write_buf, 3, -1));
    }

    /**
     * @brief Read the value at a register on the MAX charger
     * 
     * @param reg register to read from
     * @return uint16_t
     */
    uint16_t chrg_read_register(uint8_t reg)
    {
        uint8_t read_buf[2];

        ESP_ERROR_CHECK(i2c_master_transmit_receive(chrg_device_handle, &reg, 1, read_buf, 2, -1));
        return ((uint16_t)read_buf[1] << 8) | (uint16_t)read_buf[0];
    }

    /**
     * @brief Writes a value to a register on the MAX charger and verifies if it has been written correctly
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
            chrg_write_register(reg, value);
            vTaskDelay(pdMS_TO_TICKS(1));
            reg_value_read = chrg_read_register(reg);
        }
        while (value != reg_value_read && attempts++ < 3);
    }

    /**
     * @brief Initializes the MAX IC's registers when the POR bit is 1 or if the battery's stored nominal capacity value is mismatched. Does nothing otherwise.
     * 
     */
    void init_chrg_ic()
    {
        if ((chrg_read_register(0x00) & 0x0002) != 0 || chrg_read_register(0x18) != DESIGN_CAP)
        {
            while(chrg_read_register(0x3D) & 1) vTaskDelay(pdMS_TO_TICKS(10));

            uint16_t hib_cfg = chrg_read_register(0xBA);
            chrg_write_register(0x60, 0x90);
            chrg_write_register(0xBA, 0x00);
            chrg_write_register(0x60, 0x00);

            chrg_write_register(0x18, DESIGN_CAP);
            chrg_write_register(0x1E, ICHGTERM);
            chrg_write_register(0x3A, V_EMPTY);

            chrg_write_register(0xDB, 0x8000);

            while (chrg_read_register(0xDB) & 0x8000) vTaskDelay(pdMS_TO_TICKS(10));

            chrg_write_register(0xBA, hib_cfg);

            // Reset POR bit and verify
            write_verify_register(0x00, chrg_read_register(0x00) & 0xFFFD);
        }
    }

    /**
     * @brief Sets the BQ25040 charge mode to ISET Programmed. Initializes I2C bus and adds the MAX IC to the bus. Initializes the IC's registers after doing so.
     * 
     */
    void init()
    {
        // Configure EN/SET pin on BQ25040
        gpio_config_t chg_en_set_pin_config = {};
        chg_en_set_pin_config.pin_bit_mask = (1ULL << BoardConfig::CHG_EN_SET);
        chg_en_set_pin_config.mode = GPIO_MODE_OUTPUT;
        chg_en_set_pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        chg_en_set_pin_config.pull_up_en = GPIO_PULLUP_DISABLE;
        chg_en_set_pin_config.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&chg_en_set_pin_config));

        gpio_set_level(BoardConfig::CHG_EN_SET, 1); // Clear pin
        esp_rom_delay_us(2000);
        gpio_set_level(BoardConfig::CHG_EN_SET, 0); // Wait for deglitch to expire
        esp_rom_delay_us(200);
        gpio_set_level(BoardConfig::CHG_EN_SET, 1); // Send HIGH pulse for 400 us
        esp_rom_delay_us(400);
        gpio_set_level(BoardConfig::CHG_EN_SET, 0); // Hold LOW for 400 us. Enters ISET Programmed mode.
        esp_rom_delay_us(400);

        // Add MAX IC to I2C master bus
        i2c_device_config_t device_config = {};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = BoardConfig::I2C_MAX_ADDR;
        device_config.scl_speed_hz = 400'000;
        i2c_master_bus_handle_t master_bus_handle = nullptr;
        ESP_ERROR_CHECK(i2c_master_get_bus_handle(0, &master_bus_handle));
        ESP_ERROR_CHECK(i2c_master_bus_add_device(master_bus_handle, &device_config, &chrg_device_handle));

        init_chrg_ic();
    }

    /**
     * @brief Get battery details from the MAX IC
     * 
     * @param details struct to store the details in
     */
    void battery_details(BatteryDetails &details)
    {
        init_chrg_ic();

        uint16_t raw_soc = chrg_read_register(0x06);

        details.capacity_mah = chrg_read_register(0x05) * CAP_LSB;
        details.pct = (raw_soc >> 8) + (raw_soc & 0xFF)/256.0f;
        details.i_chrg_ma = (int16_t)chrg_read_register(0x0A) * CURRENT_LSB;
    }
}