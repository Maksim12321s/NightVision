#include "mlx90640.h"
#include <stdio.h>
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "driver/i2c_types.h"
#include "mlx90640_i2c.h"
#include "MLX90640_API.h"

static const char *TAG = "MLX";


#define MLX_ADDR 0x33
#define MLX_SCLK_HZ 400*1000
#define MLX_REFRESH_CODE  0x05

#define MLX_EMISSIVITY   0.95f   // излучательная способность объекта (кожа, дерево, краска)
#define MLX_TA_SHIFT     8.0f    // поправка на самонагрев: tr = Ta − 8 (рекомендация Melexis)

static uint16_t s_ee[832];           // EEPROM
static uint16_t s_frame[834];        // одна подстраница: 768 + 64 + 2 служебных слова


static float s_temps[768];       // температуры, °C
static float s_ta;               // температура корпуса датчика, °C

static paramsMLX90640 params = {};


esp_err_t mlx_init(i2c_master_bus_handle_t bus){
    

    ESP_RETURN_ON_ERROR(i2c_master_probe(bus,MLX_ADDR,1000),
                        TAG, "mlx probing failed");

    ESP_RETURN_ON_ERROR(mlx90640_i2c_attach(bus,MLX_ADDR,MLX_SCLK_HZ),
                        TAG, "adding device failed");
    
    ESP_LOGI(TAG, "refresh=%d resolution=%d mode=%d",
         MLX90640_GetRefreshRate(MLX_ADDR),
         MLX90640_GetCurResolution(MLX_ADDR),
         MLX90640_GetCurMode(MLX_ADDR));

    int r = MLX90640_DumpEE(MLX_ADDR,s_ee);
    if (r != 0) { ESP_LOGE(TAG, "DumpEE failed: %d", r); return ESP_FAIL; }
    if((s_ee[0x0F] & 0xFF) != MLX_ADDR) return ESP_FAIL;
    ESP_LOGI(TAG,"s_ee[0x0F] = %u, s_ee[0x0C] = %u",s_ee[0x0F],s_ee[0x0C]);
    int err = MLX90640_ExtractParameters(s_ee,&params);
    if(err != 0){
        ESP_LOGW(TAG,"too much bad pixels code error: %d",err);
    }
    for (int i = 0; i < 5 && params.brokenPixels[i] != 0xFFFF; i++) {
        uint16_t n = params.brokenPixels[i];
        ESP_LOGW(TAG, "broken pixel %u (row %u, col %u)", n, n / 32, n % 32);
    }
    for (int i = 0; i < 5 && params.outlierPixels[i] != 0xFFFF; i++) {
        uint16_t n = params.outlierPixels[i];
        ESP_LOGW(TAG, "outlier pixel %u (row %u, col %u)", n, n / 32, n % 32);
    }


    MLX90640_SetRefreshRate(MLX_ADDR, MLX_REFRESH_CODE);

    return ESP_OK;   
}

esp_err_t mlx90640_read_frame(float *temps_out, float *ta_out){
    uint8_t got = 0;
    for(int attempt = 0; attempt < 6 && got != 0x3; attempt++){
        int sp = MLX90640_GetFrameData(MLX_ADDR, s_frame);
        if(sp < 0){
            ESP_LOGW(TAG, "GetFrameData: %d", sp);
            continue; 
        }
        float ta = MLX90640_GetTa(s_frame, &params);
        MLX90640_CalculateTo(s_frame, &params, MLX_EMISSIVITY, ta - MLX_TA_SHIFT, s_temps);
        s_ta = ta;
        got |= 1 << sp;  
    }

    memcpy(temps_out, s_temps, sizeof(s_temps));
    *ta_out = s_ta;
    return ESP_OK;
}

