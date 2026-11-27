#pragma once

#include <array>
#include <cstdint>

namespace GestureEngine
{
    struct GloveState
    {
        std::array<uint16_t, 10> sensors;
        
        float q_x;
        float q_y;
        float q_z;
        float q_w;
    };

    struct Cursor
    {
        int16_t x = 160;
        int16_t y = 120;
        bool is_pressed = false;
    };
}