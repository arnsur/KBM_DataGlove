idf_component_register(SRCS "hal_analog.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES board_config esp_driver_gpio esp_adc)
