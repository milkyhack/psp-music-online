#ifndef METRICS_H
#define METRICS_H

typedef enum {
    BUF_EMPTY = 0,
    BUF_FILLING,
    BUF_READY,
    BUF_PLAYING,
    BUF_LOW,
    BUF_CRITICAL,
    BUF_REBUFFERING,
    BUF_NETWORK_LOST
} BufferState;

typedef struct {
    unsigned ramBufferSize;
    unsigned ramBufferUsed;
    float bufferedSeconds;
    float bufferUsedPct;
    unsigned long long networkBytesReceived;
    float networkRateMbps;
    float networkAvgMbps;
    float networkLatencyMs;
    float networkJitterMs;
    float flacDecodeRateMbps;
    float audioConsumptionMbps;
    unsigned bufferUnderruns;
    unsigned rebufferCount;
    BufferState bufferState;
    int formatLossless;
    int sampleRate;
    int bitDepth;
    int channels;
    char formatName[16];
} AudioMetrics;

void metrics_init(void);
AudioMetrics *metrics_get(void);
void metrics_reset_session(void);

void metrics_set_buffer(unsigned size, unsigned used, float seconds);
void metrics_set_format(const char *name, int lossless, int rate, int depth, int ch);
void metrics_set_state(BufferState st);
void metrics_add_network_bytes(unsigned n, float instant_mbps);
void metrics_set_latency(float ms, float jitter_ms);
void metrics_set_decode_rate(float mbps);
void metrics_set_consumption(float mbps);
void metrics_note_underrun(void);
void metrics_note_rebuffer(void);

/* Headroom check: network should exceed consumption. */
int metrics_network_ok(float headroom);

void metrics_format_diag(char *out, int out_sz);

#endif
