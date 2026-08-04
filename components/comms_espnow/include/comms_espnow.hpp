#pragma once
#include "glove_types.hpp"

namespace Comms
{
    // A0:F2:62:F2:2B:70 -- SuperMini ESP32-S3
    constexpr static uint8_t RECEIVER_ADDRESS[] = {0xA0, 0xF2, 0x62, 0xF2, 0x2B, 0x70};

    void init();

    void vCommsTask(void *pvParameters);

    void sleep();
}