idf_component_register(SRCS "battery_manager.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board_config esp_driver_i2c)
