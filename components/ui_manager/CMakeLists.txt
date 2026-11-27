idf_component_register(SRCS "glove_ui.cpp" "hal_ui.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board_config gesture_engine battery_manager lvgl esp_driver_spi esp_lcd esp_driver_ledc)
