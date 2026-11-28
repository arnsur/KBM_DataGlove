#pragma once

#include "helpers/imu_processor/imu_types.hpp"
#include "mouse_types.hpp"

namespace MouseProcessor
{
    MouseDelta mouse_delta(const IMUProcessor::IMUEuler &euler);
}