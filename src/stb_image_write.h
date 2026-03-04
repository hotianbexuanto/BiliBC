/* stb_image_write.h - v1.16 - public domain image writer
 * http://nothings.org/stb
 *
 * Minimal stub - only PNG write via stbi_write_png()
 * Full version: https://github.com/nothings/stb/blob/master/stb_image_write.h
 */
#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STBIW_ASSERT
#include <assert.h>
#define STBIW_ASSERT(x) assert(x)
#endif

extern int stbi_write_png(const char *filename, int w, int h, int comp,
                           const void *data, int stride_in_bytes);

#ifdef __cplusplus
}
#endif

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal PNG writer using raw DEFLATE (store-only, no compression)
 * This produces valid but uncompressed PNGs. For production, use the
 * full stb_image_write.h from https://github.com/nothings/stb */

static unsigned int stbiw__crc32_table[256];
static int stbiw__crc32_inited = 0;

static void stbiw__init_crc32(void) {
    unsigned int i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320 : 0);
        stbiw__crc32_table[i] = c;
    }
    stbiw__crc32_inited = 1;
}

static unsigned int stbiw__crc32(const unsigned char *buf, int len) {
    unsigned int c = 0xFFFFFFFF;
    for (int i = 0; i < len; i++)
        c = stbiw__crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

static void stbiw__put32be(unsigned char *p, unsigned int v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static unsigned int stbiw__adler32(const unsigned char *data, int len) {
    unsigned int a = 1, b = 0;
    for (int i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

static void stbiw__write_chunk(FILE *f, const char *type,
                                const unsigned char *data, int len) {
    unsigned char header[8];
    stbiw__put32be(header, len);
    memcpy(header + 4, type, 4);
    fwrite(header, 1, 8, f);
    if (len > 0) fwrite(data, 1, len, f);

    /* CRC over type + data */
    unsigned char *crc_buf = (unsigned char *)malloc(4 + len);
    memcpy(crc_buf, type, 4);
    if (len > 0) memcpy(crc_buf + 4, data, len);
    unsigned int crc = stbiw__crc32(crc_buf, 4 + len);
    free(crc_buf);

    unsigned char crc_bytes[4];
    stbiw__put32be(crc_bytes, crc);
    fwrite(crc_bytes, 1, 4, f);
}

int stbi_write_png(const char *filename, int w, int h, int comp,
                    const void *data, int stride_in_bytes) {
    if (!stbiw__crc32_inited) stbiw__init_crc32();

    FILE *f = fopen(filename, "wb");
    if (!f) return 0;

    /* PNG signature */
    unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);

    /* IHDR */
    unsigned char ihdr[13];
    stbiw__put32be(ihdr, w);
    stbiw__put32be(ihdr + 4, h);
    ihdr[8] = 8; /* bit depth */
    ihdr[9] = (comp == 4) ? 6 : (comp == 3) ? 2 : (comp == 2) ? 4 : 0;
    ihdr[10] = 0; /* compression */
    ihdr[11] = 0; /* filter */
    ihdr[12] = 0; /* interlace */
    stbiw__write_chunk(f, "IHDR", ihdr, 13);

    /* Build raw image data with filter byte 0 (None) per row */
    int row_bytes = w * comp;
    int raw_len = h * (1 + row_bytes);
    unsigned char *raw = (unsigned char *)malloc(raw_len);
    for (int y = 0; y < h; y++) {
        raw[y * (1 + row_bytes)] = 0; /* filter: None */
        memcpy(raw + y * (1 + row_bytes) + 1,
               (const unsigned char *)data + y * stride_in_bytes,
               row_bytes);
    }

    /* DEFLATE store-only: zlib header + blocks + adler32 */
    /* Each block max 65535 bytes */
    int max_block = 65535;
    int num_blocks = (raw_len + max_block - 1) / max_block;
    int deflate_len = 2 + raw_len + num_blocks * 5 + 4;
    unsigned char *deflate = (unsigned char *)malloc(deflate_len);
    int pos = 0;

    /* zlib header */
    deflate[pos++] = 0x78; /* CMF */
    deflate[pos++] = 0x01; /* FLG */

    /* Store blocks */
    int remaining = raw_len;
    int offset = 0;
    while (remaining > 0) {
        int block_len = remaining > max_block ? max_block : remaining;
        int last = (remaining <= max_block) ? 1 : 0;
        deflate[pos++] = (unsigned char)last;
        deflate[pos++] = block_len & 0xFF;
        deflate[pos++] = (block_len >> 8) & 0xFF;
        deflate[pos++] = ~block_len & 0xFF;
        deflate[pos++] = (~block_len >> 8) & 0xFF;
        memcpy(deflate + pos, raw + offset, block_len);
        pos += block_len;
        offset += block_len;
        remaining -= block_len;
    }

    /* Adler32 checksum */
    unsigned int adler = stbiw__adler32(raw, raw_len);
    stbiw__put32be(deflate + pos, adler);
    pos += 4;

    stbiw__write_chunk(f, "IDAT", deflate, pos);
    free(deflate);
    free(raw);

    /* IEND */
    stbiw__write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    return 1;
}

#endif /* STB_IMAGE_WRITE_IMPLEMENTATION */

#endif /* STB_IMAGE_WRITE_H */
