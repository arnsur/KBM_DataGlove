#pragma once

#include <array>

#include "comms_protocol.hpp"
#include "gesture_engine_types.hpp"

namespace GestureEngine
{
    struct UIOutput
    {
        Cursor cursor;
        InputMode active_mode;
        InputMode pre_rest_mode;
        const char *mode_substate = "CURSOR READY";
    };

    struct EngineOutput
    {
        CommsProtocol::ReceiverMessage recv_msg;
        UIOutput ui_output;
    };
    
    void init();

    void queue_push_data(const GloveState &state);

    EngineOutput process();
}