idf_component_register(SRCS "hal_imu.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board_config driver esp_driver_i2c sh2 esp_timer)
