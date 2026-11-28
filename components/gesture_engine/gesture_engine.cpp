#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gesture_engine.hpp"
#include "helpers/imu_processor/imu_processor.hpp"
#include "helpers/mouse_processor/mouse_processor.hpp"
#include "helpers/input_state_manager/input_state_manager.hpp"
#include "helpers/mode_handlers/ui_mode_handler/ui_mode_handler.hpp"
#include "helpers/mode_handlers/mouse_mode_handler/mouse_mode_handler.hpp"

namespace GestureEngine
{
    static QueueHandle_t gesture_state_queue = nullptr;

    static Cursor last_cursor;

    InputStateManager::InputState prev_state = {GestureEngine::InputMode::UI, GestureEngine::InputMode::UI};

    void init()
    {
        gesture_state_queue = xQueueCreate(1, sizeof(GloveState));
    }

    void queue_push_data(const GloveState &state)
    {
        xQueueOverwrite(gesture_state_queue, &state);
    }

    EngineOutput process()
    {
        EngineOutput ret = {};

        GloveState state;

        if (xQueueReceive(gesture_state_queue, &state, portMAX_DELAY) == pdTRUE)
        {
            IMUProcessor::IMUEuler euler = IMUProcessor::process(state.q_x, state.q_y, state.q_z, state.q_w);

            MouseProcessor::MouseDelta d_mouse = MouseProcessor::mouse_delta(euler);

            InputStateManager::update_state(prev_state);
            ret.ui_output.active_mode = prev_state.active_mode;
            ret.ui_output.pre_rest_mode = prev_state.pre_rest_mode;

            switch (ret.ui_output.active_mode)
            {
            case InputMode::MOUSE:
                MouseModeHandler::handle(d_mouse, ret);
                ret.ui_output.cursor = last_cursor;
                break;
            case InputMode::UI:
                last_cursor = UIModeHandler::handle(d_mouse, state.sensors, ret);
                break;
            case InputMode::KEYBOARD:
                ret.ui_output.cursor = last_cursor;
                break;
            case InputMode::REST:
                ret.ui_output.cursor = last_cursor;
                break;
            }
        }

        return ret;
    }
}