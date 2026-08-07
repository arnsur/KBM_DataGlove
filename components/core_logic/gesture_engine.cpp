#include "gesture_engine.hpp"
#include "gesture_config.hpp"
#include "esp_timer.h"
#include <cstring>
#include <cmath>

using namespace GestureConfig;

namespace GestureEngine
{
    // --- Mode State Memory ---
    static InputMode currentInputMode = InputMode::IMODE_UI;
    static MouseMode currentMouseMode = MouseMode::MMODE_MAIN;
    static InputMode lastInputModeBeforeSwitch = InputMode::IMODE_MOUSE; // Remembers mode before long-pressing to Rest

    // --- Clutch & Switch State Memory ---
    static bool lastClutchState = false;        // Detects the exact moment the clutch is pressed/released
    static bool clutchLongPressHandled = false; // Prevents cycling modes when releasing a long press
    static uint32_t clutchPressTime = 0;        // Tracks how long the clutch has been held

    static bool lastSwitchState = false; // Detects the exact moment the switch is pressed/released

    // --- IMU Delta Tracking ---
    static float lastYaw = 0.0f;
    static float lastRoll = 0.0f;
    static float smoothedYawDeg = 0.0f;
    static float smoothedRollDeg = 0.0f;
    static float keyboardStartYawDeg = 0.0f; // Anchors the keyboard grid relative to where mode 2 started

    // --- Mouse Math & Smoothing ---
    static float remainderX = 0.0f; // Stores fractional X movement across loops
    static float remainderY = 0.0f; // Stores fractional Y movement across loops

    // --- UI Cursor Tracking ---
    // (If the Gesture Engine is processing mode 1 cursor coordinates before sending them)
    static float guiRemainderX = 0.0f;
    static float guiRemainderY = 0.0f;
    static int16_t cursor_x = 160; // Initial center X
    static int16_t cursor_y = 120; // Initial center Y

    // --- Click & Scroll Freeze Timers ---
    static uint8_t previousButtons = 0;      // Bitmask to detect if a click state changed
    static uint32_t lastStateChangeTime = 0; // Starts the 150ms click-freeze timer
    static uint32_t lastScrollTime = 0;      // Rate limits the scroll ticks

    // --- Battery Constants ---
    constexpr int BATTERY_MILLIVOLTS_DIVIDER_MAX = 2020;
    constexpr int BATTERY_MILLIVOLTS_DIVIDER_MIN = 1600;
    constexpr float BATTERY_SMOOTHING_ALPHA = 0.05;
    float batterySmoothedMilliVolts = 0;

    static FingerGridPos getFingerRowCol(const FingerProfile &profile, const GloveState &gloveState)
    {
        FingerGridPos fingerPos = {TOP_ROW, COL_MAIN};

        int rowVal = gloveState.muxValues[profile.rowSensor];
        int colVal = -1;

        fingerPos.column = COL_MAIN;
        if (profile.colSensor >= 0)
        {
            colVal = profile.altColVal;
            if (smoothedYawDeg - keyboardStartYawDeg >= colVal)
            {
                fingerPos.column = COL_ALT;
            }
        }

        if (fingerPos.column == COL_ALT)
        {
            if (rowVal > profile.bottomValAlt)
            {
                fingerPos.row = BOTTOM_ROW;
            }
            else if (rowVal > profile.homeValAlt)
            {
                fingerPos.row = HOME_ROW;
            }
            else
            {
                fingerPos.row = TOP_ROW;
            }
        }
        else
        {
            if (rowVal > profile.bottomValMain)
            {
                fingerPos.row = BOTTOM_ROW;
            }
            else if (rowVal > profile.homeValMain)
            {
                fingerPos.row = HOME_ROW;
            }
            else
            {
                fingerPos.row = TOP_ROW;
            }
        }

        return fingerPos;
    }

