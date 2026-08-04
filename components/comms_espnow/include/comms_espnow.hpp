#pragma once
#include "glove_types.hpp"

namespace Comms
{
    void init();

    void vCommsTask(void *pvParameters);

    void sleep();
}