#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gesture_engine.hpp"
#include "helpers/imu_processor/imu_processor.hpp"
#include "helpers/mouse_processor/mouse_processor.hpp"

namespace GestureEngine
{
    static QueueHandle_t gesture_state_queue = nullptr;

    static int16_t cursor_x = 160;
    static int16_t cursor_y = 120;
    static float ui_remainder_x = 0.0f;
    static float ui_remainder_y = 0.0f;

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

            float exact_x = d_mouse.x * 0.2f + ui_remainder_x;
            float exact_y = d_mouse.y * 0.2f + ui_remainder_y;

            cursor_x += (int16_t)exact_x;
            cursor_y += (int16_t)exact_y;

            ui_remainder_x = exact_x - (int16_t)exact_x;
            ui_remainder_y = exact_y - (int16_t)exact_y;

            if (cursor_x < 0) cursor_x = 0;
            if (cursor_x > 319) cursor_x = 319;
            if (cursor_y < 0) cursor_y = 0;
            if (cursor_y > 239) cursor_y = 239;

            ret.ui_output.cursor.x = cursor_x;
            ret.ui_output.cursor.y = cursor_y;
            ret.ui_output.cursor.is_pressed = (state.sensors[0] > 1000);

            ret.ui_output.active_mode = InputMode::UI;
        }

        return ret;
    }
}