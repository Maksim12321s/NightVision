idf_component_register(SRCS "board.c"
                    REQUIRES esp_driver_i2c
                    PRIV_REQUIRES esp32-camera esp_driver_gpio 
                    INCLUDE_DIRS "include")