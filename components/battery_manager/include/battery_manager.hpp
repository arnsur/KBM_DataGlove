#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"

#include "battery_manager_types.hpp"

namespace BatteryManager
{
    enum class ChargeMode : int8_t
    {
        CHRG_DISABLED = -1,
        USB500,
        ISET_PROGRAMMED,
        USB100,
    };

    void init();

    void select_charge_mode(ChargeMode mode);

    bool battery_details(BatteryDetails &details);

    void sleep();
}