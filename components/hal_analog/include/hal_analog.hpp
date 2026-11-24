#pragma once

#include <array>
#include <cstdint>

namespace HalAnalog
{
    void init();

    std::array<uint16_t, 10> sensor_values();
}