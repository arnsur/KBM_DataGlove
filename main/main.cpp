#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery_manager.hpp"

void battery_task(void *pvParameters)
{
    BatteryManager::init();
    
    BatteryManager::BatteryDetails details;

    while(1)
    {
        BatteryManager::battery_details(details);

        printf("Pct: %f | Capacity: %u mAh\n", details.pct, details.capacity_mah);

        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
}

extern "C" void app_main(void)
{
    xTaskCreatePinnedToCore(
        battery_task,
        "Battery Task",
        4096,
        NULL,
        3,
        NULL,
        1
    );
}
