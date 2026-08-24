#include "lv_port_disp.h"
#include "tft_display.h"

static lv_disp_drv_t  disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[LV_HOR_RES_MAX * 10];

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area,
                       lv_color_t *color_p)
{
    uint16_t w = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t h = (uint16_t)(area->y2 - area->y1 + 1);
    (void)drv;

    TFT_SetAddrWindow(area->x1, area->y1, area->x2, area->y2);
    TFT_PushColors((uint16_t *)color_p, (uint32_t)w * h);
    lv_disp_flush_ready(drv);
}

void lv_port_disp_init(void)
{
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, LV_HOR_RES_MAX * 10);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}
