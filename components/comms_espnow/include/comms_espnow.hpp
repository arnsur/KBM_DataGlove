#pragma once
#include "glove_types.hpp"

namespace Comms
{
    // A0:F2:62:F2:2B:70 -- SuperMini ESP32-S3
    constexpr static uint8_t RECEIVER_ADDRESS[] = {0xA0, 0xF2, 0x62, 0xF2, 0x2B, 0x70};

    // E0:8C:FE:E5:FE:04 -- ESP32
    constexpr static uint8_t LEFT_GLOVE_ADDRESS[] = {0xE0, 0x8C, 0xFE, 0xE5, 0xFE, 0x04};

    extern CommsStatus comms_status;

    extern PeerConnection recv_peer;

    extern PeerConnection left_peer;
    
    extern std::array<int, 12> left_mux_values;
    
    extern bool left_telemetry_available;

    extern portMUX_TYPE left_telemetry_mux;

    constexpr int MAX_SEARCH_TIME_S = 10;

    void init();

    void vCommsTask(void *pvParameters);

    void sleep();

    void wake_up_comms(uint8_t input_mode, uint8_t mouse_mode);
}