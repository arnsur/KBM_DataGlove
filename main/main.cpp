#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_analog.hpp" // Bring in your analog component

// This is your FreeRTOS Task (equivalent to the Arduino loop)
void debug_analog_task(void *pvParameters) {
    while (1) {
        // // 1. Grab the latest data
        HalAnalog::SensorArray sensors = HalAnalog::getSensorValues();
        // int battery_mv = HalAnalog::readBatteryMilliVolts();

        // // 2. Print the Battery Voltage
        // printf("Battery: %d mV | Mux: ", battery_mv);

        // 3. Loop through and print the 12 Mux channels
        for(int i = 0; i < HalAnalog::NUM_SENSORS; i++) {
            printf("[%d]: %d  ", i, sensors[i]);
        }
        printf("\n"); // New line at the end

        // 4. Delay to prevent spamming the monitor and triggering the watchdog timer
        // pdMS_TO_TICKS converts standard milliseconds into FreeRTOS clock ticks
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

// The main entry point MUST be wrapped in extern "C" so the OS can find it
extern "C" void app_main(void) {
    printf("Booting Glove OS...\n");

    // Initialize the hardware first
    HalAnalog::init();
    
    printf("Hardware initialized. Spawning debug task...\n");

    // Spin up the task to run in the background
    // Parameters: function name, task name, stack size (bytes), parameters, priority, task handle
    xTaskCreate(debug_analog_task, "DebugAnalog", 4096, NULL, 5, NULL);
}