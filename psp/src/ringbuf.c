#include "ringbuf.h"

#include <pspkernel.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#define RING_MIN (512 * 1024)
#define RING_MAX (2 * 1024 * 1024)
#define RING_DEFAULT (768 * 1024)

struct RingBuf {
    unsigned char *data;
    size_t cap;
    volatile size_t head; /* write */
    volatile size_t tail; /* read */
    volatile size_t used;
    volatile int eof;
    volatile int aborting;
    SceUID sem_data;  /* signaled when data available */
    SceUID sem_space; /* signaled when space available */
};

size_t ringbuf_recommend_size(int bitrate_kbps, int target_seconds) {
    size_t need;
    size_t free_mem;
    int sec = target_seconds;

    if (sec < 5) {
        sec = 5;
    }
    if (sec > 15) {
        sec = 15;
    }
    if (bitrate_kbps <= 0) {
        bitrate_kbps = 900; /* typical FLAC 16/44.1 stereo */
    }

    need = (size_t)((bitrate_kbps * 1000 / 8) * sec);
    if (need < RING_MIN) {
        need = RING_MIN;
    }
    if (need > RING_MAX) {
        need = RING_MAX;
    }

    free_mem = (size_t)sceKernelMaxFreeMemSize();
    /* Leave headroom for UI, net PRX, decoder (~2.5 MB). */
    if (free_mem > 3 * 1024 * 1024) {
        size_t budget = free_mem - 3 * 1024 * 1024;
        if (need > budget) {
            need = budget;
        }
    } else if (free_mem > RING_MIN) {
        need = RING_MIN;
    } else {
        need = RING_DEFAULT;
        if (need > free_mem / 2 && free_mem > 256 * 1024) {
            need = free_mem / 2;
        }
    }

    if (need < RING_MIN && free_mem >= RING_MIN) {
        need = RING_MIN;
    }
    if (need > RING_MAX) {
        need = RING_MAX;
    }
    /* Align to 64 for cache. */
    need = (need + 63) & ~(size_t)63;
    return need ? need : RING_DEFAULT;
}

RingBuf *ringbuf_create(size_t capacity) {
    RingBuf *rb;
    if (capacity < 4096) {
        capacity = RING_DEFAULT;
    }
    rb = (RingBuf *)calloc(1, sizeof(RingBuf));
    if (!rb) {
        return NULL;
    }
    rb->data = (unsigned char *)memalign(64, capacity);
    if (!rb->data) {
        free(rb);
        return NULL;
    }
    rb->cap = capacity;
    rb->sem_data = sceKernelCreateSema("rb_data", 0, 0, 1, NULL);
    rb->sem_space = sceKernelCreateSema("rb_space", 0, 1, 1, NULL);
    if (rb->sem_data < 0 || rb->sem_space < 0) {
        ringbuf_destroy(rb);
        return NULL;
    }
    return rb;
}

void ringbuf_destroy(RingBuf *rb) {
    if (!rb) {
        return;
    }
    if (rb->sem_data >= 0) {
        sceKernelDeleteSema(rb->sem_data);
    }
    if (rb->sem_space >= 0) {
        sceKernelDeleteSema(rb->sem_space);
    }
    free(rb->data);
    free(rb);
}

void ringbuf_reset(RingBuf *rb) {
    if (!rb) {
        return;
    }
    rb->head = 0;
    rb->tail = 0;
    rb->used = 0;
    rb->eof = 0;
    rb->aborting = 0;
}

size_t ringbuf_capacity(const RingBuf *rb) {
    return rb ? rb->cap : 0;
}

size_t ringbuf_used(const RingBuf *rb) {
    return rb ? rb->used : 0;
}

size_t ringbuf_free(const RingBuf *rb) {
    if (!rb) {
        return 0;
    }
    return rb->cap - rb->used;
}

static void ring_signal(SceUID sem) {
    if (sem >= 0) {
        sceKernelSignalSema(sem, 1);
    }
}

