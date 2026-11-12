#include "mlx90640.h"
#include <stdio.h>
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "driver/i2c_types.h"
#include "MLX90640_I2C_Driver.h"

static i2c_master_dev_handle_t s_dev = NULL;
static const char *TAG = "MLX";


#define MLX_ADDR 0x33
#define MLX_SCLK_HZ 400*1000



static esp_err_t mlx90640_i2c_attach(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t scl_hz){
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = scl_hz,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus,&dev_cfg,&s_dev),
                        TAG,"mlx adding failed");

    return ESP_OK;
}

esp_err_t mlx_init(i2c_master_bus_handle_t bus){
    

    ESP_RETURN_ON_ERROR(i2c_master_probe(bus,MLX_ADDR,1000),
                        TAG, "mlx probing failed");

    ESP_RETURN_ON_ERROR(mlx90640_i2c_attach(bus,MLX_ADDR,MLX_SCLK_HZ),
                        TAG, "adding device failed");
    
    uint8_t addr[2] = { 0x80, 0x0D };            
    uint8_t rx[2];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_dev, addr, sizeof(addr), rx, sizeof(rx), 100),
                        TAG, "reading reg failed");
    uint16_t value = ((uint16_t)rx[0] << 8) | rx[1];
    ESP_LOGI(TAG,"MLX reg info: %d",value);
    return ESP_OK;


    
     
    
}


