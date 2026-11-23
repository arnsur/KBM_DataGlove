#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

namespace BatteryManager{
    struct BatteryDetails
    {
        float pct;
        int16_t i_chrg_ma;
        uint16_t capacity_mah;
    };

    void init();

    void battery_details(BatteryDetails &details);
}