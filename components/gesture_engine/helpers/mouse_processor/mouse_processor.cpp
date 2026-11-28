#include <cstdint>

#include "gesture_config.hpp"
#include "mouse_processor.hpp"

namespace MouseProcessor
{
    static float last_yaw = 0.0f;
    static float last_roll = 0.0f;

    MouseDelta mouse_delta(const IMUProcessor::IMUEuler &euler)
    {
        MouseDelta ret = {};

        float yaw_displacement = euler.yaw - last_yaw;
        float roll_displacement = euler.roll - last_roll;
        last_yaw = euler.yaw;
        last_roll = euler.roll;

        float d_mouse_x = -yaw_displacement * MOUSE_SENSITIVITY;
        float d_mouse_y = -roll_displacement * MOUSE_SENSITIVITY;

        ret.x = d_mouse_x;
        ret.y = d_mouse_y;

        return ret;
    }
}