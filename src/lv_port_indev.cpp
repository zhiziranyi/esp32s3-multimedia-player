#include "lv_port_indev.h"
#include "joystick.h"

static void indev_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    uint8_t j = joystick_read();
    uint32_t key = 0;

    switch (j) {
    case JOY_UP:     key = LV_KEY_PREV;  break;
    case JOY_DOWN:   key = LV_KEY_NEXT;  break;
    case JOY_LEFT:   key = LV_KEY_LEFT;  break;
    case JOY_RIGHT:  key = LV_KEY_RIGHT; break;
    case JOY_SHORT:  key = LV_KEY_ENTER; break;
    case JOY_LONG:   key = LV_KEY_ESC;   break;
    default: break;
    }

    if (key) {
        data->state = LV_INDEV_STATE_PR;
        data->key   = key;
    } else {
        data->state = LV_INDEV_STATE_REL;
        data->key   = 0;
    }
}

void lv_port_indev_init(void)
{
    joystick_init();

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = indev_read;
    lv_indev_drv_register(&indev_drv);
}
