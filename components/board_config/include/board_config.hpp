#pragma once

#include "hal/adc_types.h"
#include "driver/gpio.h"

namespace BoardConfig
{
    // BNO086 IMU
    constexpr gpio_num_t IMU_H_INTN = GPIO_NUM_1;
    constexpr gpio_num_t IMU_NRST = GPIO_NUM_2;
    
    // CD74HC4067SM MUX
    constexpr adc_channel_t MUX_COM = ADC_CHANNEL_3;
    constexpr gpio_num_t MUX_S0 = GPIO_NUM_15;
    constexpr gpio_num_t MUX_S1 = GPIO_NUM_16;
    constexpr gpio_num_t MUX_S2 = GPIO_NUM_17;
    constexpr gpio_num_t MUX_S3 = GPIO_NUM_18;
    
    // TFT Display
    constexpr gpio_num_t TFT_DC = GPIO_NUM_5;
    constexpr gpio_num_t TFT_RST = GPIO_NUM_6;
    constexpr gpio_num_t TFT_BL = GPIO_NUM_7;
    constexpr gpio_num_t TFT_CS = GPIO_NUM_10;
    constexpr gpio_num_t TFT_CLK = GPIO_NUM_12;
    constexpr gpio_num_t TFT_MOSI = GPIO_NUM_11;
    
    // BQ24040 Charger
    constexpr gpio_num_t CHG_EN_SET = GPIO_NUM_13;
    
    // I2C
    constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
    constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
    constexpr uint8_t I2C_IMU_ADDR = 0x4B;
    constexpr uint8_t I2C_MAX_ADDR = (0x6C >> 1); // From datasheet - MAX17260SETD+
}