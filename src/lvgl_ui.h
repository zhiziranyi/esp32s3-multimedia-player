#ifndef LVGL_UI_H
#define LVGL_UI_H

#include "lvgl.h"

void lvgl_ui_init(void);
void lvgl_ui_show_debug(uint16_t adc_x, uint16_t adc_y, uint8_t sw);
void lvgl_ui_handle_key(uint32_t lv_key);
void lvgl_ui_tick(void);
int  lvgl_ui_is_viewing(void);
int  lvgl_ui_is_streaming(void);

#endif
