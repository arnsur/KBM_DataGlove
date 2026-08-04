#pragma once
#include <cstdint>
#include <array>

typedef struct ReceiverMessage
{
    uint8_t hand_id;
    int8_t mouseX;
    int8_t mouseY;
    int8_t scrollTicks;
    bool leftClick;
    bool rightClick;
    bool middleClick;
    bool mouseFwd;
    bool mouseBack;
    char keysPressed[6];
} ReceiverMessage;

typedef struct ModeUpdateMessage
{
    int currentInputMode;
    bool wakeUpComms;
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

struct GloveState
{
    float quatReal;
    float quatX;
    float quatY;
    float quatZ;
    std::array<int, 12> muxValues;
    int batteryDividerMilliVolts;
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
    int batteryPct;
    float batteryMilliVolts;
};
