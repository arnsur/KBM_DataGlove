#pragma once

#include "gesture_engine_types.hpp"

namespace InputStateManager
{
    struct InputState
    {
        GestureEngine::InputMode active_mode;
        GestureEngine::InputMode pre_rest_mode;
    };

    void update_state(InputState &old_state);
}