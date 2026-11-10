#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/gpio.h"
#include "sensor.h"

static const char *TAG = "BOARD";

//======================
//CAMERA PINS
//======================
#define CAMERA_PIN_XCLK     GPIO_NUM_15
#define CAMERA_PIN_SIOD     GPIO_NUM_4
#define CAMERA_PIN_SIOC     GPIO_NUM_5
#define CAMERA_PIN_VSYNC    GPIO_NUM_6
#define CAMERA_PIN_HREF     GPIO_NUM_7
#define CAMERA_PIN_PCLK     GPIO_NUM_13
#define CAMERA_PIN_PWDN     -1
#define CAMERA_PIN_RESET    -1
#define CAMERA_PIN_D0       GPIO_NUM_11
#define CAMERA_PIN_D1       GPIO_NUM_9
#define CAMERA_PIN_D2       GPIO_NUM_8
#define CAMERA_PIN_D3       GPIO_NUM_10
#define CAMERA_PIN_D4       GPIO_NUM_12
#define CAMERA_PIN_D5       GPIO_NUM_18
#define CAMERA_PIN_D6       GPIO_NUM_17
#define CAMERA_PIN_D7       GPIO_NUM_16
//=====================

esp_err_t board_init(void){ 
    return ESP_OK; 
}

static esp_err_t camera_sensor_setup(void){
    sensor_t* s = esp_camera_sensor_get();
    if(s == NULL) return ESP_ERR_NOT_FOUND;
    uint16_t pid = s->id.PID;
    ESP_LOGI(TAG,"PID: 0x%04x", pid);
    if(pid == OV3660_PID) return ESP_OK;
    if(s->set_vflip(s,1) != 0) ESP_LOGW(TAG, "vflip error in settings PID=0x%04x, OV3660 settings skipped", s->id.PID);
    if(s->set_brightness(s,1) != 0) ESP_LOGW(TAG, "brightness error in settings PID=0x%04x, OV3660 settings skipped", s->id.PID);
    if(s->set_saturation(s,-2) != 0) ESP_LOGW(TAG, "saturation error in settings PID=0x%04x, OV3660 settings skipped", s->id.PID);
    return ESP_OK;

}

esp_err_t board_camera_init(void){
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
        .pin_pclk = CAMERA_PIN_PCLK,
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
    ESP_RETURN_ON_ERROR(camera_sensor_setup(),TAG,"setup failed");
    return ESP_OK;
}

