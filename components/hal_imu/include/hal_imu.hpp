#pragma once

namespace HalImu
{
    bool quaternion(float &x, float &y, float &z, float &real);

    void enable_interrupts();

    bool is_data_ready();

    void init();
}