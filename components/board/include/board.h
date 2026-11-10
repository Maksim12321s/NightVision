#pragma once
#include "esp_err.h"
#include "driver/gpio.h"


//======================
//CAMERA PINS
//======================
#define CAMERA_PIN_XCLK     GPIO_NUM_15
#define CAMERA_PIN_SIOD     GPIO_NUM_4
#define CAMERA_PIN_SIOC     GPIO_NUM_5
#define CAMERA_PIN_VSYNC    GPIO_NUM_6
#define CAMERA_PIN_HREF     GPIO_NUM_7
#define CAMERA_PIN_PCKL     GPIO_NUM_13
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




#ifdef __cplusplus
extern "C"{
#endif

esp_err_t board_init(void);
esp_err_t board_camera_init(void);

#ifdef __cplusplus
}
#endif