#pragma once
#include <cstdint>
#include <array>
#include <atomic>
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

enum ConnectionStatus
{
    UNKNOWN,
    CONNECTED,
    DISCONNECTED,
    SEARCHING,
};

enum class InputMode
{
    IMODE_MOUSE,
    IMODE_UI,
    IMODE_KEYBOARD,
    IMODE_REST,
};

enum class MouseMode
{
    MMODE_MAIN,
    MMODE_ALT,
};

typedef struct __attribute__((packed)) LeftModeUpdateMessage
{
    InputMode newInputMode;
    MouseMode newMouseMode;
    bool wakeUpComms;
} LeftModeUpdateMessage;

typedef struct __attribute__((packed)) LeftTelemetryMessage
{
    std::array<int, 12> muxValues;
    ConnectionStatus l_to_recv_conn_status;
} LeftTelemetryMessage;

typedef struct CommsMessage
{
    uint8_t address[6];
    size_t payload_length;

    union
    {
        ReceiverMessage receiver_message;
        LeftModeUpdateMessage mode_update_message;
    } payload;
} CommsMessage;

typedef struct PowerManagerMessage
{
    bool shutdown_requested;
} PowerManagerMessage;

struct PeerConnection {
    std::atomic<bool> search_timed_out;
};

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
    InputMode inputMode;
    MouseMode mouseMode;
    bool hasClicked;
    int16_t uiCursorX;
    int16_t uiCursorY;
    int batteryPct;
    std::array<int, 12> rightMuxValues;
    std::array<int, 12> leftMuxValues;
    float yaw;
    float roll;
    CommsStatus comms_status;
};

struct EngineOutput
{
    ReceiverMessage message;

    bool modeChanged;
    InputMode newInputMode;
    MouseMode newMouseMode;
    bool uiClick;
    int16_t uiCursorX;
    int16_t uiCursorY;
    float yaw;
    float roll;
    int batteryPct;
    float batteryMilliVolts;
};
