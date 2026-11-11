#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/gpio.h"
#include "sensor.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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

//======================
//DISPLAY PINS
//======================
#define DISPLAY_PIN_CS      42
#define DISPLAY_PIN_RESET   41
#define DISPLAY_PIN_DC      40
#define DISPLAY_PIN_MOSI    39
#define DISPLAY_PIN_SCK     38

#define LCD_H_RES        320     // ширина экрана в пикселях (в альбомной ориентации)
#define LCD_V_RES        240     // высота экрана
#define LCD_STRIP_LINES  40      // высота одной полосы в строках

#define LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)
//======================

static esp_lcd_panel_handle_t s_panel = NULL;
static SemaphoreHandle_t s_trans_done = NULL;
static uint16_t *s_strip_buf;



esp_err_t board_init(void){ 
    return ESP_OK; 
}

static esp_err_t camera_sensor_setup(void){
    sensor_t* s = esp_camera_sensor_get();
    if(s == NULL) return ESP_ERR_NOT_FOUND;
    uint16_t pid = s->id.PID;
    ESP_LOGI(TAG,"PID: 0x%04x", pid);
    if(pid != OV3660_PID) {
        ESP_LOGW(TAG,"camera not OV3660 PID=0x%04x",pid);
        return ESP_OK;
    }
    if(s->set_vflip(s,1) != 0) ESP_LOGW(TAG, "vflip error in settings PID=0x%04x, OV3660 settings skipped", pid);
    if(s->set_brightness(s,1) != 0) ESP_LOGW(TAG, "brightness error in settings PID=0x%04x, OV3660 settings skipped", pid);
    if(s->set_saturation(s,-2) != 0) ESP_LOGW(TAG, "saturation error in settings PID=0x%04x, OV3660 settings skipped", pid);
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

//максимум 2 кадра одновременно
//перед использованием следующего нужно освободить предыдущие 2

esp_err_t board_camera_get_frame(board_frame_t *frame){
    if(frame == NULL) return ESP_ERR_INVALID_ARG;
    camera_fb_t* fb = esp_camera_fb_get();
    if(fb == NULL) return ESP_ERR_TIMEOUT;
    if(fb->format != PIXFORMAT_RGB565){
        esp_camera_fb_return(fb);
        return ESP_ERR_INVALID_RESPONSE;
    }
     
    frame->data = fb->buf;
    frame->height = fb->height;
    frame->width = fb->width;
    frame->len = fb->len;
    frame->handle = fb;
    return ESP_OK;

}

//после освобождения кадром пользоваться нельзя, туда будет загружен следующий
void board_camera_release_frame(board_frame_t *frame){
    if(frame == NULL || frame->handle == NULL) return;
    esp_camera_fb_return(frame->handle);
    frame->handle = NULL;
    frame->data = NULL;
}



static bool lcd_trans_done_cb(esp_lcd_panel_io_handle_t io,
                              esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_trans_done, &woken);
    return woken == pdTRUE;
}



esp_err_t board_display_init(void){
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = DISPLAY_PIN_MOSI,
        .sclk_io_num = DISPLAY_PIN_SCK,
        .miso_io_num = -1,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_STRIP_LINES * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST,&bus_cfg,SPI_DMA_CH_AUTO),
                        TAG,"error in bus setup");

    s_trans_done = xSemaphoreCreateBinary();
    if (s_trans_done == NULL) return ESP_ERR_NO_MEM;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = DISPLAY_PIN_CS,
        .dc_gpio_num = DISPLAY_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .user_ctx = NULL,
        .on_color_trans_done = lcd_trans_done_cb,
    };
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io), 
                        TAG, "panel io init failed");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = DISPLAY_PIN_RESET,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(io, &panel_cfg, &s_panel), 
                        TAG, "panel create failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), TAG, "swap_xy failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false), TAG, "mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on failed");

    s_strip_buf = heap_caps_malloc(LCD_H_RES * LCD_STRIP_LINES * 2,
                                    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    return ESP_OK;
}


esp_err_t board_display_fill(uint16_t color){
    
    uint16_t c = (color >> 8) | (color << 8);   // переставить байты
    for (int i = 0; i < LCD_H_RES * LCD_STRIP_LINES; ++i) {
        s_strip_buf[i] = c;
        
    }
    for (int y = 0; y < LCD_V_RES; y += LCD_STRIP_LINES) {
            ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + LCD_STRIP_LINES, s_strip_buf),
                                TAG, "draw failed");
            xSemaphoreTake(s_trans_done, portMAX_DELAY);
        }
    return ESP_OK;
}

esp_err_t board_display_rect(int x, int y,int w,int h,uint16_t color){
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > LCD_H_RES || y + h > LCD_V_RES)
        return ESP_ERR_INVALID_ARG;
    uint16_t c = (color >> 8) | (color << 8);
    if(w*h > LCD_H_RES*LCD_STRIP_LINES) return ESP_ERR_INVALID_SIZE;
    for (int i = 0; i < w * h; ++i) {
        s_strip_buf[i] = c;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, s_strip_buf),
                        TAG,"rect draw failed");
    xSemaphoreTake(s_trans_done, portMAX_DELAY);
    return ESP_OK;
    
}

esp_err_t board_display_frame(const board_frame_t *frame){
    if(s_panel == NULL || s_strip_buf == NULL) return ESP_ERR_INVALID_STATE;
    if(frame == NULL) return ESP_ERR_INVALID_ARG;
    if(frame->width != LCD_H_RES || frame->height != LCD_V_RES) return ESP_ERR_INVALID_SIZE;

    const size_t strip_bytes = LCD_H_RES * LCD_STRIP_LINES * 2;
    for (int y = 0; y < LCD_V_RES; y += LCD_STRIP_LINES) {
            memcpy(s_strip_buf,frame->data + y*LCD_H_RES*2,strip_bytes);
            ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + LCD_STRIP_LINES, s_strip_buf),
                        TAG,"frame draw failed");
            xSemaphoreTake(s_trans_done, portMAX_DELAY);
    }
    return ESP_OK;
}