#pragma once

#include <array>
#include <cstdint>

namespace HalAnalog
{
    using SensorArray = std::array<uint16_t, 10>;

    void init();

    SensorArray sensor_values();
}