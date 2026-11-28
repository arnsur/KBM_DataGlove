#include <cmath>

#include "imu_processor.hpp"

namespace IMUProcessor
{
    IMUEuler process(float x, float y, float z, float w)
    {
        IMUEuler ret = {};

        // Yaw
        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        ret.yaw = std::atan2(siny_cosp, cosy_cosp) * (180.0f / M_PI);

        // Pitch
        float sinp = std::sqrt(1.0f + 2.0f * (w * y - x * z));
        float cosp = std::sqrt(1.0f - 2.0f * (w * y - x * z));
        ret.pitch = (2.0f * std::atan2(sinp, cosp) - M_PI / 2.0f) * (180.0f / M_PI);

        // Roll
        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        ret.roll = std::atan2(sinr_cosp, cosr_cosp) * (180.0f / M_PI);

        return ret;
    }
}