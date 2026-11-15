#pragma once
#include "esp_err.h"

typedef enum {
    THERMAL_PALETTE_GRAY,
    THERMAL_PALETTE_IRON
}thermal_palette_t;

typedef enum {
    THERMAL_INTERP_NEAREST,
    THERMAL_INTERP_BILINEAR
}thermal_interp_t;

typedef struct{
    float min;
    float max;
} thermal_range_t;

typedef struct { 
    int x, y, w, h; 
    uint8_t alpha;
}thermal_overlay_t;

#ifdef __cplusplus
extern "C"{
#endif

esp_err_t thermal_init(void);
void thermal_set_palette(thermal_palette_t p);
void thermal_set_interp(thermal_interp_t in);
esp_err_t thermal_render(const float* temps, uint16_t* dst, int w, int h, thermal_range_t* range_out);
esp_err_t thermal_blend(const float *temps, const uint8_t *cam, uint16_t *dst, int w, int h, const thermal_overlay_t *ov, thermal_range_t *range_out);
esp_err_t thermal_update(const float *temps, thermal_range_t *range_out);

#ifdef __cplusplus
}
#endif