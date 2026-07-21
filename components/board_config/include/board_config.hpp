#pragma once
#include "hal/adc_types.h"

constexpr int MUX_S0 = 14;
constexpr int MUX_S1 = 27;
constexpr int MUX_S2 = 26;
constexpr int MUX_S3 = 25;
constexpr int MUX_SIG = 34;
constexpr adc_channel_t MUX_SIG_CHANNEL = ADC_CHANNEL_6;

constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;
constexpr int IMU_INT = 4;
constexpr int IMU_RST = 23;
constexpr int IMU_ADDRESS = 0x4B;

constexpr int BATTERY_PCT_PIN = 35;
constexpr adc_channel_t BATTERY_PCT_CHANNEL = ADC_CHANNEL_7;

constexpr int TFT_DIN = 32;
constexpr int TFT_CLK = 18;
constexpr int TFT_CS = 19;
constexpr int TFT_DC = 33;
constexpr int TFT_RST = 16;
constexpr int TFT_BL = 17;