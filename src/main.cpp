#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "tft_display.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_fs.h"
#include "joystick.h"
#include "lvgl_ui.h"
#include "wifi_stream.h"
#include "net_services.h"
#include "secrets.h"

#define SD_CS   10
#define SD_SCK  14
#define SD_MISO 16
#define SD_MOSI 15

static uint8_t joy_to_lv_key(uint8_t j)
{
    switch (j) {
    case JOY_UP:     return LV_KEY_PREV;
    case JOY_DOWN:   return LV_KEY_NEXT;
    case JOY_LEFT:   return LV_KEY_LEFT;
    case JOY_RIGHT:  return LV_KEY_RIGHT;
    case JOY_SHORT:  return LV_KEY_ENTER;
    case JOY_LONG:   return LV_KEY_ESC;
    default:         return 0;
    }
}

void setup()
{
    TFT_Init();

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SPI, 400000)) {
        TFT_FillScreen(COLOR_RED);
        while (1) { delay(100); }
    }

    /* WiFi + network services */
    wifi_stream_init(WIFI_SSID, WIFI_PASS);
    net_services_init();

    lv_init();
    lv_port_disp_init();
    lv_port_fs_init();
    joystick_init();
    lvgl_ui_init();
}

void loop()
{
    static uint32_t last_tick = 0;
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    lvgl_ui_tick();
    net_services_loop();  // handle web server, fetch weather/RSS

    if (lvgl_ui_is_streaming()) {
        wifi_stream_show_frame();
        yield();
        return;
    }

    uint8_t j = joystick_read();
    if (j != JOY_NONE) {
        lvgl_ui_handle_key(joy_to_lv_key(j));
    }

    if (!lvgl_ui_is_viewing()) {
        lv_timer_handler();
    }

    delay(5);
}
