#include <cstdio>
#include "board.h"
#include "mlx90640.h"
#include "thermal.h"
#include "ui.h"

extern "C" void app_main(void)
{
    board_init();
    mlx_init();
    thermal_init();
    ui_init();

}
