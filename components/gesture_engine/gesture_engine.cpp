#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gesture_engine.hpp"
#include "helpers/imu_processor/imu_processor.hpp"
#include "helpers/mouse_processor/mouse_processor.hpp"
#include "helpers/mode_handlers/ui_mode_handler/ui_mode_handler.hpp"

namespace GestureEngine
{
    static QueueHandle_t gesture_state_queue = nullptr;

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

            ret.ui_output.cursor = UIModeHandler::handle(d_mouse, state.sensors);

            ret.ui_output.active_mode = InputMode::UI;
        }

        return ret;
    }
}