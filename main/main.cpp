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

#define LCD_H_RES        320     // ширина экрана в пикселях (в альбомной ориентации)
#define LCD_V_RES        240     // высота экрана

static const char *TAG = "main";
static float temps[768];

typedef enum{ VIEW_OPTICAL, VIEW_THERMAL, VIEW_OVERLAY }view_mode_t;
#define VIEW_MODE VIEW_OVERLAY
static view_mode_t s_mode = VIEW_MODE;

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

extern "C" void app_main(void){
    ESP_ERROR_CHECK(board_init());
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(board_i2c_bus_init(&bus));
    ESP_ERROR_CHECK(board_camera_init());
    ESP_ERROR_CHECK(mlx_init(bus));
    ESP_ERROR_CHECK(mlx90640_start_task(2, 1));
    ESP_ERROR_CHECK(thermal_init());
    ESP_ERROR_CHECK(board_display_init());
    thermal_set_interp(THERMAL_INTERP_BILINEAR);



    #if RUN_CAMERA_TEST
        log_free_mem("before camera");
    #endif

    #if RUN_CAMERA_TEST
        log_free_mem("after camera");
        camera_test();
    #endif


    #if RUN_LCD_TEST
        test_lcd();
    #endif


    float ta;
    thermal_range_t range;
    board_frame_t frame = {};
    board_frame_t frame_cam = {};
    thermal_overlay_t ov = {0, 0, 320, 240, 128};
    uint32_t seq = 0, last_seq = 0;
    uint16_t *thermal_buf = (uint16_t *)heap_caps_malloc(320 * 240 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (thermal_buf == NULL) {
        ESP_LOGE(TAG, "no PSRAM for thermal frame");
        return;
    }
    int c = 0;
    int64_t t0, t1,t2,t3,t4;
    esp_err_t err;
    int64_t delta0 = 0;
    int64_t delta1 = 0;
    int64_t delta2 = 0;
    int64_t delta3 = 0;
    while(1){
        bool new_thermal = false;
        t0 = esp_timer_get_time();
        if (s_mode != VIEW_OPTICAL && mlx90640_get_latest(temps, &ta, &seq) == ESP_OK && seq != last_seq) {
            last_seq = seq;
            new_thermal = true;
        } 
        t1 = esp_timer_get_time();
        if(s_mode == VIEW_THERMAL && !new_thermal) { vTaskDelay(1); continue; }
        
        if(s_mode != VIEW_THERMAL && board_camera_get_frame(&frame_cam) != ESP_OK) if(s_mode == VIEW_OPTICAL) continue;

        t2 = esp_timer_get_time();
        
        switch(s_mode){
            case VIEW_OPTICAL:
                frame = frame_cam;
                break;
            case VIEW_THERMAL:
                err = thermal_render(temps,thermal_buf,LCD_H_RES,LCD_V_RES,&range);
                if(err != ESP_OK) {ESP_LOGE(TAG,"render error: %s",esp_err_to_name(err)); break;}
                frame = make_frame(thermal_buf,LCD_H_RES,LCD_V_RES);
                break;
            case VIEW_OVERLAY:
                err = thermal_update(temps, &range);
                if(err != ESP_OK) {ESP_LOGE(TAG,"update error %s",esp_err_to_name(err)); break;}
                err = thermal_blend(frame_cam.data, thermal_buf,LCD_H_RES, LCD_V_RES, &ov);
                if(err != ESP_OK) {ESP_LOGE(TAG,"blending error %s",esp_err_to_name(err)); break;}
                frame = make_frame(thermal_buf,LCD_H_RES,LCD_V_RES);
                break;
                
        }
        t3 = esp_timer_get_time();
        err = board_display_frame(&frame);
        if(err != ESP_OK) ESP_LOGE(TAG,"error with dispay frame: %s",esp_err_to_name(err));
        t4 = esp_timer_get_time();
        board_camera_release_frame(&frame_cam);
        delta0 += (t1 - t0);
        delta1 += (t2 - t1);
        delta2 += (t3 - t2);
        delta3 += (t4 - t3);

        c++;
        if(c == 50){
            ESP_LOGI(TAG,"get thermal frame ms: %f", delta0 / (1000.0 * c));
            ESP_LOGI(TAG,"get optical frame ms: %f", delta1 / (1000.0 * c));
            ESP_LOGI(TAG,"render frame ms: %f", delta2 / (1000.0 * c));
            ESP_LOGI(TAG,"send to display frame ms: %f", delta3 / (1000.0 * c));
            ESP_LOGI(TAG,"Total ms: %f", (delta0 + delta1 + delta2 + delta3) / (1000.0 * c));
            ESP_LOGI(TAG,"Total FPS: %f", c * 1000*1000/(delta0 + delta1 + delta2 + delta3));
            delta0 = 0;
            delta1 = 0;
            delta2 = 0;
            delta3 = 0;
            c = 0;
        }
        

    }

}