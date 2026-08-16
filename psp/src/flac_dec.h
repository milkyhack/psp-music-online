#ifndef FLAC_DEC_H
#define FLAC_DEC_H

#include "ringbuf.h"

typedef struct FlacDec FlacDec;

typedef struct {
    unsigned sample_rate;
    unsigned channels;
    unsigned bits_per_sample;
    unsigned long long total_samples;
    int md5_present;
    unsigned char md5[16];
    char title[96];
    char artist[96];
    char album[96];
    char album_artist[96];
    char genre[48];
    int track_num;
    int disc_num;
    int year;
} FlacInfo;

FlacDec *flac_dec_create(void);
void flac_dec_destroy(FlacDec *d);

/* Embedded PICTURE (JPEG/PNG) in RAM — never written back to the FLAC. */
const unsigned char *flac_dec_picture(const FlacDec *d, int *len_out, int *is_jpeg_out);
void flac_dec_clear_picture(FlacDec *d);
/* Metadata-only cover extract from a local FLAC path. Caller frees *out. */
int flac_extract_cover(
    const char *path,
    unsigned char **out,
    int *out_len,
    int *is_jpeg
);

/* Source: ring (streaming) or file path (offline). Exactly one. */
int flac_dec_open_ring(FlacDec *d, RingBuf *rb);
int flac_dec_open_file(FlacDec *d, const char *path);

void flac_dec_close(FlacDec *d);

const FlacInfo *flac_dec_info(const FlacDec *d);

/*
 * Decode next block into interleaved s32 samples (or s16 if requested).
 * out_frames = frames written (per channel count).
 * Returns 1 = ok, 0 = need more / EOF, -1 = error.
 */
int flac_dec_read_s16(FlacDec *d, short *out, int max_frames, int *out_frames);

int flac_dec_eof(const FlacDec *d);
int flac_dec_error(const FlacDec *d);
const char *flac_dec_error_str(const FlacDec *d);

/* Verify complete file: STREAMINFO + optional MD5. Returns 0 on OK. */
int flac_verify_file(const char *path, char *err, int err_sz);

/* PSP-safe: 44100/48000 @ 16/24, 1–2 ch. 96000 may be rejected after probe. */
int flac_format_supported(const FlacInfo *info, char *err, int err_sz);

#endif
