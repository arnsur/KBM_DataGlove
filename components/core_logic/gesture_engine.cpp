#include "gesture_engine.hpp"
#include "gesture_config.hpp"
#include "esp_timer.h"
#include <cstring>
#include <cmath>

using namespace GestureConfig;

namespace GestureEngine
{
    constexpr ReceiverMessage DEFAULT_RECEIVER_MSG = {
        .hand_id = 1,
        .deltaMouseX = 0,
        .deltaMouseY = 0,
        .scrollTicks = 0,
        .leftClick = false,
        .rightClick = false,
        .middleClick = false,
        .mouseFwd = false,
        .mouseBack = false,
        .keysPressed = {GestureConfig::KEY_NONE.keycode},
        .modifier_bitmask = 0,
    };
    
    namespace
    {
        // Anchors the keyboard grid relative to where mode 2 started
        float keyboardStartYawDeg = 0.0f;
        float keyboardStartPitchDeg = 0.0f;
        float keyboardStartRollDeg = 0.0f;

        struct BatteryData
        {
            int battery_pct;
            float battery_millivolts;
        };
        
        struct KinematicData
        {
            float smoothed_yaw_deg;
            float smoothed_pitch_deg;
            float smoothed_roll_deg;
        };
    
        struct InputState
        {
            InputMode input_mode;
            MouseMode mouse_mode;
            bool mode_changed;
        };
        
        struct MouseDelta
        {
            float x;
            float y;
        };
        
        struct MouseState
        {
            bool lmb_clicked;
            bool rmb_clicked;
            bool mmb_clicked;
            bool mb5_clicked;
            bool mb4_clicked;
            int8_t scroll_ticks;
            MouseDelta mouse_delta;
        };
        
        struct UIInteraction
        {
            bool ui_click;
            int16_t cursor_x_pos;
            int16_t cursor_y_pos;
        };

        uint32_t get_time_millis()
        {
            return esp_timer_get_time() / 1000;
        }
        
