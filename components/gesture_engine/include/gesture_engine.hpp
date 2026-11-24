#pragma once

#include <array>

#include "comms_protocol.hpp"

namespace GestureEngine
{
    struct EngineOutput
    {
        CommsProtocol::ReceiverMessage recv_msg;
        // TODO: Add UI stuff
    };

    struct GloveState
    {
        std::array<uint16_t, 10> sensors;
        float q_x;
        float q_y;
        float q_z;
        float q_w;
    };
    
    void init();

    void queue_push_data(const GloveState &state);

    EngineOutput process();
}