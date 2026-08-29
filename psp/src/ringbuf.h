#ifndef RINGBUF_H
#define RINGBUF_H

#include <stddef.h>

typedef struct RingBuf RingBuf;

/* Adaptive size: clamp 512KB–2MB from free RAM / bitrate hints. */
size_t ringbuf_recommend_size(int bitrate_kbps, int target_seconds);

RingBuf *ringbuf_create(size_t capacity);
void ringbuf_destroy(RingBuf *rb);
void ringbuf_reset(RingBuf *rb);

size_t ringbuf_capacity(const RingBuf *rb);
size_t ringbuf_used(const RingBuf *rb);
size_t ringbuf_free(const RingBuf *rb);

/* Non-blocking: write/read as much as fits. Returns bytes transferred. */
size_t ringbuf_write(RingBuf *rb, const void *data, size_t len);
size_t ringbuf_read(RingBuf *rb, void *data, size_t len);

/* Blocking with timeout (us). Returns bytes or 0 on abort/timeout. */
size_t ringbuf_write_wait(RingBuf *rb, const void *data, size_t len, unsigned timeout_us);
size_t ringbuf_read_wait(RingBuf *rb, void *data, size_t len, unsigned timeout_us);

void ringbuf_set_eof(RingBuf *rb, int eof);
int ringbuf_eof(const RingBuf *rb);
void ringbuf_set_abort(RingBuf *rb, int aborting);

/* Buffered media seconds estimate from used bytes and bitrate (kbps). */
float ringbuf_buffered_seconds(const RingBuf *rb, int bitrate_kbps);

/* Peek without consuming. Returns bytes copied (may be < want). */
size_t ringbuf_peek(const RingBuf *rb, void *data, size_t want);

#endif
