#pragma once
#include <cstdint>
#include <array>
#include "esp_timer.h"

typedef struct __attribute__((packed)) ReceiverMessage
{
    uint8_t hand_id;
    int8_t deltaMouseX;
    int8_t deltaMouseY;
    int8_t scrollTicks;
    bool leftClick;
    bool rightClick;
    bool middleClick;
    bool mouseFwd;
    bool mouseBack;
    uint8_t keysPressed[6];
    uint8_t modifier_bitmask;
} ReceiverMessage;

typedef struct ModeUpdateMessage
{
    int newInputMode;
    int newMouseMode;
} ModeUpdateMessage;

typedef struct CommsMessage
{
    uint8_t address[6];
    size_t payload_length;

    union
    {
        ReceiverMessage receiver_message;
        ModeUpdateMessage mode_update_message;
    } payload;
} CommsMessage;

typedef struct PowerManagerMessage
{
    bool shutdown_requested;
} PowerManagerMessage;

enum ConnectionStatus
{
    CONNECTED,
    DISCONNECTED,
    SEARCHING,
    UNKNOWN
};

struct PeerConnection {
    bool search_timed_out;
    int64_t first_fail_time;
};

// TODO: MOVE THIS TO A GLOBAL CONFIG FILE
inline int64_t curr_time_ms()
{
    return esp_timer_get_time() / 1000;
}

struct GloveState
{
    float quatReal;
    float quatX;
    float quatY;
    float quatZ;
    std::array<int, 12> muxValues;
    int batteryDividerMilliVolts;
};

struct CommsStatus
{
    ConnectionStatus r_to_recv_conn_status;
    ConnectionStatus r_to_l_conn_status;
    ConnectionStatus l_to_recv_conn_status;
};

struct UIState
{
    bool modeChanged;
    int inputMode;
    int mouseMode;
    bool hasClicked;
    int16_t uiCursorX;
    int16_t uiCursorY;
    int batteryPct;
    std::array<int, 12> muxValues;
    float yaw;
    float roll;
    CommsStatus comms_status;
};

struct EngineOutput
{
    ReceiverMessage message;

    bool modeChanged;
    int newInputMode;
    int newMouseMode;
    bool uiClick;
    int16_t uiCursorX;
    int16_t uiCursorY;
    float yaw;
    float roll;
    int batteryPct;
    float batteryMilliVolts;
};
