#pragma once
#include "glove_types.hpp"

namespace HalDisplay
{
    void init();
    
    void update_display(UIState &currentUIState);

    void sleep();
}