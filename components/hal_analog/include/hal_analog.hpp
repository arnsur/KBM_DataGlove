#pragma once

#include <array>
#include <cstdint>

namespace HalAnalog
{
    /**
     * @brief Used to store the value of each analog sensor calibrated in mV
     *  
     */
    using SensorArray = std::array<uint16_t, 10>;

    void init();

    SensorArray sensor_values();
}