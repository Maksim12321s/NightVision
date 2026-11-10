#pragma once
#include "esp_err.h"


typedef struct {
    const uint8_t *data;   // пиксели RGB565, 2 байта на пиксель
    size_t len;            // длина в байтах
    uint16_t width;
    uint16_t height;
    void *handle;          // служебное поле board, main его не трогает
} board_frame_t;


#ifdef __cplusplus
extern "C"{
#endif

esp_err_t board_init(void);
esp_err_t board_camera_init(void);
esp_err_t board_camera_get_frame(board_frame_t *frame);
void board_camera_release_frame(board_frame_t *frame);

#ifdef __cplusplus
}
#endif