    EngineOutput processData(const GloveState &currentGloveState)
    {
        uint32_t currentTimeMillis = esp_timer_get_time() / 1000;

        // ------------ CONVERT QUAT TO YAW-PITCH-ROLL EULER ------------
        float w = currentGloveState.quatReal;
        float x = currentGloveState.quatX;
        float y = currentGloveState.quatY;
        float z = currentGloveState.quatZ;

        // Yaw
        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        float currentYawDeg = std::atan2(siny_cosp, cosy_cosp) * (180.0f / M_PI);

        // Pitch calculations if needed in the future
        // float sinp = std::sqrt(1.0f + 2.0f * (w * y - x * z));
        // float cosp = std::sqrt(1.0f - 2.0f * (w * y - x * z));
        // float currentPitchDeg = (2.0f * std::atan2(sinp, cosp) - M_PI / 2.0f) * (180.0f / M_PI);

        // Roll
        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        float currentRollDeg = std::atan2(sinr_cosp, cosr_cosp) * (180.0f / M_PI);

        EngineOutput out;
        out.message = {};
        out.message.hand_id = 1;
        out.modeChanged = false;
        out.newInputMode = currentInputMode;
        out.newMouseMode = currentMouseMode;
        out.uiClick = false;
        out.message.modifier_bitmask = 0;

        int batteryDividerMilliVoltsRaw = currentGloveState.batteryDividerMilliVolts;
        if (batterySmoothedMilliVolts == 0) batterySmoothedMilliVolts = batteryDividerMilliVoltsRaw; 
        batterySmoothedMilliVolts = (BATTERY_SMOOTHING_ALPHA * batteryDividerMilliVoltsRaw) + ((1 - BATTERY_SMOOTHING_ALPHA) * batterySmoothedMilliVolts);

        // pct = (v-LOW)/(HIGH - LOW)
        out.batteryPct = (int)(100.0f * (((float)(batterySmoothedMilliVolts - BATTERY_MILLIVOLTS_DIVIDER_MIN))/(BATTERY_MILLIVOLTS_DIVIDER_MAX - BATTERY_MILLIVOLTS_DIVIDER_MIN)));
        out.batteryMilliVolts = batterySmoothedMilliVolts * 2;

        if (lastYaw == 0)
        {
            smoothedYawDeg = lastYaw;
            lastYaw = currentYawDeg;
        }

        if (lastRoll == 0)
        {
            smoothedRollDeg = lastRoll;
            lastRoll = currentRollDeg;
        }

        smoothedYawDeg = (MOUSE_SMOOTHING_ALPHA * currentYawDeg) + ((1.0 - MOUSE_SMOOTHING_ALPHA) * smoothedYawDeg);
        smoothedRollDeg = (MOUSE_SMOOTHING_ALPHA * currentRollDeg) + ((1.0 - MOUSE_SMOOTHING_ALPHA) * smoothedRollDeg);

        out.yaw = smoothedYawDeg;
        out.roll = smoothedRollDeg;

        float yawDisplacement = smoothedYawDeg - lastYaw;
        lastYaw = smoothedYawDeg;
        float rollDisplacement = smoothedRollDeg - lastRoll;
        lastRoll = smoothedRollDeg;

        float mouseX = -yawDisplacement * MOUSE_SENSITIVITY;
        mouseX += remainderX;
        float mouseY = rollDisplacement * MOUSE_SENSITIVITY;
        mouseY += remainderY;

        remainderX = mouseX - (int)mouseX;
        remainderY = mouseY - (int)mouseY;

        bool clutchBent = false;
        if (currentGloveState.muxValues[5] >= CLUTCH_START_THRESHOLD)
        {
            clutchBent = true;
        }
        else if (currentGloveState.muxValues[5] < CLUTCH_EXIT_THRESHOLD)
        {
            clutchBent = false;
        }

        bool clutchActsAsModeSwitch = !(currentInputMode == InputMode::IMODE_MOUSE && currentMouseMode == MouseMode::MMODE_ALT);
        if (clutchActsAsModeSwitch)
        {
            // Detect the exact moment the clutch is bent
            if (clutchBent && !lastClutchState)
            {
                clutchPressTime = currentTimeMillis;
                clutchLongPressHandled = false;
            }

            // LONG PRESS: Toggle Rest Mode (Mode 3)
            if (clutchBent && !clutchLongPressHandled)
            {
                if (currentTimeMillis - clutchPressTime > LONG_PRESS_DELAY_MS)
                {
                    if (currentInputMode == InputMode::IMODE_REST)
                    {
                        currentInputMode = lastInputModeBeforeSwitch;
                    }
                    else
                    {
                        lastInputModeBeforeSwitch = currentInputMode;
                        currentInputMode = InputMode::IMODE_REST;
                    }
                    clutchLongPressHandled = true;
                    out.modeChanged = true;
                    out.newInputMode = currentInputMode;
                }
            }

            // SHORT PRESS: Cycle Active Modes (0 -> 1 -> 2 -> 0)
            if (!clutchBent && lastClutchState)
            {
                if (!clutchLongPressHandled && currentInputMode != InputMode::IMODE_REST)
                {
                    currentInputMode = static_cast<InputMode>((static_cast<int>(currentInputMode) + 1) % 3);
                    if (currentInputMode == InputMode::IMODE_KEYBOARD)
                    {
                        keyboardStartYawDeg = smoothedYawDeg;
                    }
                    currentMouseMode = MouseMode::MMODE_MAIN; // Reset mouse mode if the input mode is cycled
                    out.modeChanged = true;
                    out.newInputMode = currentInputMode;
                }
            }
        }
        lastClutchState = clutchBent;

        bool switchBent = false;
        if (currentGloveState.muxValues[10] >= SWITCH_START_THRESHOLD)
        {
            switchBent = true;
        }
        else if (currentGloveState.muxValues[10] < SWITCH_EXIT_THRESHOLD)
        {
            switchBent = false;
        }

        if (currentInputMode == InputMode::IMODE_MOUSE && switchBent && lastSwitchState == false)
        {
            if (currentMouseMode == MouseMode::MMODE_MAIN)
            {
                currentMouseMode = MouseMode::MMODE_ALT;
            }
            else if (currentMouseMode == MouseMode::MMODE_ALT)
            {
                currentMouseMode = MouseMode::MMODE_MAIN;
            }

            out.modeChanged = true;
            out.newMouseMode = currentMouseMode;
        }
        lastSwitchState = switchBent;

        bool is_using_mouse = (currentInputMode == InputMode::IMODE_MOUSE || currentInputMode == InputMode::IMODE_UI);

        bool lmbClicked = false;
        bool rmbClicked = false;
        bool mmbClicked = false;
        bool mb5Clicked = false;
        bool mb4Clicked = false;
        bool scrollUp = false;
        bool scrollDown = (is_using_mouse && currentMouseMode == MouseMode::MMODE_ALT && (currentGloveState.muxValues[0] > 2100 || currentGloveState.muxValues[5] > 2800));


        if (is_using_mouse && (currentGloveState.muxValues[1] > 2250 || currentGloveState.muxValues[7] > 2700))
        {
            if (currentMouseMode == MouseMode::MMODE_MAIN)
            {
                lmbClicked = true;
                mb4Clicked = false;
            }
            else if (currentMouseMode == MouseMode::MMODE_ALT)
            {
                lmbClicked = false;
                mb4Clicked = true;
            }
        }

        if (is_using_mouse && (currentGloveState.muxValues[3] > 2150 || currentGloveState.muxValues[9] > 2600))
        {
            if (currentMouseMode == MouseMode::MMODE_MAIN)
            {
                rmbClicked = true;
                scrollUp = false;
            }
            else if (currentMouseMode == MouseMode::MMODE_ALT)
            {
                rmbClicked = false;
                scrollUp = true;
            }
        }

        if (is_using_mouse && (currentGloveState.muxValues[2] > 2150 || currentGloveState.muxValues[8] > 2600))
        {
            if (currentMouseMode == MouseMode::MMODE_MAIN)
            {
                mmbClicked = true;
                mb5Clicked = false;
            }
            else if (currentMouseMode == MouseMode::MMODE_ALT)
            {
                mmbClicked = false;
                mb5Clicked = true;
            }
        }

        int scrollTicks = 0;

        if (currentTimeMillis - lastScrollTime >= MIN_SCROLL_INTERVAL_MS)
        {
            lastScrollTime = currentTimeMillis;
            if (scrollUp)
                scrollTicks++;
            if (scrollDown)
                scrollTicks--;
        }

        uint8_t currentButtons = 0;
        if (lmbClicked)
            currentButtons |= (1 << 0);
        if (rmbClicked)
            currentButtons |= (1 << 1);
        if (mmbClicked)
            currentButtons |= (1 << 2);

        if (currentButtons != previousButtons)
        {
            lastStateChangeTime = currentTimeMillis;
        }
        previousButtons = currentButtons;

        if (currentTimeMillis - lastStateChangeTime < CLICK_FREEZE_MS || !is_using_mouse)
        {
            out.message.deltaMouseX = 0;
            out.message.deltaMouseY = 0;
        }
        else
        {
            out.message.deltaMouseX = (int8_t)mouseX;
            out.message.deltaMouseY = (int8_t)mouseY;
        }

        out.message.leftClick = lmbClicked;
        out.message.rightClick = rmbClicked;
        out.message.middleClick = mmbClicked;
        out.message.scrollTicks = scrollTicks;
        out.message.mouseFwd = mb5Clicked;
        out.message.mouseBack = mb4Clicked;

        if (currentInputMode == InputMode::IMODE_UI)
        {
            float exactGuiX = (mouseX * GUI_MOUSE_SENS_MULT) + guiRemainderX;
            float exactGuiY = (mouseY * GUI_MOUSE_SENS_MULT) + guiRemainderY;

            cursor_x += (int16_t)exactGuiX;
            cursor_y += (int16_t)exactGuiY;
            guiRemainderX = exactGuiX - (int16_t)exactGuiX;
            guiRemainderY = exactGuiY - (int16_t)exactGuiY;

            out.uiClick = lmbClicked;
        }

        switch (currentInputMode)
        {
        case InputMode::IMODE_MOUSE:
            memset(out.message.keysPressed, GestureConfig::KEY_NONE.keycode, sizeof(out.message.keysPressed));
            out.message.modifier_bitmask = 0;
            break;
        case InputMode::IMODE_UI:
            out.message.deltaMouseX = 0;
            out.message.deltaMouseY = 0;
            out.message.scrollTicks = 0;
            out.message.leftClick = false;
            out.message.rightClick = false;
            out.message.middleClick = false;
            out.message.mouseFwd = false;
            out.message.mouseBack = false;
            memset(out.message.keysPressed, GestureConfig::KEY_NONE.keycode, sizeof(out.message.keysPressed));
            out.message.modifier_bitmask = 0;
            break;

        case InputMode::IMODE_KEYBOARD:
            out.message.deltaMouseX = 0;
            out.message.deltaMouseY = 0;
            out.message.scrollTicks = 0;
            out.message.leftClick = false;
            out.message.rightClick = false;
            out.message.middleClick = false;
            out.message.mouseFwd = false;
            out.message.mouseBack = false;

            // TODO: TURN THIS INTO A FOR LOOP
            if (currentGloveState.muxValues[4] > 500) {
                Key key_pressed = KEY_MAP[PINKY][getFingerRowCol(pinkyProfile, currentGloveState).row][getFingerRowCol(pinkyProfile, currentGloveState).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    out.message.keysPressed[3] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    out.message.modifier_bitmask |= key_pressed.keycode;
                }
            }
            
            if (currentGloveState.muxValues[3] > 500) {
                Key key_pressed = KEY_MAP[RING][getFingerRowCol(ringProfile, currentGloveState).row][getFingerRowCol(ringProfile, currentGloveState).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    out.message.keysPressed[2] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    out.message.modifier_bitmask |= key_pressed.keycode;
                }
            }

            if (currentGloveState.muxValues[2] > 500) {
                Key key_pressed = KEY_MAP[MIDDLE][getFingerRowCol(middleProfile, currentGloveState).row][getFingerRowCol(middleProfile, currentGloveState).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    out.message.keysPressed[1] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    out.message.modifier_bitmask |= key_pressed.keycode;
                }
            }

            if (currentGloveState.muxValues[1] > 500) {
                Key key_pressed = KEY_MAP[INDEX][getFingerRowCol(indexProfile, currentGloveState).row][getFingerRowCol(indexProfile, currentGloveState).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    out.message.keysPressed[0] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    out.message.modifier_bitmask |= key_pressed.keycode;
                }
            }

            if (currentGloveState.muxValues[0] > 500) {
                keyboardStartYawDeg = smoothedYawDeg;
            }

            break;

        case InputMode::IMODE_REST:
            out.message.deltaMouseX = 0;
            out.message.deltaMouseY = 0;
            out.message.scrollTicks = 0;
            out.message.leftClick = false;
            out.message.rightClick = false;
            out.message.middleClick = false;
            out.message.mouseFwd = false;
            out.message.mouseBack = false;
            memset(out.message.keysPressed, GestureConfig::KEY_NONE.keycode, sizeof(out.message.keysPressed));
            out.message.modifier_bitmask = 0;
            break;
        }

        // Clamp UI cursor to UI screen edges (320x240)
        if (cursor_x < 0)
            cursor_x = 0;
        if (cursor_x > 319)
            cursor_x = 319;
        if (cursor_y < 0)
            cursor_y = 0;
        if (cursor_y > 239)
            cursor_y = 239;

        out.uiCursorX = cursor_x;
        out.uiCursorY = cursor_y;

        return out;
    }
}
