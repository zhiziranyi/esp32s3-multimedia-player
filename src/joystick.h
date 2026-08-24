#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>

enum { JOY_NONE = 0, JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT,
       JOY_SHORT, JOY_LONG };

void joystick_init(void);
uint8_t joystick_read(void);
void joystick_debug(uint16_t *x, uint16_t *y, uint8_t *sw);

#endif
