#pragma once
#include "hal/adc_types.h"
#include "driver/gpio.h"

constexpr gpio_num_t MUX_S0 = GPIO_NUM_14;
constexpr gpio_num_t MUX_S1 = GPIO_NUM_27;
constexpr gpio_num_t MUX_S2 = GPIO_NUM_26;
constexpr gpio_num_t MUX_S3 = GPIO_NUM_25;
constexpr gpio_num_t MUX_SIG = GPIO_NUM_34;
constexpr adc_channel_t MUX_SIG_CHANNEL = ADC_CHANNEL_6;

constexpr gpio_num_t I2C_SDA = GPIO_NUM_21;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_22;
constexpr gpio_num_t IMU_INT = GPIO_NUM_4;
constexpr gpio_num_t IMU_RST = GPIO_NUM_23;
constexpr int IMU_ADDRESS = 0x4B;

constexpr gpio_num_t BATTERY_PCT_PIN = GPIO_NUM_35;
constexpr adc_channel_t BATTERY_PCT_CHANNEL = ADC_CHANNEL_7;

constexpr gpio_num_t TFT_DIN = GPIO_NUM_32;
constexpr gpio_num_t TFT_CLK = GPIO_NUM_18;
constexpr gpio_num_t TFT_CS = GPIO_NUM_19;
constexpr gpio_num_t TFT_DC = GPIO_NUM_33;
constexpr gpio_num_t TFT_RST = GPIO_NUM_16;
constexpr gpio_num_t TFT_BL = GPIO_NUM_17;