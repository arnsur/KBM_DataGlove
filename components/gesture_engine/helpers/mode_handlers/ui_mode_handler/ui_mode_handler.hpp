#pragma once

#include <array>
#include <cstdint>

#include "helpers/mouse_processor/mouse_types.hpp"
#include "gesture_engine.hpp"

namespace UIModeHandler
{
    GestureEngine::Cursor handle(const MouseProcessor::MouseDelta &d_mouse, const std::array<uint16_t, 10> sensors, GestureEngine::EngineOutput &out);
}