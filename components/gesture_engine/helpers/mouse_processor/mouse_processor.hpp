#pragma once

#include "helpers/imu_processor/imu_types.hpp"

namespace MouseProcessor
{
    struct MouseDelta
    {
        float x;
        float y;
    };

    MouseDelta mouse_delta(const IMUProcessor::IMUEuler &euler);
}