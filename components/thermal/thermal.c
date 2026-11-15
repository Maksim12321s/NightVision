#include "thermal.h"
#include "esp_check.h"
#include "esp_log.h"



#define THERMAL_MIN_SPAN 2.0f
#define THERMAL_FLIP_V 1
#define THERMAL_FLIP_H 1

static uint16_t s_lut_gray[256];
static uint16_t s_lut_iron[256];

static float s_idx[768];
static thermal_interp_t s_interp;

static uint16_t* s_lut;
static const char *TAG = "THERMAL";


static bool s_have_range = false;
static float s_min, s_max;

typedef struct { uint8_t pos, r, g, b; } stop_t;



static uint16_t s_row[320];
static const stop_t GRAY[] = { {0, 0, 0, 0}, {255, 255, 255, 255} };


static const stop_t IRON[] = {          // примерно, подправь на глаз
    {  0,   0,   0,   0},               // чёрный
    { 64,  32,   0, 140},               // тёмно-синий / фиолетовый
    {128, 200,   0, 120},               // малиновый
    {176, 255,  90,   0},               // оранжевый
    {224, 255, 210,   0},               // жёлтый
    {255, 255, 255, 255},               // белый
};

static uint8_t s_col_i0[320], s_col_i1[320];
static float   s_col_fx[320];
static int     s_col_w = 0; 

static void build_col_table(int w)
{
    for (int X = 0; X < w; X++) {
        float u = (X + 0.5f) * 32.0f / w - 0.5f;
        if (u < 0) u = 0;
        int i0 = (int)u;
        s_col_i0[X] = i0;
        s_col_i1[X] = (i0 < 31) ? i0 + 1 : 31;
        s_col_fx[X] = u - i0;
    }
    s_col_w = w;
}

static void build_lut(const stop_t *st, int n, uint16_t *lut)
{
    int k = 0;
    for (int i = 0; i < 256; i++) {
        while (k < n - 2 && i > st[k + 1].pos) k++;          // нашли отрезок st[k]…st[k+1]
        float f = (float)(i - st[k].pos) / (st[k + 1].pos - st[k].pos);
        uint8_t r = st[k].r + (st[k + 1].r - st[k].r) * f;
        uint8_t g = st[k].g + (st[k + 1].g - st[k].g) * f;
        uint8_t b = st[k].b + (st[k + 1].b - st[k].b) * f;
        uint16_t c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        lut[i] = __builtin_bswap16(c);                        // байты сразу для дисплея
    }
}

esp_err_t thermal_init(void){
    build_lut(GRAY, sizeof(GRAY) / sizeof(GRAY[0]), s_lut_gray);
    build_lut(IRON, sizeof(IRON) / sizeof(IRON[0]), s_lut_iron);
    s_lut = s_lut_iron;
    ESP_LOGI(TAG, "palettes ready");
    return ESP_OK;
}


void thermal_set_palette(thermal_palette_t p){
    s_lut = (p == THERMAL_PALETTE_IRON) ? s_lut_iron : s_lut_gray;
}

esp_err_t thermal_render(const float* temps, uint16_t* dst, int w, int h, thermal_range_t* range_out){
    float max = temps[0],min = temps[0];
   
    for(int i = 0; i < 32*24; i++){
        max = (max < temps[i])?temps[i]: max;
        min = (min > temps[i])?temps[i]: min;
    }
    if(max - min < 2.0f){
        float c = (max + min) / 2;
        min = c - THERMAL_MIN_SPAN / 2;
        max = c + THERMAL_MIN_SPAN / 2;
    }
    if(range_out){
        range_out->max = max;
        range_out->min = min;
    }

    if (s_have_range) {
        min = 0.8f * s_min + 0.2f * min;
        max = 0.8f * s_max + 0.2f * max;
    }
    s_min = min; s_max = max; s_have_range = true;
    float k = 255 / (s_max - s_min);

    for(int r = 0; r < 24; r++){
        int sr = THERMAL_FLIP_V ? 23 - r : r; 
        for(int c = 0; c < 32; c++){
            int sc = THERMAL_FLIP_H ? 31 - c : c;
            float v = (int)((temps[sr * 32 + sc] - s_min) * k);
            if(v < 0) v = 0; else if (v > 255) v = 255;
            s_idx[r * 32 + c] = v; 
        }
    }

    if (s_interp == THERMAL_INTERP_NEAREST) {
        for (int Y = 0; Y < h; Y++) {
            int r = Y * 24 / h;                       // какая тепловая строка
            for (int X = 0; X < w; X++) {
                int c = X * 32 / w;                   // какой тепловой столбец
                s_row[X] = s_lut[(int)(s_idx[r * 32 + c] + 0.5f)];
            }
            memcpy(&dst[Y * w], s_row, w * sizeof(uint16_t));
        }
    } else {
        if (s_col_w != w) build_col_table(w);
        float rowv[32];
        for(int y = 0; y < h; ++y){
            float v = (y + 0.5f) * 24.0f / h - 0.5f;
            if(v < 0) v = 0;
            int j0 = (int)v;
            int j1 = (j0 < 23) ? j0 + 1 : 23;
            float fy = v - j0;

            const float *a = &s_idx[j0 * 32];
            const float *b = &s_idx[j1 * 32];
            for (int c = 0; c < 32; c++) rowv[c] = a[c] + (b[c] - a[c]) * fy;
            for(int x = 0; x < w; ++x){
                float l = rowv[s_col_i0[x]];
                float r = rowv[s_col_i1[x]];
                float val = l + (r - l) * s_col_fx[x];
                s_row[x] = s_lut[(int)(val + 0.5f)];
            }
            memcpy(&dst[y * w], s_row, w * sizeof(uint16_t));
        }
    }
    return ESP_OK;
}

void thermal_set_interp(thermal_interp_t mode){
    s_interp = mode;
}

esp_err_t thermal_blend(const float *temps, const uint8_t *cam, uint16_t *dst, int w, int h, const thermal_overlay_t *ov, thermal_range_t *range_out){
    return ESP_OK;
}