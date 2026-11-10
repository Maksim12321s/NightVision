#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_camera.h"

static const char *TAG = "BOARD";



esp_err_t board_init(void){ 
    return ESP_OK; 
}

esp_err_t board_camera_init(){
    camera_config_t cfg = {
        .pin_d0 = CAMERA_PIN_D0,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d7 = CAMERA_PIN_D7,
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCKL,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_xclk = CAMERA_PIN_XCLK,
        .ledc_channel = LEDC_CHANNEL_0,
        .ledc_timer = LEDC_TIMER_0,
        .pin_sccb_scl = CAMERA_PIN_SIOC,
        .pin_sccb_sda = CAMERA_PIN_SIOD,
        .xclk_freq_hz = 20000000,
        .pixel_format = PIXFORMAT_RGB565,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST

    };
    ESP_RETURN_ON_ERROR(esp_camera_init(&cfg),TAG,"camera init failed");

    return ESP_OK;
}