#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#ifdef __cplusplus
extern "C"{
#endif

esp_err_t mlx_init(i2c_master_bus_handle_t bus);
esp_err_t mlx90640_read_frame(float *temps_out, float *ta_out);
esp_err_t mlx90640_start_task(UBaseType_t prio, BaseType_t core);
esp_err_t mlx90640_get_latest(float *temps_out, float *ta_out, uint32_t *seq_out);
#ifdef __cplusplus
}
#endif




