idf_component_register(SRCS "gesture_engine.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES glove_types hal_analog hal_imu esp_timer)
