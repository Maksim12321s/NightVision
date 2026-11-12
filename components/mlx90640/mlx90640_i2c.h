#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C"{
#endif

esp_err_t mlx90640_i2c_attach(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t scl_hz);
void MLX90640_I2CInit(void);
int  MLX90640_I2CGeneralReset(void);
int  MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data);
int  MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data);
void MLX90640_I2CFreqSet(int freq);

#ifdef __cplusplus
}
#endif




