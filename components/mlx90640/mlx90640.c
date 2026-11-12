#include "mlx90640.h"
#include <stdio.h>
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "driver/i2c_types.h"
#include "MLX90640_I2C_Driver.h"
#include "mlx90640_i2c.h"


static const char *TAG = "MLX";


#define MLX_ADDR 0x33
#define MLX_SCLK_HZ 400*1000

static uint16_t s_ee[832];           // EEPROM
static uint16_t s_frame[834];        // одна подстраница: 768 + 64 + 2 служебных слова



esp_err_t mlx_init(i2c_master_bus_handle_t bus){
    

    ESP_RETURN_ON_ERROR(i2c_master_probe(bus,MLX_ADDR,1000),
                        TAG, "mlx probing failed");

    ESP_RETURN_ON_ERROR(mlx90640_i2c_attach(bus,MLX_ADDR,MLX_SCLK_HZ),
                        TAG, "adding device failed");
    
    
    return ESP_OK;   
}


