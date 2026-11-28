#include <cstdint>

#include "mouse_mode_handler.hpp"

namespace MouseModeHandler
{
    static float remainder_x = 0.0f;
    static float remainder_y = 0.0f;

    void handle(const MouseProcessor::MouseDelta &d_mouse, GestureEngine::EngineOutput &out)
    {
        float exact_dx = d_mouse.x + remainder_x;
        float exact_dy = d_mouse.y + remainder_y;

        remainder_x = exact_dx - (int)exact_dx;
        remainder_y = exact_dy - (int)exact_dy;

        out.recv_msg.d_mouse_x = (int8_t)exact_dx;
        out.recv_msg.d_mouse_y = (int8_t)exact_dy;
    }
}