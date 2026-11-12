#include "mlx90640_i2c.h"
#include "MLX90640_I2C_Driver.h"
#include <stdio.h>
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "driver/i2c_types.h"
#include "MLX90640_I2C_Driver.h"
#include "mlx90640_i2c.h"

static const char *TAG = "MLX_I2C_DRIVER";
static i2c_master_dev_handle_t s_dev = NULL;



esp_err_t mlx90640_i2c_attach(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t scl_hz){
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = scl_hz,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus,&dev_cfg,&s_dev),
                        TAG,"mlx adding failed");

    return ESP_OK;
}

int  MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data){

    esp_err_t err = i2c_master_transmit_receive(s_dev,startAddress, sizeof(startAddress), (uint8_t* )data, 2 * nMemAddressRead, 200);
    if(err != ESP_OK) return -1;
    for(int i = 0; i < nMemAddressRead; ++i){
        data[i] = __builtin_bswap16(data[i]);
    }
    
    return 0;
}


int  MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data){
    uint8_t buff[4] = {
        writeAddress >> 8,
        writeAddress & 0xFF,
        data >> 8,
        data & 0xFF
    };
    esp_err_t err = i2c_master_transmit(s_dev,buff,sizeof(buff),100);


    return 0;
}

void MLX90640_I2CInit(void){

}

void MLX90640_I2CFreqSet(int freq){

}

int  MLX90640_I2CGeneralReset(void){
    return 0; //не используется
}