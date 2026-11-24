#pragma once

#include "comms_protocol.hpp"

namespace Comms
{
    void init();

    void queue_push_data(CommsProtocol::ReceiverMessage &msg);

    void send_data();
}