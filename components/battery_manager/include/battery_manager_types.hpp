#pragma once

#include <cstdint>

namespace BatteryManager
{
    struct BatteryDetails
    {
        float pct = 0.0f;
        int16_t i_chrg_ma = 0;
        uint16_t capacity_mah = 0;
    };
}