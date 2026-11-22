idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES board_config hal_imu hal_analog battery_manager esp_driver_i2c sh2)