size_t ringbuf_write(RingBuf *rb, const void *data, size_t len) {
    size_t space;
    size_t first;
    const unsigned char *src = (const unsigned char *)data;

    if (!rb || !data || len == 0 || rb->aborting) {
        return 0;
    }
    space = rb->cap - rb->used;
    if (space == 0) {
        return 0;
    }
    if (len > space) {
        len = space;
    }

    first = rb->cap - rb->head;
    if (first > len) {
        first = len;
    }
    memcpy(rb->data + rb->head, src, first);
    if (len > first) {
        memcpy(rb->data, src + first, len - first);
    }
    rb->head = (rb->head + len) % rb->cap;
    rb->used += len;
    ring_signal(rb->sem_data);
    return len;
}

size_t ringbuf_read(RingBuf *rb, void *data, size_t len) {
    size_t avail;
    size_t first;
    unsigned char *dst = (unsigned char *)data;

    if (!rb || !data || len == 0) {
        return 0;
    }
    avail = rb->used;
    if (avail == 0) {
        return 0;
    }
    if (len > avail) {
        len = avail;
    }

    first = rb->cap - rb->tail;
    if (first > len) {
        first = len;
    }
    memcpy(dst, rb->data + rb->tail, first);
    if (len > first) {
        memcpy(dst + first, rb->data, len - first);
    }
    rb->tail = (rb->tail + len) % rb->cap;
    rb->used -= len;
    ring_signal(rb->sem_space);
    return len;
}

size_t ringbuf_peek(const RingBuf *rb, void *data, size_t want) {
    size_t avail;
    size_t first;
    size_t len;
    unsigned char *dst = (unsigned char *)data;
    size_t tail;

    if (!rb || !data || want == 0) {
        return 0;
    }
    avail = rb->used;
    if (avail == 0) {
        return 0;
    }
    len = want;
    if (len > avail) {
        len = avail;
    }
    tail = rb->tail;
    first = rb->cap - tail;
    if (first > len) {
        first = len;
    }
    memcpy(dst, rb->data + tail, first);
    if (len > first) {
        memcpy(dst + first, rb->data, len - first);
    }
    return len;
}

size_t ringbuf_write_wait(RingBuf *rb, const void *data, size_t len, unsigned timeout_us) {
    size_t total = 0;
    const unsigned char *src = (const unsigned char *)data;
    SceUInt to;

    if (!rb || !data || len == 0) {
        return 0;
    }
    while (total < len && !rb->aborting) {
        size_t n = ringbuf_write(rb, src + total, len - total);
        total += n;
        if (total >= len) {
            break;
        }
        if (rb->eof) {
            break;
        }
        to = timeout_us ? timeout_us : 50000;
        sceKernelWaitSema(rb->sem_space, 1, &to);
    }
    return total;
}

size_t ringbuf_read_wait(RingBuf *rb, void *data, size_t len, unsigned timeout_us) {
    size_t total = 0;
    unsigned char *dst = (unsigned char *)data;
    SceUInt to;

    if (!rb || !data || len == 0) {
        return 0;
    }
    while (total < len && !rb->aborting) {
        size_t n = ringbuf_read(rb, dst + total, len - total);
        total += n;
        if (total >= len) {
            break;
        }
        if (rb->eof && rb->used == 0) {
            break;
        }
        to = timeout_us ? timeout_us : 50000;
        sceKernelWaitSema(rb->sem_data, 1, &to);
    }
    return total;
}

void ringbuf_set_eof(RingBuf *rb, int eof) {
    if (!rb) {
        return;
    }
    rb->eof = eof ? 1 : 0;
    ring_signal(rb->sem_data);
}

int ringbuf_eof(const RingBuf *rb) {
    return rb ? rb->eof : 1;
}

void ringbuf_set_abort(RingBuf *rb, int aborting) {
    if (!rb) {
        return;
    }
    rb->aborting = aborting ? 1 : 0;
    ring_signal(rb->sem_data);
    ring_signal(rb->sem_space);
}

float ringbuf_buffered_seconds(const RingBuf *rb, int bitrate_kbps) {
    if (!rb || bitrate_kbps <= 0) {
        return 0.0f;
    }
    return (float)(rb->used * 8.0) / (float)(bitrate_kbps * 1000.0);
}
