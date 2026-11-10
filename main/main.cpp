#include <cinttypes>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

#include "board.h"
#include "mlx90640.h"
#include "thermal.h"
#include "ui.h"

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

extern "C" void app_main(void)
{
    print_system_info();

    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(mlx_init());
    ESP_ERROR_CHECK(thermal_init());
    ESP_ERROR_CHECK(ui_init());
    ESP_ERROR_CHECK(board_camera_init());
    board_frame_t frame;
    ESP_ERROR_CHECK(board_camera_get_frame(&frame));
    ESP_LOGI(TAG,"HEIGHT: %d\n",frame.height);
    ESP_LOGI(TAG,"WIDTH: %d\n",frame.width);
    ESP_LOGI(TAG,"LENGTH: %d\n",frame.len);
    board_camera_release_frame(&frame);
}
