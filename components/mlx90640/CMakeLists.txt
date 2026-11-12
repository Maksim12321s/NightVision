idf_component_register(SRCS "mlx90640.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "lib/melexis"
                    REQUIRES esp_driver_i2c)