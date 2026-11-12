idf_component_register(SRCS "mlx90640.c" "MLX90640_I2C_Driver.c" "lib/melexis/MLX90640_API.c"
                    INCLUDE_DIRS "include" 
                    PRIV_INCLUDE_DIRS "lib/melexis"
                    REQUIRES esp_driver_i2c)