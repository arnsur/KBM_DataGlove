#pragma once
#include <array>
#include <cstdint>

namespace HalAnalog
{
    constexpr int NUM_SENSORS = 12;

    using SensorArray = std::array<int, NUM_SENSORS>;

    void init();

    SensorArray getSensorValues();

    int readBatteryMilliVolts();
}