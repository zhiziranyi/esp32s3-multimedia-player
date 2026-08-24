#ifndef CN_FONT_H
#define CN_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t code;
    uint8_t  bmp[32];
} cn_glyph_t;

#define CN_FONT_COUNT 407
extern const cn_glyph_t cn_font[CN_FONT_COUNT];

/* Look up a glyph by Unicode code point; returns bmp or NULL */
const uint8_t *cn_font_lookup(uint16_t unicode);

#ifdef __cplusplus
}
#endif

#endif
