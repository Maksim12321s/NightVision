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

static const char *TAG = "main";

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
    const int N_frames = 1000;
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


extern "C" void app_main(void)
{
    // print_system_info();

    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(mlx_init());
    ESP_ERROR_CHECK(thermal_init());
    ESP_ERROR_CHECK(ui_init());

    #if RUN_CAMERA_TEST
    log_free_mem("before camera");
    #endif
    ESP_ERROR_CHECK(board_camera_init());
    #if RUN_CAMERA_TEST
    log_free_mem("after camera");
    camera_test_run();
    #endif


    #if RUN_LCD_TEST
    esp_log_level_set("spi_master", ESP_LOG_DEBUG);
    #endif
    ESP_ERROR_CHECK(board_display_init());
    #if RUN_LCD_TEST
    esp_log_level_set("spi_master", ESP_LOG_INFO);
    test_lcd();
    #endif
    board_frame_t frame = {};
    while(1){
        ESP_ERROR_CHECK(board_camera_get_frame(&frame));
        ESP_ERROR_CHECK(board_display_frame(&frame));
        board_camera_release_frame(&frame);
    }
}
