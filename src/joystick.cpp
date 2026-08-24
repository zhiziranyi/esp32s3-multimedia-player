#include "joystick.h"
#include <Arduino.h>

#define JOY_X_PIN   4
#define JOY_Y_PIN   5
#define JOY_SW_PIN  6

#define JOY_CENTER    1800
#define JOY_DEADZONE  300
#define SW_DEBOUNCE   30
#define SW_LONG       500

void joystick_init(void)
{
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    analogReadResolution(12);
    analogSetPinAttenuation(JOY_X_PIN, ADC_11db);
    analogSetPinAttenuation(JOY_Y_PIN, ADC_11db);
    analogRead(JOY_X_PIN);
    analogRead(JOY_Y_PIN);
}

uint8_t joystick_read(void)
{
    uint32_t now = millis();

    int16_t dy_axis = (int16_t)(analogRead(JOY_X_PIN) - JOY_CENTER);
    int16_t dx_axis = (int16_t)(analogRead(JOY_Y_PIN) - JOY_CENTER);

    uint8_t dir = JOY_NONE;
    if (abs(dx_axis) > abs(dy_axis)) {
        if      (dx_axis >  JOY_DEADZONE) dir = JOY_RIGHT;
        else if (dx_axis < -JOY_DEADZONE) dir = JOY_LEFT;
    } else {
        if      (dy_axis >  JOY_DEADZONE) dir = JOY_DOWN;
        else if (dy_axis < -JOY_DEADZONE) dir = JOY_UP;
    }

    static uint8_t  last_dir = JOY_NONE;
    static uint32_t dir_since;
    if (dir != last_dir) {
        last_dir  = dir;
        dir_since = now;
        if (dir != JOY_NONE) return dir;
    }
    if (dir != JOY_NONE && (now - dir_since) > 250) {
        dir_since = now;
        return dir;
    }

    static uint8_t  sw_db      = 1;
    static uint8_t  sw_last    = 1;
    static uint32_t sw_changed;
    static uint32_t sw_press;
    static uint8_t  sw_short_done;

    uint8_t raw = (digitalRead(JOY_SW_PIN) == LOW) ? 0 : 1;

    if (raw != sw_last) { sw_changed = now; sw_last = raw; }
    if ((now - sw_changed) > SW_DEBOUNCE && raw != sw_db) {
        sw_db = raw;
        if (sw_db == 0) {
            sw_press = now;
            sw_short_done = 0;
        } else {
            if (!sw_short_done) return JOY_SHORT;
        }
    }
    if (sw_db == 0 && !sw_short_done && (now - sw_press) > SW_LONG) {
        sw_short_done = 1;
        return JOY_LONG;
    }

    return JOY_NONE;
}

void joystick_debug(uint16_t *x, uint16_t *y, uint8_t *sw)
{
    *x  = (uint16_t)analogRead(JOY_X_PIN);
    *y  = (uint16_t)analogRead(JOY_Y_PIN);
    *sw = (digitalRead(JOY_SW_PIN) == LOW) ? 0 : 1;
}
