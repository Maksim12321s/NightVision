idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES board mlx90640 thermal ui esp_timer esp_psram spi_flash)
