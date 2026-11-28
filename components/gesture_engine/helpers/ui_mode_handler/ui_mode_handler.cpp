#include "ui_mode_handler.hpp"

namespace UIModeHandler
{
    static int16_t cursor_x = 160;
    static int16_t cursor_y = 120;
    static float ui_remainder_x = 0.0f;
    static float ui_remainder_y = 0.0f;

    GestureEngine::Cursor handle(const MouseProcessor::MouseDelta &d_mouse, const std::array<uint16_t, 10> sensors)
    {
        GestureEngine::Cursor ret = {};

        float exact_x = d_mouse.x * 0.2f + ui_remainder_x;
        float exact_y = d_mouse.y * 0.2f + ui_remainder_y;

        cursor_x += (int16_t)exact_x;
        cursor_y += (int16_t)exact_y;

        ui_remainder_x = exact_x - (int16_t)exact_x;
        ui_remainder_y = exact_y - (int16_t)exact_y;

        if (cursor_x < 0) cursor_x = 0;
        if (cursor_x > 319) cursor_x = 319;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_y > 239) cursor_y = 239;

        ret.x = cursor_x;
        ret.y = cursor_y;
        ret.is_pressed = (sensors[0] > 1000 /*|| sensors[5] > 123456*/);

        return ret;
    }
}