#include "hal_analog.hpp"
#include "board_config.hpp"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace HalAnalog
{
    static adc_oneshot_unit_handle_t adc1_handle;
    static adc_cali_handle_t bat_cali_handle = NULL;

    void init()
    {
        gpio_config_t io_conf = {};
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = (1ULL << MUX_S0) | (1ULL << MUX_S1) | (1ULL << MUX_S2) | (1ULL << MUX_S3);
        gpio_config(&io_conf);

        adc_oneshot_unit_init_cfg_t adc1_conf = {};
        adc1_conf.unit_id = ADC_UNIT_1;
        adc1_conf.clk_src = ADC_RTC_CLK_SRC_DEFAULT;
        adc_oneshot_new_unit(&adc1_conf, &adc1_handle);

        adc_oneshot_chan_cfg_t mux_sig_config = {};
        mux_sig_config.atten = ADC_ATTEN_DB_12;
        mux_sig_config.bitwidth = ADC_BITWIDTH_12;
        adc_oneshot_config_channel(adc1_handle, MUX_SIG_CHANNEL, &mux_sig_config);

        adc_oneshot_chan_cfg_t battery_pct_config = {};
        battery_pct_config.atten = ADC_ATTEN_DB_12;
        battery_pct_config.bitwidth = ADC_BITWIDTH_12;
        adc_oneshot_config_channel(adc1_handle, BATTERY_PCT_CHANNEL, &battery_pct_config);

        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
            .default_vref = 0};
        adc_cali_create_scheme_line_fitting(&cali_config, &bat_cali_handle);
    }

    SensorArray getSensorValues()
    {
        SensorArray sensorValues = {0};

        for (int i = 0; i < NUM_SENSORS; i++)
        {
            gpio_set_level((gpio_num_t)MUX_S0, (i >> 0) & 1);
            gpio_set_level((gpio_num_t)MUX_S1, (i >> 1) & 1);
            gpio_set_level((gpio_num_t)MUX_S2, (i >> 2) & 1);
            gpio_set_level((gpio_num_t)MUX_S3, (i >> 3) & 1);

            esp_rom_delay_us(150);
            int dummyRead;
            adc_oneshot_read(adc1_handle, MUX_SIG_CHANNEL, &dummyRead);
            esp_rom_delay_us(50);

            int signalValue;
            // Alpha-trimmed mean to filter noise for flex sensors (index 5+)
            if (i >= 5)
            {
                const int sampleCount = 7;
                int samples[sampleCount];
                for (int s = 0; s < sampleCount; s++)
                {
                    adc_oneshot_read(adc1_handle, MUX_SIG_CHANNEL, &samples[s]);
                }

                // Sort samples from lowest to highest using insertion sort
                for (int j = 1; j < sampleCount; j++)
                {
                    int key = samples[j];
                    int k = j - 1;
                    while (k >= 0 && samples[k] > key)
                    {
                        samples[k + 1] = samples[k];
                        k--;
                    }
                    samples[k + 1] = key;
                }

                signalValue = (samples[2] + samples[3] + samples[4]) / 3;
            }
            else
            {
                adc_oneshot_read(adc1_handle, MUX_SIG_CHANNEL, &signalValue);
            }

            sensorValues[i] = signalValue;
        }

        return sensorValues;
    }

    int readBatteryMilliVolts()
    {
        int dummy_read;
        adc_oneshot_read(adc1_handle, BATTERY_PCT_CHANNEL, &dummy_read);
        esp_rom_delay_us(10);

        int raw_val = 0;
        adc_oneshot_read(adc1_handle, BATTERY_PCT_CHANNEL, &raw_val);

        int battery_mv = 0;
        if (bat_cali_handle != NULL)
        {
            adc_cali_raw_to_voltage(bat_cali_handle, raw_val, &battery_mv);
        }

        return battery_mv;
    }
}