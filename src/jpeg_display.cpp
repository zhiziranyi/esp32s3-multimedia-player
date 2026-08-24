#include "jpeg_display.h"
#include <string.h>

static unsigned char jpeg_pool[5500];
static File *g_jpeg_file;

static unsigned int jpeg_input_func(JDEC *jdec, unsigned char *buff, unsigned int nbyte)
{
    (void)jdec;
    if (buff) {
        return g_jpeg_file->read(buff, nbyte);
    } else {
        unsigned char byte;
        int br = g_jpeg_file->read(&byte, 1);
        return br ? (0x100U | byte) : 0;
    }
}

static int jpeg_output_func(JDEC *jdec, void *bitmap, JRECT *rect)
{
    (void)jdec;

    int src_w = rect->right - rect->left + 1;

    if (rect->left >= TFT_WIDTH || rect->top >= TFT_HEIGHT ||
        rect->right < 0 || rect->bottom < 0) {
        return 1;
    }

    int skip_l = 0, skip_t = 0;
    if (rect->left < 0)      { skip_l = -rect->left; rect->left = 0; }
    if (rect->top < 0)       { skip_t = -rect->top;  rect->top = 0; }
    if (rect->right  >= TFT_WIDTH)  rect->right  = TFT_WIDTH - 1;
    if (rect->bottom >= TFT_HEIGHT) rect->bottom = TFT_HEIGHT - 1;

    int w = rect->right - rect->left + 1;
    int h = rect->bottom - rect->top + 1;

    unsigned char *src = (unsigned char *)bitmap + (skip_t * src_w + skip_l) * 3;
    uint16_t line_buf[TFT_WIDTH];

    for (int y = 0; y < h; y++) {
        uint16_t *dst = line_buf;
        unsigned char *px = src;
        for (int x = 0; x < w; x++) {
            *dst++ = RGB565(px[0], px[1], px[2]);
            px += 3;
        }
        TFT_SetAddrWindow(rect->left, rect->top + y, rect->right, rect->top + y);
        TFT_PushColors(line_buf, w);
        src += src_w * 3;
    }
    return 1;
}

int JPEG_Display(File *file)
{
    JDEC jdec;
    JRESULT rc;

    g_jpeg_file = file;
    file->seek(0);

    memset(&jdec, 0, sizeof(JDEC));
    rc = jd_prepare(&jdec, jpeg_input_func, jpeg_pool, sizeof(jpeg_pool), NULL);
    if (rc != JDR_OK) return (int)rc;

    rc = jd_decomp(&jdec, jpeg_output_func, 0);
    if (rc != JDR_OK) return (int)rc;

    return 0;
}
