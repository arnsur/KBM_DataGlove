#pragma once
#include <cstdint>

namespace GestureConfig
{

    enum Finger { INDEX, MIDDLE, RING, PINKY };
    enum BendZone { NUM_ROW, TOP_ROW, HOME_ROW, BOTTOM_ROW };
    enum RotZone { COL_MAIN, COL_ALT };

    struct FingerProfile {
        int bottomValMain;
        int homeValMain;

        int bottomValAlt;
        int homeValAlt;

        float altColVal;

        int rowSensor;
        int colSensor;
    };
    
    struct FingerGridPos {
        BendZone row;
        RotZone column;
    };

    constexpr char KEY_MAP[4][4][2] = {
    //--------------------------INDEX--------------------------
    {
        //MAIN | ALT
        { '7', '6'}, // NUM
        { 'u', 'y'}, // TOP
        { 'j', 'h'}, // HOME
        { 'm', 'n'} // BOTTOM
    },

    //-------------------------MIDDLE--------------------------
    {
        //MAIN | ALT (NONE)
        { '8', '\0'}, // NUM
        { 'i', '\0'}, // TOP
        { 'k', '\0'}, // HOME
        { ',', '\0'} // BOTTOM
    },

    //---------------------------RING--------------------------
    {
        //MAIN | ALT (NONE)
        { '9', '\0'}, // NUM
        { 'o', '\0'}, // TOP
        { 'l', '\0'}, // HOME
        { '.', '\0'} // BOTTOM
    },

    //--------------------------PINKY--------------------------
    {
        //MAIN | ALT
        { '0', '\0'}, // NUM
        { 'p', '\0'}, // TOP
        { ';', '\0'}, // HOME
        { '/', '\0'} // BOTTOM
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
        .colSensor = 6
    };

    constexpr FingerProfile middleProfile = {
        .bottomValMain = 2530,
        .homeValMain = 2400,

        .bottomValAlt = 2530,
        .homeValAlt = 2360,
        
        .altColVal = 9999.0f,

        .rowSensor = 8,
        .colSensor = -1
    };

    constexpr FingerProfile ringProfile = {
        .bottomValMain = 2610,
        .homeValMain = 2455,

        .bottomValAlt = 2610,
        .homeValAlt = 2455,
        
        .altColVal = 9999.0f,

        .rowSensor = 9,
        .colSensor = -1
    };

    constexpr FingerProfile pinkyProfile = {
        .bottomValMain = 2650,
        .homeValMain = 2500,

        .bottomValAlt = 2610,
        .homeValAlt = 2455,
        
        .altColVal = 9999.0f,

        .rowSensor = 10,
        .colSensor = -1
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