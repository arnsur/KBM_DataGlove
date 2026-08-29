#pragma once
#include <cstdint>

namespace GestureConfig
{

    enum Finger
    {
        INDEX,
        MIDDLE,
        RING,
        PINKY,
    };
    enum BendZone
    {
        NUM_ROW,
        TOP_ROW,
        HOME_ROW,
        BOTTOM_ROW,
    };
    enum RotZone
    {
        COL_MAIN,
        COL_ALT,
        COL_FAR_ALT,
    };

    struct FingerProfile
    {
        int bottomValMain;
        int homeValMain;

        int bottomValAlt;
        int homeValAlt;

        float altColVal;
        int farAltVal;

        int rowSensor;
        int colSensor;
    };

    struct FingerGridPos
    {
        BendZone row;
        RotZone column;
    };

    enum class KeyType {STANDARD, MODIFIER};
    
    struct Key {
        uint8_t keycode;
        KeyType type;
    };

    // --- Null Char ---
    constexpr Key KEY_NONE = {0x00, KeyType::STANDARD};

    // --- Keys ---
    constexpr Key KEY_6 = {0x23, KeyType::STANDARD};
    constexpr Key KEY_7 = {0x24, KeyType::STANDARD};
    constexpr Key KEY_8 = {0x25, KeyType::STANDARD};
    constexpr Key KEY_9 = {0x26, KeyType::STANDARD};
    constexpr Key KEY_0 = {0x27, KeyType::STANDARD};
    constexpr Key KEY_MINUS = {0x2D, KeyType::STANDARD};
    constexpr Key KEY_EQUALS = {0x2E, KeyType::STANDARD};
    constexpr Key KEY_BACKSPACE = {0x2A, KeyType::STANDARD};

    constexpr Key KEY_Y = {0x1C, KeyType::STANDARD};
    constexpr Key KEY_U = {0x18, KeyType::STANDARD};
    constexpr Key KEY_I = {0x0C, KeyType::STANDARD};
    constexpr Key KEY_O = {0x12, KeyType::STANDARD};
    constexpr Key KEY_P = {0x13, KeyType::STANDARD};
    constexpr Key KEY_LEFT_BRACKET = {0x2F, KeyType::STANDARD};
    constexpr Key KEY_RIGHT_BRACKET = {0x30, KeyType::STANDARD};
    constexpr Key KEY_BACKSLASH = {0x31, KeyType::STANDARD};

    constexpr Key KEY_H = {0x0B, KeyType::STANDARD};
    constexpr Key KEY_J = {0x0D, KeyType::STANDARD};
    constexpr Key KEY_K = {0x0E, KeyType::STANDARD};
    constexpr Key KEY_L = {0x0F, KeyType::STANDARD};
    constexpr Key KEY_SEMICOLON = {0x33, KeyType::STANDARD};
    constexpr Key KEY_APOSTROPHE = {0x34, KeyType::STANDARD};
    constexpr Key KEY_ENTER = {0x28, KeyType::STANDARD};
    
    constexpr Key KEY_N = {0x11, KeyType::STANDARD};
    constexpr Key KEY_M = {0x10, KeyType::STANDARD};
    constexpr Key KEY_COMMA = {0x36, KeyType::STANDARD};
    constexpr Key KEY_PERIOD = {0x37, KeyType::STANDARD};
    constexpr Key KEY_FWSLASH = {0x38, KeyType::STANDARD};
    constexpr Key KEY_RSHIFT = {0x20, KeyType::MODIFIER};

    constexpr Key KEY_RALT = {0x40, KeyType::MODIFIER};
    constexpr Key KEY_RCTRL = {0x10, KeyType::MODIFIER};

