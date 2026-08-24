#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <stdint.h>

/* ST7789V display parameters */
#define TFT_WIDTH           240
#define TFT_HEIGHT          240

/* ASCII 8x16 font dimensions */
#define FONT_W              8
#define FONT_H              16

/* Color helpers (RGB565) */
#define RGB565(r, g, b)     ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))
#define COLOR_BLACK         RGB565(0, 0, 0)
#define COLOR_WHITE         RGB565(255, 255, 255)
#define COLOR_RED           RGB565(255, 0, 0)
#define COLOR_GREEN         RGB565(0, 255, 0)
#define COLOR_BLUE          RGB565(0, 0, 255)
#define COLOR_YELLOW        RGB565(255, 255, 0)

void TFT_Init(void);
void TFT_SetBacklight(uint8_t brightness);
void TFT_SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void TFT_FillScreen(uint16_t color);
void TFT_PushColors(uint16_t *colors, uint32_t count);
void TFT_StreamStart(void);
void TFT_StreamPush(uint16_t *data, uint32_t count);
void TFT_StreamEnd(void);

void TFT_SetTextColor(uint16_t fg, uint16_t bg);
void TFT_DrawChar(uint16_t x, uint16_t y, char c);
void TFT_DrawString(uint16_t x, uint16_t y, const char *str);
void TFT_DrawChar16(uint16_t x, uint16_t y, const uint8_t *bmp);
void TFT_DrawUTF8(uint16_t x0, uint16_t y0, const uint8_t *data, unsigned int len);

#endif /* TFT_DISPLAY_H */
