#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gesture_engine.hpp"

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
            // TODO: process data here
        }

        return ret;
    }
}