    constexpr Key KEY_MAP[4][4][3] = {
        //--------------------------INDEX--------------------------
        {
            // MAIN | ALT
            {KEY_7, KEY_6, KEY_NONE},  // NUM
            {KEY_U, KEY_Y, KEY_NONE},  // TOP
            {KEY_J, KEY_H, KEY_NONE},  // HOME
            {KEY_M, KEY_N, KEY_NONE},  // BOTTOM
        },

        //-------------------------MIDDLE--------------------------
        {
            // MAIN | ALT (NONE)
            {KEY_8, KEY_NONE, KEY_NONE},     // NUM
            {KEY_I, KEY_NONE, KEY_NONE},     // TOP
            {KEY_K, KEY_NONE, KEY_NONE},     // HOME
            {KEY_COMMA, KEY_NONE, KEY_NONE}, // BOTTOM
        },

        //---------------------------RING--------------------------
        {
            // MAIN | ALT 
            {KEY_9, KEY_BACKSPACE, KEY_NONE},      // NUM
            {KEY_O, KEY_BACKSLASH, KEY_NONE},      // TOP
            {KEY_L, KEY_ENTER, KEY_NONE},      // HOME
            {KEY_PERIOD, KEY_NONE, KEY_NONE}, // BOTTOM
        },

        //--------------------------PINKY--------------------------
        {
            // MAIN | ALT
            {KEY_0, KEY_MINUS, KEY_EQUALS},               // NUM
            {KEY_P, KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET}, // TOP
            {KEY_SEMICOLON, KEY_APOSTROPHE, KEY_APOSTROPHE},   // HOME
            {KEY_FWSLASH, KEY_RSHIFT, KEY_RSHIFT},        // BOTTOM
        }
    };

    // --- Finger Profiles ---
    constexpr FingerProfile indexProfile = {
        .bottomValMain = 2610,
        .homeValMain = 2445,

        .bottomValAlt = 2570,
        .homeValAlt = 2425,

        .altColVal = 5.5f,
        .farAltVal = -1,

        .rowSensor = 7,
        .colSensor = 6,
    };

    constexpr FingerProfile middleProfile = {
        .bottomValMain = 2575,
        .homeValMain = 2420,

        .bottomValAlt = 2530,
        .homeValAlt = 2360,

        .altColVal = 0.0f,
        .farAltVal = -1,

        .rowSensor = 8,
        .colSensor = -1,
    };

    constexpr FingerProfile ringProfile = {
        .bottomValMain = 2655,
        .homeValMain = 2510,

        .bottomValAlt = 2655,
        .homeValAlt = 2510,

        .altColVal = -4.0f,
        .farAltVal = -1,

        .rowSensor = 9,
        .colSensor = -1,
    };

    constexpr FingerProfile pinkyProfile = {
        .bottomValMain = 2650,
        .homeValMain = 2485,

        .bottomValAlt = 2580,
        .homeValAlt = 2445,

        .altColVal = -4.0f,
        .farAltVal = 1475,

        .rowSensor = 10,
        .colSensor = 11,
    };

    float fn_pitch_threshold = 20.0;
    float alt_pitch_threshold = -20.0;
    float ctrl_roll_threshold = -25.0;

    // --- Mouse & UI Multipliers ---
    constexpr float MOUSE_SENSITIVITY = 100.0f;
    constexpr float GUI_MOUSE_SENS_MULT = 0.2f;
    constexpr float MOUSE_SMOOTHING_ALPHA = 0.4f;

    // --- Flex Sensor Thresholds ---
    constexpr int CLUTCH_START_THRESHOLD = 3000;
    constexpr int CLUTCH_EXIT_THRESHOLD = 2800;

    constexpr int SWITCH_START_THRESHOLD = 2650;
    constexpr int SWITCH_EXIT_THRESHOLD = 2500;

    // --- Timers & Delays (in milliseconds) ---
    constexpr uint32_t MIN_SCROLL_INTERVAL_MS = 50;
    constexpr uint32_t LONG_PRESS_DELAY_MS = 550;
    constexpr uint32_t CLICK_FREEZE_MS = 150;

    // --- Battery Config ---
    constexpr int BATTERY_MILLIVOLTS_DIVIDER_MAX = 2020;
    constexpr int BATTERY_MILLIVOLTS_DIVIDER_MIN = 1600;
    constexpr float BATTERY_SMOOTHING_ALPHA = 0.05;
}