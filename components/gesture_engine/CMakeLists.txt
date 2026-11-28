idf_component_register(
    SRCS
        "gesture_engine.cpp"
        "helpers/imu_processor/imu_processor.cpp"
        "helpers/mouse_processor/mouse_processor.cpp"
        "helpers/ui_mode_handler/ui_mode_handler.cpp"
                    
    INCLUDE_DIRS
        "include"

    PRIV_INCLUDE_DIRS
        "."
                    
    REQUIRES
        board_config
        comms_protocol
)
