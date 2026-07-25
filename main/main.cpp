// TEST SCRIPT

#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
    #include "sh2.h"
    #include "sh2_err.h"
    #include "sh2_hal.h"
}

#include "hal_imu.hpp"

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    printf("Initializing BNO085 IMU...\n");
    HalIMU::init();
    printf("BNO085 Initialized. Starting sensor read loop...\n");

    while (true)
    {
        if (HalIMU::is_data_ready())
        {
            sh2_service();
        }

        float qx, qy, qz, qw;
        if (HalIMU::get_quaternion(qx, qy, qz, qw))
        {
            // Convert quaternion (qx,qy,qz,qw) to aerospace Euler angles (roll, pitch, yaw)
            // Using Tait-Bryan angles Z (yaw) - Y (pitch) - X (roll) (intrinsic rotations)
            double x = qx;
            double y = qy;
            double z = qz;
            double w = qw;

            // roll (x-axis rotation)
            double sinr_cosp = 2.0 * (w * x + y * z);
            double cosr_cosp = 1.0 - 2.0 * (x * x + y * y);
            double roll = atan2(sinr_cosp, cosr_cosp);

            // pitch (y-axis rotation)
            double sinp = 2.0 * (w * y - z * x);
            double pitch;
            if (fabs(sinp) >= 1)
                pitch = copysign(M_PI / 2.0, sinp); // use 90 degrees if out of range
            else
                pitch = asin(sinp);

            // yaw (z-axis rotation)
            double siny_cosp = 2.0 * (w * z + x * y);
            double cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
            double yaw = atan2(siny_cosp, cosy_cosp);

            // Convert to degrees for readable output
            const double RAD_TO_DEG = 180.0 / M_PI;
            double roll_deg = roll * RAD_TO_DEG;
            double pitch_deg = pitch * RAD_TO_DEG;
            double yaw_deg = yaw * RAD_TO_DEG;

            printf("Quat xyzw: %8.4f %8.4f %8.4f %8.4f  |  Euler xyz (deg): %8.3f %8.3f %8.3f\n",
                   qx, qy, qz, qw, roll_deg, pitch_deg, yaw_deg);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}