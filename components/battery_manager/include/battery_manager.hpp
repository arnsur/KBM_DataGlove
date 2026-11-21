#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

namespace BatteryManager{
    struct BatteryDetails
    {
        uint16_t capacity_mah;
        float pct;
    };

    void init(i2c_master_bus_handle_t master_bus_handle);

    void battery_details(BatteryDetails &details);
}