        FingerGridPos getFingerRowCol(const FingerProfile &profile, std::array<int, 12> sensor_values, const KinematicData &imu_kinematics)
        {
            FingerGridPos fingerPos = {TOP_ROW, COL_MAIN};
            
            int rowVal = sensor_values[profile.rowSensor];
            float colVal = -1;
            
            fingerPos.column = COL_MAIN;
            if (profile.colSensor >= 0)
            {
                colVal = profile.altColVal;

                float diff = imu_kinematics.smoothed_yaw_deg - keyboardStartYawDeg;

                if ((colVal > 0.0f && diff >= colVal) || (colVal < 0.0f && diff <= colVal))
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

        // Mode handlers only modify fields they interact with in the ReceiverMessage
        // Anything that they don't handle is already zeroed
        void handle_mouse_mode(ReceiverMessage &engine_recv_message, const MouseState &current_mouse_state)
        {
            engine_recv_message.deltaMouseX = (int8_t)current_mouse_state.mouse_delta.x;
            engine_recv_message.deltaMouseY = (int8_t)current_mouse_state.mouse_delta.y;
            engine_recv_message.scrollTicks = current_mouse_state.scroll_ticks;
            engine_recv_message.leftClick = current_mouse_state.lmb_clicked;
            engine_recv_message.rightClick = current_mouse_state.rmb_clicked;
            engine_recv_message.middleClick = current_mouse_state.mmb_clicked;
            engine_recv_message.mouseFwd = current_mouse_state.mb5_clicked;
            engine_recv_message.mouseBack = current_mouse_state.mb4_clicked;
        }
    
        void handle_keyboard_mode(ReceiverMessage &engine_recv_message, std::array<int, 12> sensor_values, const KinematicData &kinematic_data)
        {
            // TODO: TURN THIS INTO A FOR LOOP
            if (sensor_values[4] > 500) {
                Key key_pressed = KEY_MAP[PINKY][getFingerRowCol(pinkyProfile, sensor_values, kinematic_data).row][getFingerRowCol(pinkyProfile, sensor_values, kinematic_data).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    engine_recv_message.keysPressed[3] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    engine_recv_message.modifier_bitmask |= key_pressed.keycode;
                }
            }
            
            if (sensor_values[3] > 500) {
                Key key_pressed = KEY_MAP[RING][getFingerRowCol(ringProfile, sensor_values, kinematic_data).row][getFingerRowCol(ringProfile, sensor_values, kinematic_data).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    engine_recv_message.keysPressed[2] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    engine_recv_message.modifier_bitmask |= key_pressed.keycode;
                }
            }
    
            if (sensor_values[2] > 500) {
                Key key_pressed = KEY_MAP[MIDDLE][getFingerRowCol(middleProfile, sensor_values, kinematic_data).row][getFingerRowCol(middleProfile, sensor_values, kinematic_data).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    engine_recv_message.keysPressed[1] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    engine_recv_message.modifier_bitmask |= key_pressed.keycode;
                }
            }
    
            if (sensor_values[1] > 500) {
                Key key_pressed = KEY_MAP[INDEX][getFingerRowCol(indexProfile, sensor_values, kinematic_data).row][getFingerRowCol(indexProfile, sensor_values, kinematic_data).column];
                if (key_pressed.type == KeyType::STANDARD)
                {
                    engine_recv_message.keysPressed[0] = key_pressed.keycode;
                }
                else if (key_pressed.type == KeyType::MODIFIER)
                {
                    engine_recv_message.modifier_bitmask |= key_pressed.keycode;
                }
            }
    
            if (sensor_values[0] > 500) {
                keyboardStartYawDeg = kinematic_data.smoothed_yaw_deg;
                keyboardStartPitchDeg = kinematic_data.smoothed_pitch_deg;
                keyboardStartRollDeg = kinematic_data.smoothed_roll_deg;
            }            

            if (kinematic_data.smoothed_pitch_deg - keyboardStartPitchDeg >= fn_pitch_threshold)
            {
                printf("fn\n");
            } else if (kinematic_data.smoothed_pitch_deg - keyboardStartPitchDeg <= alt_pitch_threshold)
            {
                engine_recv_message.modifier_bitmask |= KEY_RALT.keycode;
            }

            if (kinematic_data.smoothed_roll_deg - keyboardStartRollDeg <= ctrl_roll_threshold)
            {
                engine_recv_message.modifier_bitmask |= KEY_RCTRL.keycode;
            }
        }

        class EMAFilter
        {
            public:
                EMAFilter(float alpha) : alpha_{alpha} {}

                float update(float new_value)
                {
                    if (!is_init_)
                    {
                        is_init_ = true;
                        value_ = new_value;
                    }
                    else
                    {
                        value_ = (alpha_ * new_value) + ((1.0f - alpha_) * value_);
                    }

                    return value_;
                }

            private:
                float value_ = 0.0f;
                float alpha_;
                bool is_init_ = false;
        };

        class IMUProcessor
        {
            public:
                KinematicData process_data(const Quaternion &quat)
                {
                    KinematicData out = {};

                    // ------------ CONVERT QUAT TO YAW-PITCH-ROLL EULER ------------
                    float x = quat.x;
                    float y = quat.y;
                    float z = quat.z;
                    float w = quat.real;

                    // Yaw
                    float siny_cosp = 2.0f * (w * z + x * y);
                    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
                    float currentYawDeg = std::atan2(siny_cosp, cosy_cosp) * (180.0f / M_PI);

                    // Pitch
                    float sinp = std::sqrt(1.0f + 2.0f * (w * y - x * z));
                    float cosp = std::sqrt(1.0f - 2.0f * (w * y - x * z));
                    float currentPitchDeg = (2.0f * std::atan2(sinp, cosp) - M_PI / 2.0f) * (180.0f / M_PI);

                    // Roll
                    float sinr_cosp = 2.0f * (w * x + y * z);
                    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
                    float currentRollDeg = std::atan2(sinr_cosp, cosr_cosp) * (180.0f / M_PI);

                    yaw_ = yaw_filter_.update(currentYawDeg);
                    pitch_ = pitch_filter_.update(currentPitchDeg);
                    roll_ = roll_filter_.update(currentRollDeg);

                    out.smoothed_yaw_deg = yaw_;
                    out.smoothed_pitch_deg = pitch_;
                    out.smoothed_roll_deg = roll_;

                    return out;
                }

            private:
                float yaw_;
                float pitch_;
                float roll_;

                EMAFilter yaw_filter_{MOUSE_SMOOTHING_ALPHA};
                EMAFilter pitch_filter_{MOUSE_SMOOTHING_ALPHA};
                EMAFilter roll_filter_{MOUSE_SMOOTHING_ALPHA};
        };

        class BatteryProcessor
        {
            public:
                BatteryData process_data(int raw_battery_divider_millivolts)
                {
                    BatteryData out = {};

                    battery_smoothed_millivolts_ = divider_filter_.update(raw_battery_divider_millivolts);

                    // pct = (v-LOW)/(HIGH - LOW)
                    out.battery_pct = (int)(100.0f * (((float)(battery_smoothed_millivolts_ - BATTERY_MILLIVOLTS_DIVIDER_MIN))/(BATTERY_MILLIVOLTS_DIVIDER_MAX - BATTERY_MILLIVOLTS_DIVIDER_MIN)));
                    out.battery_millivolts = battery_smoothed_millivolts_ * 2;

                    return out;
                }
            private:
                float battery_smoothed_millivolts_;

                EMAFilter divider_filter_{BATTERY_SMOOTHING_ALPHA};
        };

        class InputStateManager
        {
            public:
                InputState state() const
                {
                    return state_;
                }

                void update_state(std::array<int, 12> sensor_values, const KinematicData &imu_kinematics)
                {
                    uint32_t current_time_ms = get_time_millis();

                    state_.mode_changed = false;

                    bool clutchBent = false;
                    if (sensor_values[5] >= CLUTCH_START_THRESHOLD)
                    {
                        clutchBent = true;
                    }
                    else if (sensor_values[5] < CLUTCH_EXIT_THRESHOLD)
                    {
                        clutchBent = false;
                    }

                    bool clutchActsAsModeSwitch = !(state_.input_mode == InputMode::IMODE_MOUSE && state_.mouse_mode == MouseMode::MMODE_ALT);
                    if (clutchActsAsModeSwitch)
                    {
                        // Detect the exact moment the clutch is bent
                        if (clutchBent && !last_clutch_state_)
                        {
                            clutch_press_time_ = current_time_ms;
                            clutch_long_press_handled_ = false;
                        }

                        // LONG PRESS: Toggle Rest Mode (Mode 3)
                        if (clutchBent && !clutch_long_press_handled_)
                        {
                            if (current_time_ms - clutch_press_time_ > LONG_PRESS_DELAY_MS)
                            {
                                if (state_.input_mode == InputMode::IMODE_REST)
                                {
                                    state_.input_mode = input_mode_before_switch_;
                                }
                                else
                                {
                                    input_mode_before_switch_ = state_.input_mode;
                                    state_.input_mode = InputMode::IMODE_REST;
                                }
                                clutch_long_press_handled_ = true;
                                state_.mode_changed = true;
                            }
                        }

                        // SHORT PRESS: Cycle Active Modes (0 -> 1 -> 2 -> 0)
                        if (!clutchBent && last_clutch_state_)
                        {
                            if (!clutch_long_press_handled_ && state_.input_mode != InputMode::IMODE_REST)
                            {
                                state_.input_mode = static_cast<InputMode>((static_cast<int>(state_.input_mode) + 1) % 3);
                                if (state_.input_mode == InputMode::IMODE_KEYBOARD)
                                {
                                    keyboardStartYawDeg = imu_kinematics.smoothed_yaw_deg;
                                    keyboardStartPitchDeg = imu_kinematics.smoothed_pitch_deg;
                                    keyboardStartRollDeg = imu_kinematics.smoothed_roll_deg;
                                }
                                state_.mouse_mode = MouseMode::MMODE_MAIN; // Reset mouse mode if the input mode is cycled
                                state_.mode_changed = true;
                            }
                        }
                    }
                    last_clutch_state_ = clutchBent;

                    bool switchBent = false;
                    if (sensor_values[10] >= SWITCH_START_THRESHOLD)
                    {
                        switchBent = true;
                    }
                    else if (sensor_values[10] < SWITCH_EXIT_THRESHOLD)
                    {
                        switchBent = false;
                    }

                    if (state_.input_mode == InputMode::IMODE_MOUSE && switchBent && last_switch_state_ == false)
                    {
                        if (state_.mouse_mode == MouseMode::MMODE_MAIN)
                        {
                            state_.mouse_mode = MouseMode::MMODE_ALT;
                        }
                        else if (state_.mouse_mode == MouseMode::MMODE_ALT)
                        {
                            state_.mouse_mode = MouseMode::MMODE_MAIN;
                        }

                        state_.mode_changed = true;
                    }
                    last_switch_state_ = switchBent;
                }

            private:
                InputState state_ {
                    .input_mode = InputMode::IMODE_UI,
                    .mouse_mode = MouseMode::MMODE_MAIN,
                    .mode_changed = false,
                };

                InputMode input_mode_before_switch_ = state_.input_mode;

                bool last_clutch_state_ = false;
                bool clutch_long_press_handled_ = false;
                uint32_t clutch_press_time_ = 0;
                bool last_switch_state_ = false;
        };

        class MouseProcessor
        {
            public:
                MouseState process_mouse(const KinematicData &imu_kinematics, MouseMode current_mouse_mode, std::array<int, 12> sensor_values)
                {
                    MouseState out = {};

                    MouseDelta current_delta = get_mouse_delta(imu_kinematics);
                    uint32_t current_time_ms = get_time_millis();

                    bool lmbClicked = false;
                    bool rmbClicked = false;
                    bool mmbClicked = false;
                    bool mb5Clicked = false;
                    bool mb4Clicked = false;
                    bool scrollUp = false;
                    bool scrollDown = (current_mouse_mode == MouseMode::MMODE_ALT && (sensor_values[0] > 2100 || sensor_values[5] > 2800));

                    if (sensor_values[1] > 2250 || sensor_values[7] > 2700)
                    {
                        if (current_mouse_mode == MouseMode::MMODE_MAIN)
                        {
                            lmbClicked = true;
                            mb4Clicked = false;
                        }
                        else if (current_mouse_mode == MouseMode::MMODE_ALT)
                        {
                            lmbClicked = false;
                            mb4Clicked = true;
                        }
                    }

                    if (sensor_values[3] > 2150 || sensor_values[9] > 2600)
                    {
                        if (current_mouse_mode == MouseMode::MMODE_MAIN)
                        {
                            rmbClicked = true;
                            scrollUp = false;
                        }
                        else if (current_mouse_mode == MouseMode::MMODE_ALT)
                        {
                            rmbClicked = false;
                            scrollUp = true;
                        }
                    }

                    if (sensor_values[2] > 2150 || sensor_values[8] > 2600)
                    {
                        if (current_mouse_mode == MouseMode::MMODE_MAIN)
                        {
                            mmbClicked = true;
                            mb5Clicked = false;
                        }
                        else if (current_mouse_mode == MouseMode::MMODE_ALT)
                        {
                            mmbClicked = false;
                            mb5Clicked = true;
                        }
                    }

                    int scrollTicks = 0;

                    if (current_time_ms - last_scroll_time >= MIN_SCROLL_INTERVAL_MS)
                    {
                        last_scroll_time = current_time_ms;
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

                    if (currentButtons != previous_buttons)
                    {
                        last_state_change_time = current_time_ms;
                    }
                    previous_buttons = currentButtons;

                    if (current_time_ms - last_state_change_time < CLICK_FREEZE_MS)
                    {
                        out.mouse_delta.x = 0;
                        out.mouse_delta.y = 0;
                    }
                    else
                    {
                        out.mouse_delta.x = current_delta.x;
                        out.mouse_delta.y = current_delta.y;
                    }

                    out.lmb_clicked = lmbClicked;
                    out.rmb_clicked = rmbClicked;
                    out.mmb_clicked = mmbClicked;
                    out.mb5_clicked = mb5Clicked;
                    out.mb4_clicked = mb4Clicked;
                    out.scroll_ticks = scrollTicks;

                    return out;
                }

            private:
                bool is_init_ = false;

                float lastYaw_ = 0.0f;
                float lastRoll_ = 0.0f;
                float remainderX_ = 0.0f;
                float remainderY_ = 0.0f;
                uint8_t previous_buttons = 0;
                uint32_t last_state_change_time = 0;
                uint32_t last_scroll_time = 0; 

                MouseDelta get_mouse_delta(const KinematicData &imu_kinematics)
                {
                    MouseDelta out = {};

                    if (!is_init_) {
                        lastYaw_ = imu_kinematics.smoothed_yaw_deg;
                        lastRoll_ = imu_kinematics.smoothed_roll_deg;
                        is_init_ = true;
                    }

                    float yawDisplacement = imu_kinematics.smoothed_yaw_deg - lastYaw_;
                    lastYaw_ = imu_kinematics.smoothed_yaw_deg;
                    float rollDisplacement = imu_kinematics.smoothed_roll_deg - lastRoll_;
                    lastRoll_ = imu_kinematics.smoothed_roll_deg;

                    float mouseX = -yawDisplacement * MOUSE_SENSITIVITY;
                    mouseX += remainderX_;
                    float mouseY = rollDisplacement * MOUSE_SENSITIVITY;
                    mouseY += remainderY_;

                    remainderX_ = mouseX - (int)mouseX;
                    remainderY_ = mouseY - (int)mouseY;

                    out.x = mouseX;
                    out.y = mouseY;

                    return out;
                }
        };

        class UIProcessor {
            private:
                float guiRemainderX = 0.0f;
                float guiRemainderY = 0.0f;
                int16_t cursor_x = 160; // Start center screen
                int16_t cursor_y = 120;

            public:
                UIInteraction process_ui(const MouseState &current_mouse_state, bool is_ui_mode) {
                    UIInteraction out = {};

                    if (is_ui_mode) {
                        float exactGuiX = (current_mouse_state.mouse_delta.x * GUI_MOUSE_SENS_MULT) + guiRemainderX;
                        float exactGuiY = (current_mouse_state.mouse_delta.y * GUI_MOUSE_SENS_MULT) + guiRemainderY;

                        cursor_x += (int16_t)exactGuiX;
                        cursor_y += (int16_t)exactGuiY;
                        guiRemainderX = exactGuiX - (int16_t)exactGuiX;
                        guiRemainderY = exactGuiY - (int16_t)exactGuiY;

                        // Clamp to screen bounds (320 x 240)
                        if (cursor_x < 0) cursor_x = 0;
                        if (cursor_x > 319) cursor_x = 319;
                        if (cursor_y < 0) cursor_y = 0;
                        if (cursor_y > 239) cursor_y = 239;

                        out.ui_click = current_mouse_state.lmb_clicked;
                    } else {
                        out.ui_click = false;
                    }

                    out.cursor_x_pos = cursor_x;
                    out.cursor_y_pos = cursor_y;

                    return out;
                }
            };

    }


    EngineOutput processData(const GloveState &currentGloveState)
    {
        EngineOutput out;

        static IMUProcessor imu_processor;
        static BatteryProcessor battery_processor;
        static InputStateManager input_state_manager;
        static MouseProcessor mouse_handler;
        static UIProcessor ui_processor;

        BatteryData battery_data = battery_processor.process_data(currentGloveState.batteryDividerMilliVolts);
        KinematicData kinematic_data = imu_processor.process_data(currentGloveState.imu_quaternion);
        
        input_state_manager.update_state(currentGloveState.muxValues, kinematic_data);
        InputState current_input_state = input_state_manager.state();

        InputMode current_input_mode = current_input_state.input_mode;
        MouseMode current_mouse_mode = current_input_state.mouse_mode;

        MouseState mouse_state = mouse_handler.process_mouse(kinematic_data, current_mouse_mode, currentGloveState.muxValues);

        ReceiverMessage output_message = DEFAULT_RECEIVER_MSG;

        bool is_ui_mode = (current_input_mode == InputMode::IMODE_UI);
        UIInteraction ui_interaction = ui_processor.process_ui(mouse_state, is_ui_mode);

        out.uiCursorX = ui_interaction.cursor_x_pos;
        out.uiCursorY = ui_interaction.cursor_y_pos;
        out.uiClick = ui_interaction.ui_click;

        switch (current_input_mode)
        {
        case InputMode::IMODE_MOUSE:
            handle_mouse_mode(output_message, mouse_state);
            break;

        case InputMode::IMODE_UI:
            break;

        case InputMode::IMODE_KEYBOARD:
            handle_keyboard_mode(output_message, currentGloveState.muxValues, kinematic_data);
            break;

        case InputMode::IMODE_REST:
            break;
        }

        out.message = output_message;
        out.modeChanged = current_input_state.mode_changed;
        out.newInputMode = current_input_mode;
        out.newMouseMode = current_mouse_mode;
        out.batteryPct = battery_data.battery_pct;
        out.batteryMilliVolts = battery_data.battery_millivolts;
        out.yaw = kinematic_data.smoothed_yaw_deg;
        out.pitch = kinematic_data.smoothed_pitch_deg;
        out.roll = kinematic_data.smoothed_roll_deg;

        return out;
    }
}
