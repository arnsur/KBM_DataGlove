idf_component_register(SRCS "hal_imu.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board_config esp_driver_i2c esp_driver_gpio sh2 esp_timer)
