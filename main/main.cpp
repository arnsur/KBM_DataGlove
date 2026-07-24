#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
}