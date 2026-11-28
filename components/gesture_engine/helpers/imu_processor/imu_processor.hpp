#pragma once

#include "imu_types.hpp"

namespace IMUProcessor
{
    IMUEuler process(float x, float y, float z, float w);
}