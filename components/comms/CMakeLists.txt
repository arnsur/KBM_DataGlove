idf_component_register(SRCS "comms.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board_config comms_protocol esp_wifi nvs_flash)
