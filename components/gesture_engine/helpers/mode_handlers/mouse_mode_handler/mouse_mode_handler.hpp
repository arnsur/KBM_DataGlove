#pragma once

#include "helpers/mouse_processor/mouse_types.hpp"
#include "gesture_engine.hpp"

namespace MouseModeHandler
{
    void handle(const MouseProcessor::MouseDelta &d_mouse, GestureEngine::EngineOutput &out);
}