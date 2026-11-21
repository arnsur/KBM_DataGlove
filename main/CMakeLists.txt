idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES board_config battery_manager esp_driver_i2c)
