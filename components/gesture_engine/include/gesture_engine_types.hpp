#pragma once

#include <array>
#include <cstdint>

namespace GestureEngine
{
    struct GloveState
    {
        std::array<uint16_t, 10> sensors = {0};
        
        float q_x = 1.0f;
        float q_y = 0.0f;
        float q_z = 0.0f;
        float q_w = 0.0f;
    };

    enum class InputMode : uint8_t
    {
        MOUSE    = 0,
        UI       = 1,
        KEYBOARD = 2,
        REST     = 3,
    };

    struct Cursor
    {
        int16_t x = 160;
        int16_t y = 120;
        bool is_pressed = false;
    };
    
}