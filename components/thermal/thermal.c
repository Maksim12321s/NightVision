#include "thermal.h"
#include "esp_check.h"
#include "esp_log.h"

static uint16_t s_lut_gray[256];
static uint16_t s_lut_iron[256];
static uint16_t* s_lut;
static const char *TAG = "THERMAL";
typedef struct { uint8_t pos, r, g, b; } stop_t;

static const stop_t GRAY[] = { {0, 0, 0, 0}, {255, 255, 255, 255} };

static const stop_t IRON[] = {          // примерно, подправь на глаз
    {  0,   0,   0,   0},               // чёрный
    { 64,  32,   0, 140},               // тёмно-синий / фиолетовый
    {128, 200,   0, 120},               // малиновый
    {176, 255,  90,   0},               // оранжевый
    {224, 255, 210,   0},               // жёлтый
    {255, 255, 255, 255},               // белый
};


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
    s_lut = s_lut_gray;
    ESP_LOGI(TAG, "palettes ready");
    return ESP_OK;
}


void thermal_set_palette(thermal_palette_t p){
    s_lut = (p == THERMAL_PALETTE_IRON) ? s_lut_iron : s_lut_gray;
}

esp_err_t thermal_render(const float* temps, uint16_t* dst, int w, int h, thermal_range_t* range_out){

    return ESP_OK;
}