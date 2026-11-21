#pragma once

#include <cstdint>

namespace BatteryManager{
    struct BatteryDetails
    {
        uint16_t capacity_mah;
        float pct;
    };

    void init();

    void battery_details(BatteryDetails &details);
}