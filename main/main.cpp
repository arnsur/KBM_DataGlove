#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
    // This runs once when the ESP32 boots up
    printf("Hello World! The Data Glove is alive!\n");

    // This replaces your Arduino loop()
    while (1) {
        printf("Main loop is running...\n");
        
        // Pause for 1000 milliseconds (1 second)
        // In FreeRTOS, you MUST delay to feed the watchdog timer!
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}