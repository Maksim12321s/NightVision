#pragma once
#include "esp_err.h"

typedef enum {
    THERMAL_PALETTE_GRAY,
    THERMAL_PALETTE_IRON
}thermal_palette_t;

typedef struct{
    float min;
    float max;
} thermal_range_t;

#ifdef __cplusplus
extern "C"{
#endif

esp_err_t thermal_init(void);
void thermal_set_palette(thermal_palette_t p);
esp_err_t thermal_render(const float* temps, uint16_t* dst, int w, int h, thermal_range_t* range_out);


#ifdef __cplusplus
}
#endif