#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C"{
#endif

esp_err_t mlx_init(i2c_master_bus_handle_t bus);
esp_err_t mlx90640_read_frame(float *temps_out, float *ta_out);

#ifdef __cplusplus
}
#endif




