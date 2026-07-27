file(GLOB_RECURSE app_sources "*.c" "*.cpp")

idf_component_register(SRCS ${app_sources} "hal_display.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board_config glove_types esp_driver_spi esp_lcd lvgl esp_timer)
