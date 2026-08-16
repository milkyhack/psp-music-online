#include "flac_dec.h"

#include <FLAC/stream_decoder.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FlacDec {
    FLAC__StreamDecoder *dec;
    RingBuf *rb;
    int fd;
    FlacInfo info;
    int have_info;
    int eof;
    int err;
    char err_str[64];
    /* Pending PCM from write callback */
    short *pcm;
    int pcm_cap_frames;
    int pcm_frames;
    int pcm_channels;
    /* Embedded cover (RAM only) */
    unsigned char *picture;
    int picture_len;
    int picture_jpeg;
};

static void set_err(FlacDec *d, const char *s) {
    d->err = 1;
    if (s) {
        strncpy(d->err_str, s, sizeof(d->err_str) - 1);
        d->err_str[sizeof(d->err_str) - 1] = '\0';
    }
}

static FLAC__StreamDecoderReadStatus read_cb(
    const FLAC__StreamDecoder *decoder,
    FLAC__byte buffer[],
    size_t *bytes,
    void *client_data
) {
    FlacDec *d = (FlacDec *)client_data;
    size_t want = *bytes;
    size_t n = 0;
    (void)decoder;

    if (d->rb) {
        n = ringbuf_read_wait(d->rb, buffer, want, 100000);
        if (n == 0) {
            if (ringbuf_eof(d->rb) && ringbuf_used(d->rb) == 0) {
                *bytes = 0;
                return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
            }
            *bytes = 0;
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
        }
        *bytes = n;
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }

    if (d->fd >= 0) {
        int r = sceIoRead(d->fd, buffer, (unsigned)want);
        if (r < 0) {
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        }
        if (r == 0) {
            *bytes = 0;
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        }
        *bytes = (size_t)r;
        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

static FLAC__StreamDecoderWriteStatus write_cb(
    const FLAC__StreamDecoder *decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *client_data
) {
    FlacDec *d = (FlacDec *)client_data;
    unsigned ch = frame->header.channels;
    unsigned frames = frame->header.blocksize;
    unsigned bps = frame->header.bits_per_sample;
    unsigned i, c;
    int need;
    (void)decoder;

    if (ch == 0 || frames == 0) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }
    need = (int)frames;
    if (need > d->pcm_cap_frames) {
        short *np = (short *)realloc(d->pcm, (size_t)need * ch * sizeof(short));
        if (!np) {
            set_err(d, "oom");
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
        d->pcm = np;
        d->pcm_cap_frames = need;
    }
    d->pcm_channels = (int)ch;
    d->pcm_frames = (int)frames;

    for (i = 0; i < frames; i++) {
        for (c = 0; c < ch; c++) {
            FLAC__int32 s = buffer[c][i];
            if (bps > 16) {
                s >>= (bps - 16);
            } else if (bps < 16) {
                s <<= (16 - bps);
            }
            if (s > 32767) {
                s = 32767;
            }
            if (s < -32768) {
                s = -32768;
            }
            d->pcm[i * ch + c] = (short)s;
        }
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_cb(
    const FLAC__StreamDecoder *decoder,
    const FLAC__StreamMetadata *metadata,
    void *client_data
) {
    FlacDec *d = (FlacDec *)client_data;
    (void)decoder;
    if (!metadata) {
        return;
    }
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        const FLAC__StreamMetadata_StreamInfo *si = &metadata->data.stream_info;
        d->info.sample_rate = si->sample_rate;
        d->info.channels = si->channels;
        d->info.bits_per_sample = si->bits_per_sample;
        d->info.total_samples = si->total_samples;
        d->info.md5_present = 0;
        {
            int i;
            for (i = 0; i < 16; i++) {
                if (si->md5sum[i]) {
                    d->info.md5_present = 1;
                    break;
                }
            }
            memcpy(d->info.md5, si->md5sum, 16);
        }
        d->have_info = 1;
    } else if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
        const FLAC__StreamMetadata_VorbisComment *vc = &metadata->data.vorbis_comment;
        FLAC__uint32 i;
        for (i = 0; i < vc->num_comments; i++) {
            const char *entry = (const char *)vc->comments[i].entry;
            unsigned len = vc->comments[i].length;
            char key[32];
            const char *eq;
            unsigned klen;
            if (!entry || len == 0) {
                continue;
            }
            eq = (const char *)memchr(entry, '=', len);
            if (!eq) {
                continue;
            }
            klen = (unsigned)(eq - entry);
            if (klen >= sizeof(key)) {
                klen = sizeof(key) - 1;
            }
            memcpy(key, entry, klen);
            key[klen] = '\0';
            {
                unsigned j;
                for (j = 0; j < klen; j++) {
                    if (key[j] >= 'a' && key[j] <= 'z') {
                        key[j] = (char)(key[j] - 'a' + 'A');
                    }
                }
            }
            eq++;
            {
                char val[96];
                unsigned vlen = len - (unsigned)(eq - entry);
                if (vlen >= sizeof(val)) {
                    vlen = sizeof(val) - 1;
                }
                memcpy(val, eq, vlen);
                val[vlen] = '\0';
                if (strcmp(key, "TITLE") == 0) {
                    strncpy(d->info.title, val, sizeof(d->info.title) - 1);
                } else if (strcmp(key, "ARTIST") == 0) {
                    strncpy(d->info.artist, val, sizeof(d->info.artist) - 1);
                } else if (strcmp(key, "ALBUM") == 0) {
                    strncpy(d->info.album, val, sizeof(d->info.album) - 1);
                } else if (strcmp(key, "ALBUMARTIST") == 0) {
                    strncpy(d->info.album_artist, val, sizeof(d->info.album_artist) - 1);
                } else if (strcmp(key, "GENRE") == 0) {
                    strncpy(d->info.genre, val, sizeof(d->info.genre) - 1);
                } else if (strcmp(key, "TRACKNUMBER") == 0) {
                    d->info.track_num = atoi(val);
                } else if (strcmp(key, "DISCNUMBER") == 0) {
                    d->info.disc_num = atoi(val);
                } else if (strcmp(key, "DATE") == 0 || strcmp(key, "YEAR") == 0) {
                    d->info.year = atoi(val);
                }
            }
        }
    } else if (metadata->type == FLAC__METADATA_TYPE_PICTURE && !d->picture) {
        const FLAC__StreamMetadata_Picture *pic = &metadata->data.picture;
        const char *mime = pic->mime_type ? (const char *)pic->mime_type : "";
        int is_jpeg = 0;
        int is_png = 0;
        if (strstr(mime, "jpeg") || strstr(mime, "jpg")) {
            is_jpeg = 1;
        } else if (strstr(mime, "png")) {
            is_png = 1;
        } else if (pic->data_length >= 3 &&
                   pic->data[0] == 0xFF && pic->data[1] == 0xD8) {
            is_jpeg = 1;
        } else if (pic->data_length >= 8 && memcmp(pic->data, "\x89PNG\r\n\x1a\n", 8) == 0) {
            is_png = 1;
        }
        if ((is_jpeg || is_png) && pic->data && pic->data_length > 0 &&
            pic->data_length <= 512 * 1024) {
            unsigned char *buf = (unsigned char *)malloc(pic->data_length);
            if (buf) {
                memcpy(buf, pic->data, pic->data_length);
                d->picture = buf;
                d->picture_len = (int)pic->data_length;
                d->picture_jpeg = is_jpeg;
            }
        }
    }
}

static void error_cb(
    const FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderErrorStatus status,
    void *client_data
) {
    FlacDec *d = (FlacDec *)client_data;
    (void)decoder;
    set_err(d, FLAC__StreamDecoderErrorStatusString[status]);
}

FlacDec *flac_dec_create(void) {
    FlacDec *d = (FlacDec *)calloc(1, sizeof(FlacDec));
    if (!d) {
        return NULL;
    }
    d->fd = -1;
    d->dec = FLAC__stream_decoder_new();
    if (!d->dec) {
        free(d);
        return NULL;
    }
    FLAC__stream_decoder_set_metadata_respond(d->dec, FLAC__METADATA_TYPE_VORBIS_COMMENT);
    FLAC__stream_decoder_set_metadata_respond(d->dec, FLAC__METADATA_TYPE_PICTURE);
    return d;
}

void flac_dec_destroy(FlacDec *d) {
    if (!d) {
        return;
    }
    flac_dec_close(d);
    flac_dec_clear_picture(d);
    if (d->dec) {
        FLAC__stream_decoder_delete(d->dec);
    }
    free(d->pcm);
    free(d);
}

static int init_callbacks(FlacDec *d) {
    FLAC__StreamDecoderInitStatus st;
    st = FLAC__stream_decoder_init_stream(
        d->dec,
        read_cb,
        NULL,
        NULL,
        NULL,
        NULL,
        write_cb,
        metadata_cb,
        error_cb,
        d
    );
    if (st != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        set_err(d, "init");
        return -1;
    }
    /* Process metadata until STREAMINFO (+ optional PICTURE/tags). */
    {
        int tries = 0;
        while (!d->err && tries < 128) {
            if (!FLAC__stream_decoder_process_single(d->dec)) {
                break;
            }
            tries++;
            /* Stop after we have STREAMINFO and either a picture or enough tries. */
            if (d->have_info && tries > 24) {
                break;
            }
        }
    }
    if (!d->have_info) {
        set_err(d, "no_streaminfo");
        return -1;
    }
    return 0;
}

const unsigned char *flac_dec_picture(const FlacDec *d, int *len_out, int *is_jpeg_out) {
    if (len_out) {
        *len_out = d ? d->picture_len : 0;
    }
    if (is_jpeg_out) {
        *is_jpeg_out = d ? d->picture_jpeg : 0;
    }
    return d && d->picture_len > 0 ? d->picture : NULL;
}

void flac_dec_clear_picture(FlacDec *d) {
    if (!d) {
        return;
    }
    free(d->picture);
    d->picture = NULL;
    d->picture_len = 0;
    d->picture_jpeg = 0;
}

int flac_extract_cover(
    const char *path,
    unsigned char **out,
    int *out_len,
    int *is_jpeg
) {
    FlacDec *d;
    const unsigned char *pic;
    int len = 0;
    int jpeg = 0;
    if (out) {
        *out = NULL;
    }
    if (out_len) {
        *out_len = 0;
    }
    if (is_jpeg) {
        *is_jpeg = 0;
    }
    d = flac_dec_create();
    if (!d) {
        return -1;
    }
    if (flac_dec_open_file(d, path) < 0) {
        flac_dec_destroy(d);
        return -1;
    }
    pic = flac_dec_picture(d, &len, &jpeg);
    if (!pic || len <= 0) {
        flac_dec_destroy(d);
        return -1;
    }
    if (out) {
        unsigned char *copy = (unsigned char *)malloc((size_t)len);
        if (!copy) {
            flac_dec_destroy(d);
            return -1;
        }
        memcpy(copy, pic, (size_t)len);
        *out = copy;
    }
    if (out_len) {
        *out_len = len;
    }
    if (is_jpeg) {
        *is_jpeg = jpeg;
    }
    flac_dec_destroy(d);
    return 0;
}

int flac_dec_open_ring(FlacDec *d, RingBuf *rb) {
    if (!d || !rb) {
        return -1;
    }
    flac_dec_close(d);
    flac_dec_clear_picture(d);
    memset(&d->info, 0, sizeof(d->info));
    d->rb = rb;
    d->fd = -1;
    d->err = 0;
    d->eof = 0;
    d->have_info = 0;
    d->err_str[0] = '\0';
    FLAC__stream_decoder_reset(d->dec);
    return init_callbacks(d);
}

int flac_dec_open_file(FlacDec *d, const char *path) {
    if (!d || !path) {
        return -1;
    }
    flac_dec_close(d);
    flac_dec_clear_picture(d);
    memset(&d->info, 0, sizeof(d->info));
    d->rb = NULL;
    d->fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (d->fd < 0) {
        set_err(d, "open");
        return -1;
    }
    d->err = 0;
    d->eof = 0;
    d->have_info = 0;
    d->err_str[0] = '\0';
    FLAC__stream_decoder_reset(d->dec);
    return init_callbacks(d);
}

void flac_dec_close(FlacDec *d) {
    if (!d) {
        return;
    }
    if (d->dec) {
        FLAC__stream_decoder_finish(d->dec);
    }
    if (d->fd >= 0) {
        sceIoClose(d->fd);
        d->fd = -1;
    }
    d->rb = NULL;
    d->pcm_frames = 0;
}

const FlacInfo *flac_dec_info(const FlacDec *d) {
    return d && d->have_info ? &d->info : NULL;
}

int flac_dec_read_s16(FlacDec *d, short *out, int max_frames, int *out_frames) {
    int ch;
    int copy;
    if (out_frames) {
        *out_frames = 0;
    }
    if (!d || !out || max_frames <= 0) {
        return -1;
    }
    if (d->err) {
        return -1;
    }

    if (d->pcm_frames <= 0) {
        FLAC__bool ok = FLAC__stream_decoder_process_single(d->dec);
        if (d->err) {
            return -1;
        }
        if (!ok || d->pcm_frames <= 0) {
            FLAC__StreamDecoderState st = FLAC__stream_decoder_get_state(d->dec);
            if (st == FLAC__STREAM_DECODER_END_OF_STREAM) {
                d->eof = 1;
                return 0;
            }
            if (st == FLAC__STREAM_DECODER_ABORTED) {
                set_err(d, "abort");
                return -1;
            }
            return 0;
        }
    }

    ch = d->pcm_channels > 0 ? d->pcm_channels : 2;
    copy = d->pcm_frames;
    if (copy > max_frames) {
        copy = max_frames;
    }
    memcpy(out, d->pcm, (size_t)copy * (size_t)ch * sizeof(short));
    if (copy < d->pcm_frames) {
        memmove(
            d->pcm,
            d->pcm + copy * ch,
            (size_t)(d->pcm_frames - copy) * (size_t)ch * sizeof(short)
        );
    }
    d->pcm_frames -= copy;
    if (out_frames) {
        *out_frames = copy;
    }
    return 1;
}

int flac_dec_eof(const FlacDec *d) {
    return d ? d->eof : 1;
}

int flac_dec_error(const FlacDec *d) {
    return d ? d->err : 1;
}

const char *flac_dec_error_str(const FlacDec *d) {
    return d && d->err_str[0] ? d->err_str : "error";
}

int flac_format_supported(const FlacInfo *info, char *err, int err_sz) {
    if (!info) {
        if (err && err_sz > 0) {
            snprintf(err, err_sz, "No STREAMINFO");
        }
        return 0;
    }
    if (info->channels < 1 || info->channels > 2) {
        if (err && err_sz > 0) {
            snprintf(err, err_sz, "Unsupported channels");
        }
        return 0;
    }
    if (info->bits_per_sample != 16 && info->bits_per_sample != 24) {
        if (err && err_sz > 0) {
            snprintf(err, err_sz, "Unsupported bit depth");
        }
        return 0;
    }
    if (info->sample_rate == 44100 || info->sample_rate == 48000) {
        return 1;
    }
    if (info->sample_rate == 96000) {
        /* Allowed to try; player may reject after underrun. */
        return 1;
    }
    if (err && err_sz > 0) {
        snprintf(err, err_sz, "Unsupported: %u Hz", info->sample_rate);
    }
    return 0;
}

int flac_verify_file(const char *path, char *err, int err_sz) {
    FlacDec *d;
    short buf[4096];
    int frames;
    int rc;

    if (err && err_sz > 0) {
        err[0] = '\0';
    }
    {
        int fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
        char magic[4];
        if (fd < 0) {
            if (err) {
                snprintf(err, err_sz, "Cannot open");
            }
            return -1;
        }
        if (sceIoRead(fd, magic, 4) != 4 || memcmp(magic, "fLaC", 4) != 0) {
            sceIoClose(fd);
            if (err) {
                snprintf(err, err_sz, "Not FLAC");
            }
            return -1;
        }
        sceIoClose(fd);
    }

    d = flac_dec_create();
    if (!d) {
        if (err) {
            snprintf(err, err_sz, "OOM");
        }
        return -1;
    }
    if (flac_dec_open_file(d, path) < 0) {
        if (err) {
            snprintf(err, err_sz, "%s", flac_dec_error_str(d));
        }
        flac_dec_destroy(d);
        return -1;
    }
    /* Decode through to validate (may be slow for long files — still correct). */
    for (;;) {
        rc = flac_dec_read_s16(d, buf, 2048, &frames);
        if (rc < 0) {
            if (err) {
                snprintf(err, err_sz, "FLAC ERROR");
            }
            flac_dec_destroy(d);
            return -1;
        }
        if (rc == 0 && flac_dec_eof(d)) {
            break;
        }
        if (rc == 0 && frames == 0) {
            /* starve — treat as error on file source */
            if (err) {
                snprintf(err, err_sz, "Incomplete FLAC");
            }
            flac_dec_destroy(d);
            return -1;
        }
    }
    flac_dec_destroy(d);
    return 0;
}
