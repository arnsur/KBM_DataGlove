#pragma once

#include <array>

#include "comms_protocol.hpp"
#include "gesture_engine_types.hpp"

namespace GestureEngine
{
    struct EngineOutput
    {
        CommsProtocol::ReceiverMessage recv_msg;
        Cursor cursor;
    };
    
    void init();

    void queue_push_data(const GloveState &state);

    EngineOutput process();
}