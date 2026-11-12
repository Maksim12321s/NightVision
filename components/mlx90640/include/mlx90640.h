#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C"{
#endif

esp_err_t mlx_init(i2c_master_bus_handle_t bus);


#ifdef __cplusplus
}
#endif




