#pragma once

namespace HalIMU {
    void init();
    bool is_data_ready();
    bool get_quaternion(float &x, float &y, float &z, float &real);
    void sleep();
}