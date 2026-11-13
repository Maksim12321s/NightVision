#include <cinttypes>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "mlx90640.h"
#include "thermal.h"
#include "ui.h"

#define RUN_CAMERA_TEST 0
#define RUN_LCD_TEST 0
#define RUN_MLX_TEST 0
#define RUN_CAMERA_MODE 0


#define LCD_H_RES        320     // ширина экрана в пикселях (в альбомной ориентации)
#define LCD_V_RES        240     // высота экрана

static const char *TAG = "main";
static float temps[768];

static void print_system_info(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);

    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGW(TAG, "Не удалось получить размер Flash");
    }

    ESP_LOGI(TAG, "Чип: %s, ядер: %d, ревизия v%d.%d, CPU %d МГц",
             CONFIG_IDF_TARGET, info.cores,
             info.revision / 100, info.revision % 100,
             CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);

    ESP_LOGI(TAG, "Возможности:%s%s%s",
             (info.features & CHIP_FEATURE_WIFI_BGN) ? " Wi-Fi" : "",
             (info.features & CHIP_FEATURE_BLE)      ? " BLE"   : "",
             (info.features & CHIP_FEATURE_EMB_PSRAM)? " встроенная PSRAM" : "");

    ESP_LOGI(TAG, "Flash: %" PRIu32 " МБ", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "PSRAM: %zu МБ", esp_psram_get_size() / (1024 * 1024));

    ESP_LOGI(TAG, "Внутренняя RAM: свободно %zu КБ из %zu КБ",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
             heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024);
    ESP_LOGI(TAG, "PSRAM: свободно %zu КБ из %zu КБ",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024,
             heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024);
}

static void log_free_mem(const char *stage)
{
    ESP_LOGI(TAG, "[%s] internal: free %zu KB (largest DMA block %zu KB), PSRAM: free %zu KB",
             stage,
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024,
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);

}

#if RUN_CAMERA_TEST
static void camera_test(void){
    board_frame_t frame = {};
    for(int i = 0; i < 100; ++i){
        ESP_ERROR_CHECK(board_camera_get_frame(&frame));
        board_camera_release_frame(&frame);
    }
    int32_t t0 = esp_timer_get_time();
    for(int i = 0; i < 10; ++i){
        ESP_ERROR_CHECK(board_camera_get_frame(&frame));
        board_camera_release_frame(&frame);
    }
    int64_t t1 = esp_timer_get_time();
    float fps = 10 * 1e6f / (t1 - t0);
    
    int64_t end = esp_timer_get_time() + 30 * 1000000;
    int64_t next_log = 0;
    while(esp_timer_get_time() < end){
        ESP_ERROR_CHECK(board_camera_get_frame(&frame));
        if(frame.len != 320*240*2) ESP_LOGW(TAG,"len size not right");
        if(esp_timer_get_time() > next_log){
            int64_t sum = 0;
            for(size_t j = 0; j < frame.len; ++j){
                sum += frame.data[j];
            }
            unsigned cp = sum / frame.len;
            ESP_LOGI(TAG,"brightness: %u", cp);
            next_log = esp_timer_get_time() + 1000000;
        }


        board_camera_release_frame(&frame);

    }

    
    ESP_LOGI(TAG,"fps: %.1f",fps);
}
#endif

#if RUN_LCD_TEST
static void test_lcd(){
    const int N = 20;
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < N; ++i) {
        ESP_ERROR_CHECK(board_display_fill(i % 2 ? 0xF800 : 0x001F));   
    }
    int64_t t1 = esp_timer_get_time();
    float ms = (t1 - t0) / 1000.0f / N;
    ESP_LOGI(TAG, "frame: %.1f ms, %.1f fps", ms, 1000.0f / ms);
}
#endif

#if RUN_MLX_TEST
static void test_mlx(void){
    static float temps[768];   // static — 3 КБ, на стек main не кладём
    float ta;
    while (1) {
        int64_t t0 = esp_timer_get_time();
        esp_err_t err = mlx90640_read_frame(temps, &ta);
        int64_t t1 = esp_timer_get_time();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read_frame: %s", esp_err_to_name(err));
            continue;
        }
        float tmin = temps[0], tmax = temps[0];
        for (int i = 1; i < 768; i++) {
            if (temps[i] < tmin) tmin = temps[i];
            if (temps[i] > tmax) tmax = temps[i];
        }
        ESP_LOGI(TAG, "Ta=%.1f min=%.1f max=%.1f center=%.1f  frame=%lld ms",
                ta, tmin, tmax, temps[400], (t1 - t0) / 1000);
    }
}
#endif

#if RUN_CAMERA_MODE
void camera_mode(void){
    board_frame_t frame = {};
    while(1){
        ESP_ERROR_CHECK(board_camera_get_frame(&frame));
        ESP_ERROR_CHECK(board_display_frame(&frame));
        board_camera_release_frame(&frame);
    }
}
#endif

static board_frame_t make_frame(uint16_t* data_,uint16_t w,uint16_t h){
    board_frame_t frame = {
        .data = (uint8_t*)data_,
        .len = 320 * 240 * sizeof(uint16_t),
        .width = w,
        .height = h,
        .handle = nullptr,
    };
    return frame;
}
extern "C" void app_main(void)
{
    // print_system_info();
    ESP_ERROR_CHECK(board_init());
    
    ESP_ERROR_CHECK(thermal_init());
    ESP_ERROR_CHECK(ui_init());

    #if RUN_CAMERA_TEST
    log_free_mem("before camera");
    #endif
    ESP_ERROR_CHECK(board_camera_init());

    #if RUN_CAMERA_TEST
    log_free_mem("after camera");
    camera_test();
    #endif


    #if RUN_LCD_TEST
    esp_log_level_set("spi_master", ESP_LOG_DEBUG);
    #endif
    ESP_ERROR_CHECK(board_display_init());
    #if RUN_LCD_TEST
    esp_log_level_set("spi_master", ESP_LOG_INFO);
    test_lcd();
    #endif

    i2c_master_bus_handle_t bus = {};
    ESP_ERROR_CHECK(board_i2c_bus_init(&bus));
    ESP_ERROR_CHECK(mlx_init(bus));

    #if RUN_MLX_TEST
    test_mlx();
    #endif
    
    #if RUN_CAMERA_MODE
    camera_mode();
    #endif

    uint16_t *thermal_buf = (uint16_t *)heap_caps_malloc(320 * 240 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (thermal_buf == NULL) {
    ESP_LOGE(TAG, "no PSRAM for thermal frame");
    return;
}
    float ta;
    thermal_range_t range;
    board_frame_t frame = {};
    while(1){
        mlx90640_read_frame(temps,&ta);
        thermal_render(temps,thermal_buf,LCD_H_RES,LCD_V_RES,&range);
        frame = make_frame(thermal_buf,LCD_H_RES,LCD_V_RES);
        board_display_frame(&frame);
        ESP_LOGI(TAG, "Ta=%.1f palette %.1f … %.1f °C", ta, range.min, range.max);
    }
}
