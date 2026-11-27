#pragma once

#include <cstdint>

namespace BatteryManager
{
    struct BatteryDetails
    {
        float pct;
        int16_t i_chrg_ma;
        uint16_t capacity_mah;
    };
}