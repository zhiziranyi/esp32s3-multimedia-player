/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor include file
/----------------------------------------------------------------------------*/

#ifndef _TJPGDEC_H
#define _TJPGDEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "integer.h"

/* Error code */
typedef enum {
    JDR_OK = 0,
    JDR_INTR,
    JDR_INP,
    JDR_MEM1,
    JDR_MEM2,
    JDR_PAR,
    JDR_FMT1,
    JDR_FMT2,
    JDR_FMT3
} JRESULT;

/* Rectangular structure */
typedef struct {
    int left, right, top, bottom;
} JRECT;

/* Decompressor object structure */
typedef struct JDEC JDEC;
struct JDEC {
    /* Input stream state */
    unsigned int dctr;
    unsigned char *dptr;
    unsigned int inbuf;
    unsigned char dmsk;

    unsigned char scale;

    /* Image info */
    unsigned int width, height;
    unsigned char nc;             /* Number of components (1 or 3) */
    unsigned char ns;             /* Number of components in scan */
    unsigned char comp_id[3];     /* Component identifiers */
    unsigned char samp_factor[3]; /* Sampling factors [H4:V4] */
    unsigned char qtid[4];        /* Quantization table ID per component */
    unsigned char huff_sel[3][2]; /* Huffman table selection [comp][DC/AC] */

    short dcv[3];                 /* DC coefficient predictor */

    /* Huffman tables */
    unsigned char *huffbits[2][2];
    unsigned short *huffcode[2][2];
    unsigned char *huffdata[2][2];

    /* Quantization tables (pointers into pool) */
    long *qttbl[4];

    /* Restart interval */
    unsigned short nrst;
    unsigned int rst_cnt;

    /* Block buffer pointer (into pool) */
    int *block;
    unsigned int block_w, block_h;

    /* Callbacks */
    unsigned int (*infunc)(JDEC *, unsigned char *, unsigned int);
    int (*outfunc)(JDEC *, void *, JRECT *);

    void *device;
    void *pool;
    unsigned int pool_used;       /* Bytes allocated from pool */
};

/* User interface */
JRESULT jd_prepare(JDEC *, unsigned int (*)(JDEC *, unsigned char *, unsigned int),
                   void *, unsigned int, void *);
JRESULT jd_decomp(JDEC *, int (*)(JDEC *, void *, JRECT *), unsigned char);

#ifdef __cplusplus
}
#endif

#endif /* _TJPGDEC_H */
