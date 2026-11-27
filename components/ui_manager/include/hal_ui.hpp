#pragma once

#include <cstdint>

namespace HalUI
{
    static constexpr uint16_t SCREEN_WIDTH = 320;
    static constexpr uint16_t SCREEN_HEIGHT = 240;

    void init();

    void set_brightness_pct(uint8_t pct);
}