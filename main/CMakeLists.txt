idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES hal_analog hal_imu core_logic comms_espnow hal_ui sh2)