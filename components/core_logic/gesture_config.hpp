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
    };

    struct FingerProfile
    {
        int bottomValMain;
        int homeValMain;

        int bottomValAlt;
        int homeValAlt;

        float altColVal;

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

    constexpr Key KEY_Y = {0x1C, KeyType::STANDARD};
    constexpr Key KEY_U = {0x18, KeyType::STANDARD};
    constexpr Key KEY_I = {0x0C, KeyType::STANDARD};
    constexpr Key KEY_O = {0x12, KeyType::STANDARD};
    constexpr Key KEY_P = {0x13, KeyType::STANDARD};

    constexpr Key KEY_H = {0x0B, KeyType::STANDARD};
    constexpr Key KEY_J = {0x0D, KeyType::STANDARD};
    constexpr Key KEY_K = {0x0E, KeyType::STANDARD};
    constexpr Key KEY_L = {0x0F, KeyType::STANDARD};
    constexpr Key KEY_SEMICOLON = {0x33, KeyType::STANDARD};
    
    constexpr Key KEY_N = {0x11, KeyType::STANDARD};
    constexpr Key KEY_M = {0x10, KeyType::STANDARD};
    constexpr Key KEY_COMMA= {0x36, KeyType::STANDARD};
    constexpr Key KEY_PERIOD = {0x37, KeyType::STANDARD};
    constexpr Key KEY_FWSLASH = {0x38, KeyType::STANDARD};

    // --- Modifiers (TO BE ADDED) ----

    constexpr Key KEY_MAP[4][4][2] = {
        //--------------------------INDEX--------------------------
        {
            // MAIN | ALT
            {KEY_7, KEY_6}, // NUM
            {KEY_U, KEY_Y}, // TOP
            {KEY_J, KEY_H}, // HOME
            {KEY_M, KEY_N}  // BOTTOM
        },

        //-------------------------MIDDLE--------------------------
        {
            // MAIN | ALT (NONE)
            {KEY_8, KEY_NONE},    // NUM
            {KEY_I, KEY_NONE},    // TOP
            {KEY_K, KEY_NONE},    // HOME
            {KEY_COMMA, KEY_NONE} // BOTTOM
        },

        //---------------------------RING--------------------------
        {
            // MAIN | ALT (NONE)
            {KEY_9, KEY_NONE},     // NUM
            {KEY_O, KEY_NONE},     // TOP
            {KEY_L, KEY_NONE},     // HOME
            {KEY_PERIOD, KEY_NONE} // BOTTOM
        },

        //--------------------------PINKY--------------------------
        {
            // MAIN | ALT (IN DEVELOPMENT)
            {KEY_0, KEY_NONE},         // NUM
            {KEY_P, KEY_NONE},         // TOP
            {KEY_SEMICOLON, KEY_NONE}, // HOME
            {KEY_FWSLASH, KEY_NONE}      // BOTTOM
        }
    };

    // --- Finger Profiles ---
    constexpr FingerProfile indexProfile = {
        .bottomValMain = 2615,
        .homeValMain = 2455,

        .bottomValAlt = 2575,
        .homeValAlt = 2425,

        .altColVal = 5.5f,

        .rowSensor = 7,
        .colSensor = 6,
    };

    constexpr FingerProfile middleProfile = {
        .bottomValMain = 2530,
        .homeValMain = 2400,

        .bottomValAlt = 2530,
        .homeValAlt = 2360,

        .altColVal = 9999.0f,

        .rowSensor = 8,
        .colSensor = -1,
    };

    constexpr FingerProfile ringProfile = {
        .bottomValMain = 2610,
        .homeValMain = 2455,

        .bottomValAlt = 2610,
        .homeValAlt = 2455,

        .altColVal = 9999.0f,

        .rowSensor = 9,
        .colSensor = -1,
    };

    constexpr FingerProfile pinkyProfile = {
        .bottomValMain = 2650,
        .homeValMain = 2500,

        .bottomValAlt = 2610,
        .homeValAlt = 2455,

        .altColVal = 9999.0f,

        .rowSensor = 10,
        .colSensor = -1,
    };

    // --- Mouse & UI Multipliers ---
    constexpr float MOUSE_SENSITIVITY = 60.0f;
    constexpr float GUI_MOUSE_SENS_MULT = 0.33f;
    constexpr float MOUSE_SMOOTHING_ALPHA = 0.4f;

    // --- Flex Sensor Thresholds ---
    constexpr int CLUTCH_START_THRESHOLD = 2900;
    constexpr int CLUTCH_EXIT_THRESHOLD = 2800;

    constexpr int SWITCH_START_THRESHOLD = 2650;
    constexpr int SWITCH_EXIT_THRESHOLD = 2500;

    // --- Timers & Delays (in milliseconds) ---
    constexpr uint32_t MIN_SCROLL_INTERVAL_MS = 50;
    constexpr uint32_t LONG_PRESS_DELAY_MS = 550;
    constexpr uint32_t CLICK_FREEZE_MS = 150;

}