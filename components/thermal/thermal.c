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

static int s_col_x0 = -1, s_col_tw = -1, s_col_w = -1;

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

static void build_col_table(int x0, int tw, int w)
{
    for (int X = 0; X < w; X++) {
        float u = (X - x0 + 0.5f) * 32.0f / tw - 0.5f;
        if (u < 0) u = 0;
        int i0 = (int)u;
        if (i0 > 31) i0 = 31;
        s_col_i0[X] = i0;
        s_col_i1[X] = (i0 < 31) ? i0 + 1 : 31;
        s_col_fx[X] = (i0 < 31) ? u - i0 : 0;
    }
    s_col_x0 = x0; s_col_tw = tw; s_col_w = w;
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


static float compute_range(const float *temps, thermal_range_t *range_out)
{
    float max = temps[0], min = temps[0];
    for (int i = 1; i < 768; i++) {
        if (temps[i] > max) max = temps[i];
        if (temps[i] < min) min = temps[i];
    }
    if (max - min < THERMAL_MIN_SPAN) {
        float c = (max + min) / 2;
        min = c - THERMAL_MIN_SPAN / 2;
        max = c + THERMAL_MIN_SPAN / 2;
    }
    if (s_have_range) {
        min = 0.8f * s_min + 0.2f * min;
        max = 0.8f * s_max + 0.2f * max;
    }
    s_min = min; s_max = max; s_have_range = true;

    if (range_out) { range_out->min = s_min; range_out->max = s_max; }
    return 255.0f / (s_max - s_min);          // это k
}

static void fill_idx(const float *temps, float k)
{
    for (int r = 0; r < 24; r++) {
        int sr = THERMAL_FLIP_V ? 23 - r : r;
        for (int c = 0; c < 32; c++) {
            int sc = THERMAL_FLIP_H ? 31 - c : c;
            float v = (temps[sr * 32 + sc] - s_min) * k;
            if (v < 0) v = 0; else if (v > 255) v = 255;
            s_idx[r * 32 + c] = v;
        }
    }
}

static void draw_nearest(uint16_t *dst, int w, int h)
{
    for (int Y = 0; Y < h; Y++) {
        int r = Y * 24 / h;
        for (int X = 0; X < w; X++) {
            int c = X * 32 / w;
            s_row[X] = s_lut[(int)(s_idx[r * 32 + c] + 0.5f)];
        }
        memcpy(&dst[Y * w], s_row, w * sizeof(uint16_t));
    }
}

static void draw_bilinear(uint16_t *dst, int w, int h)
{
    if (s_col_x0 != 0 || s_col_tw != w || s_col_w != w) build_col_table(0, w, w);

    float rowv[32];
    for (int Y = 0; Y < h; Y++) {
        float v = (Y + 0.5f) * 24.0f / h - 0.5f;
        if (v < 0) v = 0;
        int j0 = (int)v;
        int j1 = (j0 < 23) ? j0 + 1 : 23;
        float fy = v - j0;

        const float *a = &s_idx[j0 * 32];
        const float *b = &s_idx[j1 * 32];
        for (int c = 0; c < 32; c++)
            rowv[c] = a[c] + (b[c] - a[c]) * fy;

        for (int X = 0; X < w; X++) {
            float l = rowv[s_col_i0[X]];
            float r = rowv[s_col_i1[X]];
            s_row[X] = s_lut[(int)(l + (r - l) * s_col_fx[X] + 0.5f)];
        }
        memcpy(&dst[Y * w], s_row, w * sizeof(uint16_t));
    }
}

esp_err_t thermal_render(const float* temps, uint16_t* dst, int w, int h, thermal_range_t* range_out){
    if (!temps || !dst) return ESP_ERR_INVALID_ARG;
    if (w <= 0 || h <= 0 || w > 320 || h > 240) return ESP_ERR_INVALID_ARG;

    float k = compute_range(temps, range_out);
    fill_idx(temps, k);
    if (s_interp == THERMAL_INTERP_BILINEAR) draw_bilinear(dst, w, h);
    else                                     draw_nearest(dst, w, h);
    return ESP_OK;
}

void thermal_set_interp(thermal_interp_t mode){
    s_interp = mode;
}

static inline uint16_t blend565(uint16_t cam_be, uint16_t th_be, int a)   // a = 0…256
{
    uint16_t c = __builtin_bswap16(cam_be);
    uint16_t t = __builtin_bswap16(th_be);
    int r = (((t >> 11) & 31) * a + ((c >> 11) & 31) * (256 - a)) >> 8;
    int g = (((t >> 5)  & 63) * a + ((c >> 5)  & 63) * (256 - a)) >> 8;
    int b = (( t        & 31) * a + ( c        & 31) * (256 - a)) >> 8;
    return __builtin_bswap16((r << 11) | (g << 5) | b);
}

esp_err_t thermal_blend(const uint8_t *cam, uint16_t *dst, int w, int h, const thermal_overlay_t *ov){
    if (!cam || !dst || !ov || ov->w <= 0 || ov->h <= 0) return ESP_ERR_INVALID_ARG;
    if (w <= 0 || h <= 0 || w > 320 || h > 240)          return ESP_ERR_INVALID_ARG;

    const uint16_t *cam16 = (const uint16_t *)cam;

    if (!s_have_range || ov->alpha == 0) {                // тепла ещё нет / не нужно
        memcpy(dst, cam, w * h * sizeof(uint16_t));
        return ESP_OK;
    }

    int xs = ov->x < 0 ? 0 : ov->x;                       // первый столбец с теплом
    int xe = ov->x + ov->w > w ? w : ov->x + ov->w;       // за последним
    int ys = ov->y < 0 ? 0 : ov->y;
    int ye = ov->y + ov->h > h ? h : ov->y + ov->h;
    if (s_col_x0 != ov->x || s_col_tw != ov->w || s_col_w != w)
        build_col_table(ov->x, ov->w, w);
    int a = ov->alpha + (ov->alpha >> 7);

    float rowv[32];
    for (int Y = 0; Y < h; Y++) {
        memcpy(s_row, &cam16[Y * w], w * sizeof(uint16_t));   // строка камеры

        if (Y >= ys && Y < ye && xs < xe) {
            float v = (Y - ov->y + 0.5f) * 24.0f / ov->h - 0.5f;
            if (v < 0) v = 0;
            int j0 = (int)v;  if (j0 > 23) j0 = 23;
            int j1 = (j0 < 23) ? j0 + 1 : 23;
            float fy = (j0 < 23) ? v - j0 : 0;

            const float *pa = &s_idx[j0 * 32], *pb = &s_idx[j1 * 32];
            for (int c = 0; c < 32; c++) rowv[c] = pa[c] + (pb[c] - pa[c]) * fy;

            for (int X = xs; X < xe; X++) {
                float l = rowv[s_col_i0[X]], r = rowv[s_col_i1[X]];
                uint16_t th = s_lut[(int)(l + (r - l) * s_col_fx[X] + 0.5f)];
                s_row[X] = blend565(s_row[X], th, a);
            }
        }
        memcpy(&dst[Y * w], s_row, w * sizeof(uint16_t));
    }
    return ESP_OK;
}

esp_err_t thermal_update(const float *temps, thermal_range_t *range_out){
    if (!temps) return ESP_ERR_INVALID_ARG;
    float k = compute_range(temps, range_out);
    fill_idx(temps, k);
    return ESP_OK;
}