/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor
/ Based on original work by ChaN, with color JPEG support.
/ Decodes baseline JPEG (grayscale and YCbCr color with subsampling).
/----------------------------------------------------------------------------*/

#include "tjpgd.h"
#include <stddef.h>

/* Static quantization table storage (enough for 4 tables) */
static long g_qttbl[4][64];

/* Zig-zag scan order */
static const unsigned char Zig[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/* IDCT constants scaled by 2^11 */
#define W1  2841
#define W2  2676
#define W3  2408
#define W5  1609
#define W6  1108
#define W7   565

/* ---- Bit stream helpers -------------------------------------------------- */

static int bitext(JDEC *jd, unsigned int nbits)
{
    unsigned int msk = jd->dmsk;
    unsigned int buf = 0;

    if (msk < nbits) {
        unsigned int byte;
        if (jd->dctr == 0) {
            byte = jd->infunc(jd, NULL, 0);
            if (jd->nrst && (unsigned char)byte == 0xFF) {
                byte = jd->infunc(jd, NULL, 0);
                if (byte && (byte & 0xD0) == 0xD0) {
                    jd->dptr[0] = 0xFF;
                    jd->dptr[1] = byte;
                    jd->dptr += 2;
                    jd->dctr += 2;
                    byte = jd->infunc(jd, NULL, 0);
                } else {
                    byte = 0;
                }
            }
            if (byte == 0) return -1;
            *jd->dptr++ = (unsigned char)byte;
            jd->dctr++;
        }
        buf = jd->inbuf << (8 - msk);
        jd->inbuf = (unsigned int)*jd->dptr++;
        jd->dctr--;
        buf |= jd->inbuf >> msk;
        msk += 8;
    }
    jd->dmsk = (unsigned char)(msk - nbits);
    return (int)((buf >> (msk - nbits)) & ((1u << nbits) - 1));
}

/* ---- Huffman decoder ----------------------------------------------------- */

static int huffext(JDEC *jd, const unsigned char *hbits, const unsigned short *hcode, const unsigned char *hdata)
{
    unsigned int code = 0;
    int bits, len;

    for (len = 1; len <= 16; len++) {
        bits = bitext(jd, 1);
        if (bits < 0) return -1;
        code = (code << 1) | bits;
        if (hcode[code] != 0xFFFF) {
            return (int)hcode[code];
        }
    }
    return -1;
}

static int huffval(JDEC *jd, int n)
{
    unsigned int mask = (unsigned int)(1L << (n - 1));
    int val = 0;

    if (n) {
        while (n--) {
            int b = bitext(jd, 1);
            if (b < 0) return -1;
            val = (val << 1) | b;
        }
    }
    if (val < (int)mask) {
        val = val - (int)(mask << 1) + 1;
    }
    return val;
}

/* ---- Build Huffman tables from DHT data --------------------------------- */

static int build_huffman(JDEC *jd, const unsigned char *data, int len, int idx)
{
    unsigned char bits[16], counts[16];
    unsigned short code[512];
    unsigned char tbl, cls;
    int i, j, k, max;
    unsigned short maxcode;

    if (len < 17) return JDR_FMT1;
    tbl = data[0];
    cls = (tbl >> 4) & 1;
    tbl = tbl & 1;

    for (i = 0; i < 16; i++) bits[i] = data[i + 1];
    max = 0;
    for (i = 0; i < 16; i++) {
        counts[i] = bits[i];
        max += bits[i];
    }
    if (len < (unsigned int)(17 + max)) return JDR_FMT1;

    /* Build code table */
    maxcode = 0;
    j = 0;
    k = 0;
    for (i = 0; i < 16; i++) {
        if (bits[i]) {
            j = (j << 1) | 1;
            maxcode = (unsigned short)((maxcode << 1) | 1);
        } else {
            j <<= 1;
            maxcode <<= 1;
        }
        for (; k <= j && k < 512; k++) {
            code[k] = maxcode;
        }
    }

    {
        unsigned char *base = (unsigned char *)jd->pool + jd->pool_used;
        jd->huffbits[cls][tbl] = base;
        jd->huffdata[cls][tbl] = base + 16;
        jd->huffcode[cls][tbl] = (unsigned short *)(base + 16 + max);
        jd->pool_used += 16 + max + 512 * sizeof(unsigned short);
    }

    for (i = 0; i < 16; i++) {
        ((unsigned char *)jd->huffbits[cls][tbl])[i] = bits[i];
    }
    for (i = 0; i < max; i++) {
        ((unsigned char *)jd->huffdata[cls][tbl])[i] = data[17 + i];
    }
    for (i = 0; i < 512; i++) {
        if (counts[0]) {
            ((unsigned short *)jd->huffcode[cls][tbl])[i] = (unsigned short)i;
            counts[0]--;
        } else {
            unsigned short tmp = code[i];
            ((unsigned short *)jd->huffcode[cls][tbl])[i] = (tmp < 512) ? tmp : 0xFFFF;
        }
    }
    return JDR_OK;
}

/* ---- Clamp --------------------------------------------------------------- */

static int clip(int v)
{
    if (v < -128) return -128;
    if (v > 127) return 127;
    return v;
}

/* ---- IDCT ---------------------------------------------------------------- */

static void idct(int *block)
{
    int i, x0, x1, x2, x3, x4, x5, x6, x7;
    int *p = block;
    int tmp[64];
    int *tp = tmp;

    for (i = 0; i < 8; i++) {
        if (p[1] == 0 && p[2] == 0 && p[3] == 0 && p[4] == 0 &&
            p[5] == 0 && p[6] == 0 && p[7] == 0) {
            tmp[0] = tmp[1] = tmp[2] = tmp[3] =
            tmp[4] = tmp[5] = tmp[6] = tmp[7] = p[0] << 3;
        } else {
            x0 = p[0] * W1 + 128;
            x1 = p[4] * W1;
            x2 = p[6];
            x3 = p[2];
            x4 = p[1];
            x5 = p[7];
            x6 = p[5];
            x7 = p[3];
            x0 += x1;
            x1 = W6 * (x2 + x3) + 4;
            x2 = (x1 - (W2 + W6) * x2) >> 3;
            x3 = (x1 + (W2 - W6) * x3) >> 3;
            x1 = x4 + x6;
            x4 -= x6;
            x6 = x5 + x7;
            x5 -= x7;
            x5 = (W3 * (x6 - x5) + 4) >> 3;
            x7 = (W3 * (x4 + x7) + 4) >> 3;
            x4 = (W7 * (x1 + x5) + 4) >> 3;
            x6 = (x1 - (W1 + W7) * x5) / W1;
            x1 = (x4 + (W3 - W7) * x6) / W3;
            x5 = (x4 - (W3 + W5) * x6) / W1;
            x4 = (W3 * (x1 - x5) + 4) >> 3;
            x1 = (x1 + x5 + 4) >> 3;
            tmp[0] = x0 + x7; tmp[7] = x0 - x7;
            tmp[1] = x2 + x4; tmp[6] = x2 - x4;
            tmp[2] = x3 + x1; tmp[5] = x3 - x1;
            tmp[3] = x2 - x4 + x3 - x1; tmp[4] = x2 - x4 - x3 + x1;
        }
        p += 8;
        tp += 8;
    }
    tp -= 64;

    for (i = 0; i < 64; i += 8) {
        x0 = tp[i];
        x1 = tp[i + 4];
        x2 = tp[i + 6];
        x3 = tp[i + 2];
        x4 = tp[i + 1];
        x5 = tp[i + 7];
        x6 = tp[i + 5];
        x7 = tp[i + 3];

        if (x1 == 0 && x2 == 0 && x3 == 0 && x4 == 0 &&
            x5 == 0 && x6 == 0 && x7 == 0) {
            int v = (x0 + 4) >> 3;
            if (v < -128) v = -128; else if (v > 127) v = 127;
            block[i] = block[i + 1] = block[i + 2] = block[i + 3] =
            block[i + 4] = block[i + 5] = block[i + 6] = block[i + 7] = v;
        } else {
            x0 = x0 * W1 + 8192;
            x1 = x1 * W1;
            x2 = x2 * W6;
            x3 = x3 * W3;
            x4 = x4 * W7;
            x5 = x5 * W5;
            x6 = x6 * W3;
            x7 = x7 * W1;
            x6 = (x2 + x6) + 4;
            x7 = (x3 + x7) + 4;
            x2 = (x2 - (W2 + W6) * x2 / W6) + 4;
            x3 = (x3 + (W2 - W6) * x3 / W3) + 4;
            x2 >>= 3; x3 >>= 3; x6 >>= 3; x7 >>= 3;
            x1 = x4 + x6;
            x4 -= x6;
            x6 = x5 + x7;
            x5 -= x7;
            x5 = (W3 * (x6 - x5) + 4) >> 3;
            x7 = (W3 * (x4 + x7) + 4) >> 3;
            x4 = (W7 * (x1 + x5) + 4) >> 3;
            x6 = (x1 - (W1 + W7) * x5) / W1;
            x1 = (x4 + (W3 - W7) * x6) / W3;
            x5 = (x4 - (W3 + W5) * x6) / W1;
            x4 = (W3 * (x1 - x5) + 4) >> 3;
            x1 = (x1 + x5 + 4) >> 3;
            block[i]   = clip((x0 + x7) >> 11);
            block[i+7] = clip((x0 - x7) >> 11);
            block[i+1] = clip((x2 + x4) >> 11);
            block[i+6] = clip((x2 - x4) >> 11);
            block[i+2] = clip((x3 + x1) >> 11);
            block[i+5] = clip((x3 - x1) >> 11);
            block[i+3] = clip((x2 - x4 + x3 - x1) >> 11);
            block[i+4] = clip((x2 - x4 - x3 + x1) >> 11);
        }
    }
}

/* ---- Decode one data unit (8x8 block) ----------------------------------- */

static int decode_block(JDEC *jd, int comp, int *out_block)
{
    int *blk = out_block;
    int dc_diff, coef, n;
    int huff_dc, huff_ac;
    long *qttbl;
    qttbl = jd->qttbl[jd->qtid[comp]];

    /* DC coefficient */
    huff_dc = huffext(jd, jd->huffbits[0][jd->huff_sel[comp][0]], jd->huffcode[0][jd->huff_sel[comp][0]], jd->huffdata[0][jd->huff_sel[comp][0]]);
    if (huff_dc < 0) return JDR_FMT1;
    dc_diff = huffval(jd, huff_dc);
    if (dc_diff == -32768) return JDR_FMT1;
    jd->dcv[comp] += (short)dc_diff;
    blk[0] = jd->dcv[comp];

    /* AC coefficients */
    n = 1;
    while (n < 64) {
        huff_ac = huffext(jd, jd->huffbits[1][jd->huff_sel[comp][1]], jd->huffcode[1][jd->huff_sel[comp][1]], jd->huffdata[1][jd->huff_sel[comp][1]]);
        if (huff_ac < 0) return JDR_FMT1;
        if (huff_ac == 0) break; /* EOB */
        n += (huff_ac >> 4);     /* Zero run */
        if (n >= 64) break;
        coef = huffval(jd, huff_ac & 15);
        if (coef == -32768) return JDR_FMT1;
        blk[Zig[n]] = coef;
        n++;
    }

    /* Dequantize */
    for (n = 0; n < 64; n++) {
        blk[n] = (int)(blk[n] * qttbl[n]);
    }

    /* IDCT */
    idct(blk);

    return JDR_OK;
}

/* ---- jd_prepare: analyze JPEG header ------------------------------------ */

JRESULT jd_prepare(
    JDEC *jd,
    unsigned int (*infunc)(JDEC *, unsigned char *, unsigned int),
    void *pool,
    unsigned int sz_pool,
    void *dev
)
{
    unsigned char *dp;
    unsigned int marker, len, i;
    int rc;

    if (sz_pool < 3584) return JDR_MEM1;

    jd->infunc = infunc;
    jd->device = dev;
    jd->pool = pool;
    jd->pool_used = 0;
    jd->dptr = (unsigned char *)pool;
    jd->dctr = 0;
    jd->dmsk = 0;
    jd->nrst = 0;
    jd->width = 0;
    jd->height = 0;
    jd->nc = 0;

    for (i = 0; i < 3; i++) jd->dcv[i] = 0;

    /* Fetch first 2 bytes (SOI) so next read starts at first marker */
    jd->inbuf = infunc(jd, jd->dptr, 2);
    if (jd->inbuf < 2) return JDR_INP;

    if (jd->dptr[0] != 0xFF || jd->dptr[1] != 0xD8) return JDR_FMT1;
    jd->dptr += 2;

    /* Search for SOF0 marker */
    while (1) {
        unsigned int b = infunc(jd, jd->dptr, 4);
        if (b < 4) return JDR_INP;
        dp = jd->dptr;

        if (dp[0] != 0xFF) return JDR_FMT1;

        /* Skip fill bytes */
        while (dp[0] == 0xFF) {
            dp++;
            b = infunc(jd, dp, 1);
            if (b < 1) return JDR_INP;
        }
        marker = dp[0];
        dp++;
        b = infunc(jd, dp, 2);
        if (b < 2) return JDR_INP;
        len = ((unsigned int)dp[0] << 8) | dp[1];
        dp += 2;

        switch (marker) {
        case 0xC0: /* SOF0 - Start of Frame */
                            b = infunc(jd, dp, len);
            if (b < len) return JDR_INP;
            if (dp[0] != 8) return JDR_FMT1; /* Precision must be 8 */
            jd->height = ((unsigned int)dp[1] << 8) | dp[2];
            jd->width  = ((unsigned int)dp[3] << 8) | dp[4];
            jd->nc = dp[5]; /* Number of components */
            if (jd->nc != 1 && jd->nc != 3) return JDR_FMT3;
            {
                unsigned char *cp = dp + 6;
                for (i = 0; i < (unsigned int)jd->nc; i++) {
                    jd->comp_id[i] = cp[0];
                    jd->samp_factor[i] = cp[1];
                    jd->qtid[i] = cp[2];
                    cp += 3;
                }
            }
            break;

        case 0xC4: { /* DHT - Define Huffman Table */
            unsigned int tblen = len;
            unsigned char *tp = dp;
            while (tblen > 16) {
                b = infunc(jd, tp, tp[0] > 16 ? 17 : tp[0]);
                if (b < (unsigned int)(tp[0] > 16 ? 17 : tp[0])) return JDR_INP;
                unsigned int seglen = 17 + tp[0];
                rc = build_huffman(jd, tp, seglen, 0);
                if (rc) return (JRESULT)rc;
                tp += seglen;
                tblen -= seglen;
            }
            break;
        }

        case 0xDB: /* DQT - Define Quantization Table */
                      b = infunc(jd, dp, len);
            if (b < len) return JDR_INP;
            {
                unsigned int tblen = len;
                unsigned char *qp = dp;
                while (tblen > 0) {
                    unsigned char prec = qp[0] >> 4;
                    unsigned char tbl = qp[0] & 15;
                    if (tbl > 3) return JDR_FMT1;
                    unsigned int qlen = prec ? 130 : 66;
                    if (tblen < qlen) return JDR_FMT1;
                    jd->qttbl[tbl] = g_qttbl[tbl];
                    for (i = 0; i < 64; i++) {
                        ((long *)jd->qttbl[tbl])[Zig[i]] = (long)qp[1 + i];
                    }
                    qp += qlen;
                    tblen -= qlen;
                }
            }
            break;

        case 0xDD: /* DRI - Define Restart Interval */
                    b = infunc(jd, dp, 2);
            if (b < 2) return JDR_INP;
            jd->nrst = ((unsigned int)dp[0] << 8) | dp[1];
            break;

        case 0xDA: /* SOS - Start of Scan */
            b = infunc(jd, dp, len);
            if (b < len) return JDR_INP;
            jd->ns = dp[0]; /* Number of components in scan */
            if ((unsigned int)jd->ns != jd->nc) return JDR_FMT1;
            {
                unsigned char *sp = dp + 1;
                for (i = 0; i < (unsigned int)jd->ns; i++) {
                    unsigned char cid = sp[0];
                    unsigned int ci;
                    for (ci = 0; ci < (unsigned int)jd->nc; ci++) {
                        if (jd->comp_id[ci] == cid) break;
                    }
                    if (ci >= jd->nc) return JDR_FMT1;
                    jd->huff_sel[ci][0] = sp[1] & 15; /* DC table */
                    jd->huff_sel[ci][1] = (sp[1] >> 4) & 15; /* AC table */
                    sp += 2;
                }
            }
            /* Skip spectral selection / approx bytes */
            /* Data follows immediately after SOS header */
            jd->dmsk = 0;
            jd->dctr = 0;
            jd->inbuf = 0;
            jd->rst_cnt = 0;
            return JDR_OK;

        case 0xD9: /* EOI */
            return JDR_FMT1;

        default: /* Skip other segments */
            if (len > 2) {
                /* Skip by reading and discarding */
                while (len > 0) {
                    unsigned int n = (len > 256) ? 256 : len;
                    jd->inbuf = infunc(jd, jd->dptr, n);
                    if (jd->inbuf < n) return JDR_INP;
                    len -= n;
                }
            }
            break;
        }
    }
}

/* ---- jd_decomp: decompress image data ----------------------------------- */

JRESULT jd_decomp(
    JDEC *jd,
    int (*outfunc)(JDEC *, void *, JRECT *),
    unsigned char scale
)
{
    JRECT rect;
    unsigned int xs[3], ys[3];
    unsigned int mcu_w, mcu_h, mx, my, mcu_per_line;
    unsigned int i;

    (void)scale;

    /* Determine sampling factors */
    xs[0] = (jd->samp_factor[0] >> 4) & 15;
    ys[0] = (jd->samp_factor[0] >> 0) & 15;
    if (jd->nc == 3) {
        xs[1] = (jd->samp_factor[1] >> 4) & 15;
        ys[1] = (jd->samp_factor[1] >> 0) & 15;
        xs[2] = (jd->samp_factor[2] >> 4) & 15;
        ys[2] = (jd->samp_factor[2] >> 0) & 15;
    }

    mcu_w = 8 * xs[0];
    mcu_h = 8 * ys[0];
    mcu_per_line = (jd->width + mcu_w - 1) / mcu_w;

    /* Pre-compute block buffer layout: each component needs its space */
    jd->block_w = xs[0];
    jd->block_h = ys[0];

    for (my = 0; my < (unsigned int)(((jd->height + mcu_h - 1) / mcu_h)); my++) {
        for (mx = 0; mx < mcu_per_line; mx++) {
            unsigned char pixels[16 * 16 * 3];
            unsigned int px, py;
            unsigned int blk_hor[3], blk_ver[3];
            unsigned int ci;

            /* Compute block counts for each component in this MCU */
            for (ci = 0; ci < (unsigned int)jd->nc; ci++) {
                blk_hor[ci] = (ci == 0) ? xs[0] : xs[ci];
                blk_ver[ci] = (ci == 0) ? ys[0] : ys[ci];
            }

            /* Decode blocks. For grayscale: 1 block. For color:
             * First Y blocks, then Cb blocks, then Cr blocks.
             * Within each component, blocks are in raster order:
             * left-to-right, top-to-bottom. */
            if (jd->nc == 1) {
                /* Grayscale: decode 1 block */
                int blk[64];
                int rc = decode_block(jd, 0, blk);
                if (rc) return (JRESULT)rc;

                /* Convert single Y block to RGB pixel data */
                unsigned char *dst = pixels;
                for (py = 0; py < 8 && (my * 8 + py) < jd->height; py++) {
                    for (px = 0; px < 8 && (mx * 8 + px) < jd->width; px++) {
                        int v = blk[py * 8 + px];
                        if (v < 0) v = 0; else if (v > 255) v = 255;
                        dst[0] = dst[1] = dst[2] = (unsigned char)v;
                        dst += 3;
                    }
                }

                rect.left = mx * 8;
                rect.right = rect.left + 7;
                if (rect.right >= (int)jd->width) rect.right = (int)jd->width - 1;
                rect.top = my * 8;
                rect.bottom = rect.top + 7;
                if (rect.bottom >= (int)jd->height) rect.bottom = (int)jd->height - 1;

                if (outfunc(jd, pixels, &rect) != 1) return JDR_INTR;
            } else {
                /* Color YCbCr: decode Y blocks, then Cb, then Cr */
                int Y_blocks[4][64]; /* Up to 4 Y blocks for 4:2:0 */
                int Cb_block[64];
                int Cr_block[64];
                unsigned int nY = blk_hor[0] * blk_ver[0];

                /* Decode Y blocks */
                for (i = 0; i < nY; i++) {
                    int rc = decode_block(jd, 0, Y_blocks[i]);
                    if (rc) return (JRESULT)rc;
                }

                /* Decode Cb block(s) */
                for (i = 0; i < blk_hor[1] * blk_ver[1]; i++) {
                    int rc = decode_block(jd, 1, Cb_block);
                    if (rc) return (JRESULT)rc;
                }

                /* Decode Cr block(s) */
                for (i = 0; i < blk_hor[2] * blk_ver[2]; i++) {
                    int rc = decode_block(jd, 2, Cr_block);
                    if (rc) return (JRESULT)rc;
                }

                /* Assemble pixels: for each pixel in MCU, get Y from
                 * appropriate block, Cb/Cr from the subsampled blocks.
                 * Y has xs[0]×ys[0] blocks, Cb/Cr have xs[1]×ys[1] each.
                 * The MCU covers mcu_w × mcu_h pixels. */

                rect.left = mx * mcu_w;
                rect.right = rect.left + (int)mcu_w - 1;
                if (rect.right >= (int)jd->width) rect.right = (int)jd->width - 1;
                rect.top = my * mcu_h;
                rect.bottom = rect.top + (int)mcu_h - 1;
                if (rect.bottom >= (int)jd->height) rect.bottom = (int)jd->height - 1;

                unsigned char *dst = pixels;
                for (py = 0; py < (unsigned int)mcu_h; py++) {
                    unsigned int y_blk_row = py * blk_hor[0] / mcu_h;
                    unsigned int y_in_blk = py * 8 / mcu_h;
                    unsigned int cbcr_y = py * ys[1] / ys[0];

                    for (px = 0; px < (unsigned int)mcu_w; px++) {
                        unsigned int y_blk_col = px * blk_hor[0] / mcu_w;
                        unsigned int y_in_blk_x = px * 8 / mcu_w;
                        unsigned int cbcr_x = px * xs[1] / xs[0];

                        /* Get Y value */
                        unsigned int y_blk = y_blk_row * blk_hor[0] + y_blk_col;
                        int yy = Y_blocks[y_blk][y_in_blk * 8 + y_in_blk_x];

                        /* Get Cb value */
                        int cb_blk_x = cbcr_x * 8 / xs[1];
                        int cb_blk_y = cbcr_y * 8 / ys[1];
                        // Clamp to block bounds
                        if (cb_blk_x > 7) cb_blk_x = 7;
                        if (cb_blk_y > 7) cb_blk_y = 7;
                        int cb = Cb_block[cb_blk_y * 8 + cb_blk_x];

                        /* Get Cr value */
                        int cr = Cr_block[cb_blk_y * 8 + cb_blk_x];

                        /* YCbCr → RGB */
                        int r = yy                       + ((1436 * cr) >> 10) - 179;
                        int g = yy - (( 351 * cb) >> 10) - (( 731 * cr) >> 10) + 135;
                        int b = yy + ((1808 * cb) >> 10) - 226;

                        if (r < 0) r = 0; else if (r > 255) r = 255;
                        if (g < 0) g = 0; else if (g > 255) g = 255;
                        if (b < 0) b = 0; else if (b > 255) b = 255;

                        dst[0] = (unsigned char)r;
                        dst[1] = (unsigned char)g;
                        dst[2] = (unsigned char)b;
                        dst += 3;
                    }
                }

                if (outfunc(jd, pixels, &rect) != 1) return JDR_INTR;
            }

            /* Handle restart markers */
            if (jd->nrst && ++(jd->rst_cnt) >= jd->nrst) {
                jd->rst_cnt = 0;
                for (i = 0; i < 3; i++) jd->dcv[i] = 0;

                /* Discard padding bits to byte boundary */
                jd->dmsk = 0;

                /* Check for and skip RST marker */
                unsigned int b = jd->infunc(jd, (unsigned char *)jd->pool, 1);
                if (b == 1) {
                    unsigned char *tmp = (unsigned char *)jd->pool;
                    if (tmp[0] == 0xFF) {
                        b = jd->infunc(jd, tmp, 1);
                        if (b == 1) {
                            /* RST marker found, continue */
                        }
                    }
                }
            }
        }
    }

    return JDR_OK;
}
