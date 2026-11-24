#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "hal_analog.hpp"
#include "board_config.hpp"

namespace HalAnalog
{
    static adc_oneshot_unit_handle_t adc_handle = nullptr;
    static adc_cali_handle_t cali_handle = nullptr;

    /**
     * @brief Fills out necessary configs for mV calibrated ADC oneshot readings from the CD74HC4067
     * 
     */
    void init()
    {
        // Configure oneshot unit
        adc_oneshot_unit_init_cfg_t adc_init_cfg = {};
        adc_init_cfg.unit_id = ADC_UNIT_1;
        adc_init_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;
        adc_init_cfg.clk_src = ADC_RTC_CLK_SRC_DEFAULT;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_cfg, &adc_handle));

        // Configure Common IO ADC channel
        adc_oneshot_chan_cfg_t channel_cfg = {};
        channel_cfg.atten = ADC_ATTEN_DB_12;
        channel_cfg.bitwidth = ADC_BITWIDTH_12;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BoardConfig::MUX_COM, &channel_cfg));

        // Configure Select/Address pins
        gpio_config_t select_pin_cfg = {};
        select_pin_cfg.pin_bit_mask = (1ULL << BoardConfig::MUX_S0) | (1ULL << BoardConfig::MUX_S1) | (1ULL << BoardConfig::MUX_S2) | (1ULL << BoardConfig::MUX_S3);
        select_pin_cfg.mode = GPIO_MODE_OUTPUT;
        select_pin_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        select_pin_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        select_pin_cfg.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&select_pin_cfg));

        // Configure ADC calibration curve fitting scheme
        adc_cali_curve_fitting_config_t curve_cfg = {};
        curve_cfg.unit_id = ADC_UNIT_1;
        curve_cfg.chan = BoardConfig::MUX_COM;
        curve_cfg.atten = ADC_ATTEN_DB_12;
        curve_cfg.bitwidth = ADC_BITWIDTH_12;
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&curve_cfg, &cali_handle));
    }


    /**
     * @brief Returns a reading of each analog sensor connected to the CD74HC4067
     * 
     * @return SensorArray
     */
    SensorArray sensor_values()
    {
        SensorArray ret = {0};

        for (int i = 0; i < 10; i++)
        {
            gpio_set_level(BoardConfig::MUX_S0, (i >> 0) & 1);
            gpio_set_level(BoardConfig::MUX_S1, (i >> 1) & 1);
            gpio_set_level(BoardConfig::MUX_S2, (i >> 2) & 1);
            gpio_set_level(BoardConfig::MUX_S3, (i >> 3) & 1);

            int raw_read = 0;
            ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BoardConfig::MUX_COM, &raw_read));
            int cali_read = 0;
            ESP_ERROR_CHECK(adc_cali_chan_raw_to_voltage(cali_handle, BoardConfig::MUX_COM, raw_read, &cali_read));

            ret[i] = (uint16_t)cali_read;
        }

        return ret;
    }
}