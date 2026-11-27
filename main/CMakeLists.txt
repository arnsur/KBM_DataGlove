idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES board_config hal_imu hal_analog battery_manager comms comms_protocol ui_manager gesture_engine esp_driver_i2c sh2)
